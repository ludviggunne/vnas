#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <unistd.h>
#include <sys/eventfd.h>

#include <jack/ringbuffer.h>

#include "io.h"
#include "log.h"
#include "api.h"

extern int is_init;

static jack_ringbuffer_t *s_buffer;
static int                s_fd;
static int                s_level;
static char              *s_read_buf = NULL;
static char              *s_write_buf = NULL;
static size_t             s_read_buf_size = 0;
static size_t             s_write_buf_size = 0;

static void io_handler(int fd, void *args)
{
  uint64_t v;
  assert(read(s_fd, &v, sizeof v) == sizeof v);

  size_t len;
  jack_ringbuffer_read(s_buffer, (char*) &len, sizeof len);

  if (len > s_read_buf_size)
    s_read_buf = realloc(s_read_buf, s_read_buf_size = len);

  jack_ringbuffer_read(s_buffer, s_read_buf, len);
  fprintf(stderr, "%.*s", (int) len, s_read_buf);
}

void init_logging(int level)
{
  s_level = level;
  s_fd = eventfd(0, EFD_SEMAPHORE);
  s_buffer = jack_ringbuffer_create(1024*16);
  io_add_input(s_fd, NULL, io_handler);
}

void lograw(int level, const char *msg)
{
  if (level > s_level)
    return;

  if (!is_init) {
    fprintf(stderr, "%s", msg);
    return;
  }

  size_t len = strlen(msg);
  jack_ringbuffer_write(s_buffer, (const char*) &len, sizeof len);
  jack_ringbuffer_write(s_buffer, msg, len);

  uint64_t v = 1;
  assert(write(s_fd, &v, sizeof v) == sizeof v);
}

void logfmt(int level, const char *fmt, ...)
{
  va_list ap_1, ap_2;

  if (level > s_level)
    return;

  if (!is_init) {
    va_start(ap_1, fmt);
    vfprintf(stderr, fmt, ap_1);
    va_end(ap_1);
    fputc('\n', stderr);
    return;
  }

  va_start(ap_1, fmt);
  va_copy(ap_2, ap_1);

  size_t len = 2+vsnprintf(NULL, 0, fmt, ap_1);
  va_end(ap_1);

  if (len > s_write_buf_size)
    s_write_buf = realloc(s_write_buf, s_write_buf_size = len);

  vsnprintf(s_write_buf, len, fmt, ap_2);
  va_end(ap_2);

  s_write_buf[len-1] = '\n';
  s_write_buf[len] = '\0';

  jack_ringbuffer_write(s_buffer, (const char*) &len, sizeof len);
  jack_ringbuffer_write(s_buffer, s_write_buf, len);

  uint64_t v = 1;
  assert(write(s_fd, &v, sizeof v) == sizeof v);
}

static int api_lograw(lua_State *state)
{
  int level = luaL_checkinteger(state, 1);
  const char *msg = luaL_checkstring(state, 2);

  lograw(level, msg);

  return 0;
}

void api_define_log(lua_State *state)
{
  lua_newtable(state);
  lua_pushcfunction(state, api_lograw);
  lua_setfield(state, -2, "raw");
  lua_setglobal(state, "log");
}
