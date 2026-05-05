#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "api.h"
#include "event.h"
#include "io.h"

#define UDP_PACKET_BUFFER_SIZE (1<<16)

static const char *const s_osc_socket_mt = "osc socket";
static const char *const s_osc_msg_mt = "osc message";

struct osc_socket {
  int                fd;
  int                cb;
  struct sockaddr_in saddr;
  char               buf[UDP_PACKET_BUFFER_SIZE];
};

static void set_osc_error(lua_State *state, const char *err)
{
  lua_pushstring(state, err);
  lua_setfield(state, -2, "err");
}

static void begin_osc_msg(lua_State *state)
{
  lua_newtable(state);
  luaL_setmetatable(state, s_osc_msg_mt);
  lua_pushnil(state);
  lua_setfield(state, -2, "err");
}

static void set_osc_addr(lua_State *state, const char *addr)
{
  lua_pushstring(state, addr);
  lua_setfield(state, -2, "addr");
}

static void begin_osc_args(lua_State *state)
{
  lua_newtable(state);
}

static void end_osc_args(lua_State *state)
{
  lua_setfield(state, -2, "args");
}

static void append_osc_arg(lua_State *state)
{
  int len = lua_rawlen(state, -2);
  lua_rawseti(state, -2, len+1);
}

static void append_osc_string_arg(lua_State *state, const char *str)
{
  lua_pushstring(state, str);
  append_osc_arg(state);
}

static void append_osc_integer_arg(lua_State *state, int i)
{
  lua_pushinteger(state, i);
  append_osc_arg(state);
}

static void append_osc_float_arg(lua_State *state, float f)
{
  lua_pushnumber(state, f);
  append_osc_arg(state);
}

static size_t osc_strlen(const char *data)
{
  size_t len = 1 + strlen(data);

  while (len % 4 != 0)
    len++;

  return len;
}

static int i32be(char *ptr)
{
  unsigned char b0, b1, b2, b3;

  b0 = *(unsigned char*) &ptr[0];
  b1 = *(unsigned char*) &ptr[1];
  b2 = *(unsigned char*) &ptr[2];
  b3 = *(unsigned char*) &ptr[3];

  return (b0<<24) | (b1<<16) | (b2<<8) | b3;
}

static void push_arg(lua_State *state, void *data, size_t size)
{
  void *end = data+size;

  char err[512] = {0};

  begin_osc_msg(state);

  const char *addr = data;
  data += osc_strlen(addr);
  set_osc_addr(state, addr);

  char tag[512];
  strcpy(tag, data);
  data += osc_strlen(tag);

  begin_osc_args(state);

  for (const char *ptr = tag; *ptr; ptr++) {

    char t = *ptr;
    switch (t) {
    case ',':
      continue;

    case 's':
      append_osc_string_arg(state, data);
      data += osc_strlen(data);
      continue;

    case 'i':
      append_osc_integer_arg(state, i32be(data));
      data += 4;
      break;

    case 'f':
      append_osc_float_arg(state, *(float*) data);
      data += 4;
      break;

    default:
      end_osc_args(state);
      snprintf(err, sizeof err, "invalid type tag %c", t);
      set_osc_error(state, err);
      goto done;
    }
  }

  end_osc_args(state);

done:
  assert(end >= data);
}

static void io_handler(int fd, void *data)
{
  struct osc_socket *s = data;

  ssize_t size = recvfrom(s->fd, s->buf, sizeof s->buf, 0, NULL, NULL);

  if (size < 0)
    /* TODO: error message */
    return;

  push_main_thread_event(s->cb, push_arg, s->buf, size);
}

static int api_create_osc_socket(lua_State *state)
{
  int port = luaL_checkinteger(state, 1);

  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  int cb = luaL_ref(state, LUA_REGISTRYINDEX);

  struct osc_socket *s = lua_newuserdata(state, sizeof(*s));
  luaL_setmetatable(state, s_osc_socket_mt);
  api_gc_protect(state, -1);

  s->fd = socket(AF_INET, SOCK_DGRAM, 0);
  s->cb = cb;
  memset(s->buf, 0, sizeof s->buf);

  if (s->fd < 0)
    luaL_error(state, "failed to create OSC socket: %s", strerror(errno));

  memset(&s->saddr, 0, sizeof(s->saddr));
  s->saddr.sin_family = AF_INET;
  s->saddr.sin_addr.s_addr = INADDR_ANY;
  s->saddr.sin_port = htons(port);

  if (bind(s->fd, &s->saddr, sizeof(s->saddr)) < 0) {
    close(s->fd);
    luaL_error(state, "failed to bind OSC socket: %s", strerror(errno));
  }

  io_add_input(s->fd, s, io_handler);

  return 1;
}

void api_define_osc_socket(lua_State *state)
{
  {
    const luaL_Reg methods[] = {
      { NULL,        NULL, },
    };

    const luaL_Reg funcs[] = {
      { "init", api_create_osc_socket, },
      { NULL,   NULL, },
    };

    lua_newtable(state);
    lua_newtable(state);
    for (const luaL_Reg *reg = funcs; reg->name; reg++) {
      lua_pushcfunction(state, reg->func);
      lua_setfield(state, -2, reg->name);
    }
    lua_setfield(state, -2, "Socket");
    lua_setglobal(state, "OSC");

    luaL_newmetatable(state, s_osc_socket_mt);
    lua_newtable(state);
    luaL_setfuncs(state, methods, 0);
    lua_setfield(state, -2, "__index");
    lua_pop(state, 1);
  }

  {
    luaL_newmetatable(state, s_osc_msg_mt);
    lua_newtable(state);
    lua_setfield(state, -2, "__index");
    lua_pop(state, 1);
  }
}
