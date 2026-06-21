#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "io.h"
#include "api.h"
#include "log.h"
#include "event.h"

static const char *const s_serial_stream_mt = "serial stream";

struct serial_stream {
  int      fd;
  int      cb;
  int      synced;
  uint8_t *buf;
  size_t   size;
  size_t   capac;
};

static int read_byte(int fd)
{
  uint8_t x;
  if (read(fd, &x, sizeof x) != sizeof x)
    return -1;
  return x;
}

static void put_byte(struct serial_stream *s, int c)
{
  if (s->size == s->capac) {
    s->capac = s->capac == 0 ? 128 : s->capac * 2;
    s->buf = realloc(s->buf, s->capac);
  }
  s->buf[s->size++] = c;
}

static void clear_buf(struct serial_stream *s)
{
  s->size = 0;
}

static int sync_stream(struct serial_stream *s)
{
  uint8_t x;
  while ((x = read_byte(s->fd)) != 0) {
    if (x < 0)
      return -1;
  }
  s->synced = 1;
  return 0;
}

static void push_arg(lua_State *state, void *data, size_t size)
{
  lua_pushlstring(state, data, size);
}

void UnStuffData(const unsigned char *ptr, unsigned long length, unsigned char *dst)
{
  const unsigned char *end = ptr + length;
  while (ptr < end)
  {
    int i, code = *ptr++;
    for (i=1; i<code; i++) *dst++ = *ptr++;
    if (code < 0xFF) *dst++ = 0;
  }
}

static void io_handler(int fd, void *data)
{
  struct serial_stream *s = data;

  if (!s->synced && sync_stream(s) < 0)
    return;

  clear_buf(s);

  int hdr;

  while ((hdr = read_byte(fd)) != 0) {
    if (hdr < 0) {
      s->synced = 0;
      return;
    }

    int i = hdr;
    while (--i) {
      int c = read_byte(fd);

      if (c < 0) {
        s->synced = 0;
        return;
      }

      put_byte(s,c);
    }

    if (hdr != 0xff)
      put_byte(s,0);
  }

  push_main_thread_event(s->cb, push_arg, s->buf, s->size);
}

static int api_create_serial_stream(lua_State *state)
{
  const char *path = luaL_checkstring(state, 1);

  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  int cb = luaL_ref(state, LUA_REGISTRYINDEX);

  struct serial_stream *s = lua_newuserdata(state, sizeof *s);
  luaL_setmetatable(state, s_serial_stream_mt);
  api_gc_protect(state, -1);

  s->cb = cb;
  s->fd = open(path, O_RDWR|O_NOCTTY);
  s->synced = 0;
  s->buf = NULL;
  s->capac = 0;
  s->size = 0;

  if (s->fd < 0) {
    const char *err = strerror(errno);
    luaL_error(state, "failed to open %s: %s", path, err);
  }

  struct termios tty;
  memset(&tty, 0, sizeof tty);

  tcgetattr(s->fd, &tty);
  cfsetspeed(&tty, B9600);

  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag |= CREAD|CLOCAL;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_lflag &= ~(ICANON|ECHO|ISIG);
  tty.c_iflag &= ~(IXON|IXOFF|IXANY);
  tty.c_oflag &= ~OPOST;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;

  tcsetattr(s->fd, TCSANOW, &tty);

  tcflush(s->fd, TCIOFLUSH);

  io_add_input(s->fd, s, io_handler);

  return 1;
}

void api_define_serial_stream(lua_State *state)
{
  const luaL_Reg methods[] = {
    { NULL,        NULL, },
  };

  const luaL_Reg funcs[] = {
    { "init", api_create_serial_stream, },
    { NULL,   NULL, },
  };

  lua_newtable(state);
  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setfield(state, -2, reg->name);
  }
  lua_setglobal(state, "Serial");

  luaL_newmetatable(state, s_serial_stream_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
