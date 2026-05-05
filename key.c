#include <stdio.h>
#include <unistd.h>

#include "io.h"
#include "api.h"
#include "event.h"

static int s_cb = LUA_NOREF;

static void push_arg(lua_State *state, void *data, size_t size)
{
  char c = *(char*) data;
  char buf[] = {c, 0};
  lua_pushstring(state, buf);
}

static void handler(int fd, void *arg)
{
  char c = fgetc(stdin);
  push_main_thread_event(s_cb, push_arg, &c, sizeof c);
}

static int api_set_key_callback(lua_State *state)
{
  if (s_cb != LUA_NOREF)
    luaL_error(state, "key callback is already set");

  luaL_checktype(state, 1, LUA_TFUNCTION);
  lua_pushvalue(state, 1);
  s_cb = luaL_ref(state, LUA_REGISTRYINDEX);
  io_add_input(STDIN_FILENO, NULL, handler);
  return 0;
}

void api_define_key_callback(lua_State *state)
{
  lua_pushcfunction(state, api_set_key_callback);
  lua_setglobal(state, "key_callback");
}
