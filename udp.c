#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <lua.h>
#include <lauxlib.h>

#include "io.h"
#include "api.h"
#include "event.h"

#define UDP_PACKET_BUFFER_SIZE (1<<16)

const char *const s_udp_socket_mt = "udp socket";

struct udp_socket {
  int                fd;
  int                cb;
  struct sockaddr_in sockaddr;
};

static void push_arg(lua_State *state, void *data, size_t size)
{
  lua_pushlstring(state, data, size);
}

static void io_handler(int fd, void *data)
{
  struct udp_socket *us = data;

  void *buf = malloc(UDP_PACKET_BUFFER_SIZE);
  ssize_t size = recvfrom(fd,buf,UDP_PACKET_BUFFER_SIZE,0,NULL,NULL);

  if (size < 0)
    return;

  push_main_thread_event(us->cb,push_arg,buf,size);
}

static int api_create_udp_socket(lua_State *state)
{
  int port = luaL_checkinteger(state, 1);

  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  int cb = luaL_ref(state, LUA_REGISTRYINDEX);

  struct udp_socket *us = lua_newuserdata(state, sizeof(*us));
  luaL_setmetatable(state, s_udp_socket_mt);
  api_gc_protect(state, -1);

  us->fd = socket(AF_INET, SOCK_DGRAM, 0);
  us->cb = cb;

  if (us->fd < 0)
    luaL_error(state, "failed to create udp socket: %s", strerror(errno));

  memset(&us->sockaddr, 0, sizeof(us->sockaddr));
  us->sockaddr.sin_family = AF_INET;
  us->sockaddr.sin_addr.s_addr = INADDR_ANY;
  us->sockaddr.sin_port = htons(port);

  if (bind(us->fd, &us->sockaddr, sizeof(us->sockaddr)) < 0) {
    close(us->fd);
    luaL_error(state, "failed to bind udp socket: %s", strerror(errno));
  }

  io_add_input(us->fd, us, io_handler);

  return 1;
}

void api_define_udp_socket(lua_State *state)
{
  const luaL_Reg methods[] = {
    { NULL,        NULL, },
  };

  const luaL_Reg funcs[] = {
    { "udp_bind", api_create_udp_socket, },
    { NULL,       NULL, },
  };

  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setglobal(state, reg->name);
  }

  luaL_newmetatable(state, s_udp_socket_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
