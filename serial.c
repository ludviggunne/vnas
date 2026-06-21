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

static void stream_setup(lua_State *state, int fd, int baud);

static int api_create_serial_stream(lua_State *state)
{
  const char *path = luaL_checkstring(state, 1);
  int baud = luaL_checkinteger(state, 2);

  luaL_checktype(state, 3, LUA_TFUNCTION);
  lua_pushvalue(state, 3);
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

  stream_setup(state, s->fd, baud);

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

static void stream_setup(lua_State *state, int fd, int baud)
{
  int bflag;

  switch (baud) {
#ifdef B50
  case 50:
    bflag = B50;
    break;
#endif
#ifdef B75
  case 75:
    bflag = B75;
    break;
#endif
#ifdef B110
  case 110:
    bflag = B110;
    break;
#endif
#ifdef B134
  case 134:
    bflag = B134;
    break;
#endif
#ifdef B150
  case 150:
    bflag = B150;
    break;
#endif
#ifdef B200
  case 200:
    bflag = B200;
    break;
#endif
#ifdef B300
  case 300:
    bflag = B300;
    break;
#endif
#ifdef B600
  case 600:
    bflag = B600;
    break;
#endif
#ifdef B1200
  case 1200:
    bflag = B1200;
    break;
#endif
#ifdef B1800
  case 1800:
    bflag = B1800;
    break;
#endif
#ifdef B2400
  case 2400:
    bflag = B2400;
    break;
#endif
#ifdef B4800
  case 4800:
    bflag = B4800;
    break;
#endif
#ifdef B9600
  case 9600:
    bflag = B9600;
    break;
#endif
#ifdef B19200
  case 19200:
    bflag = B19200;
    break;
#endif
#ifdef B38400
  case 38400:
    bflag = B38400;
    break;
#endif
#ifdef B57600
  case 57600:
    bflag = B57600;
    break;
#endif
#ifdef B115200
  case 115200:
    bflag = B115200;
    break;
#endif
#ifdef B230400
  case 230400:
    bflag = B230400;
    break;
#endif
#ifdef B460800
  case 460800:
    bflag = B460800;
    break;
#endif
#ifdef B500000
  case 500000:
    bflag = B500000;
    break;
#endif
#ifdef B576000
  case 576000:
    bflag = B576000;
    break;
#endif
#ifdef B921600
  case 921600:
    bflag = B921600;
    break;
#endif
#ifdef B1000000
  case 1000000:
    bflag = B1000000;
    break;
#endif
#ifdef B1152000
  case 1152000:
    bflag = B1152000;
    break;
#endif
#ifdef B1500000
  case 1500000:
    bflag = B1500000;
    break;
#endif
#ifdef B2000000
  case 2000000:
    bflag = B2000000;
    break;
#endif
#ifdef B2500000
  case 2500000:
    bflag = B2500000;
    break;
#endif
#ifdef B3000000
  case 3000000:
    bflag = B3000000;
    break;
#endif
#ifdef B3500000
  case 3500000:
    bflag = B3500000;
    break;
#endif
#ifdef B4000000
  case 4000000:
    bflag = B4000000;
    break;
#endif
  default:
    bflag = -1;
    break;
  }

  if (bflag < 0) {
    luaL_error(state, "unsupported baud rate: %d", baud);
  }

  struct termios tty;
  memset(&tty, 0, sizeof tty);

  tcgetattr(fd, &tty);
  cfsetspeed(&tty, bflag);

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

  tcsetattr(fd, TCSANOW, &tty);

  tcflush(fd, TCIOFLUSH);
}
