#include "midi.h"

static const char *s_midi_mt = "midi message";

struct midi_msg {
  int status;
  int data1;
  int data2;
};

void api_push_midi_message(lua_State *state, int status, int data1, int data2)
{
  struct midi_msg *msg = lua_newuserdata(state, sizeof *msg);
  luaL_setmetatable(state, s_midi_mt);

  msg->status = status;
  msg->data1 = data1;
  msg->data2 = data2;
}

static int api_get_status(lua_State *state)
{
  struct midi_msg *msg = luaL_checkudata(state, 1, s_midi_mt);
  lua_pushinteger(state, msg->status);
  return 1;
}

static int api_get_data1(lua_State *state)
{
  struct midi_msg *msg = luaL_checkudata(state, 1, s_midi_mt);
  lua_pushinteger(state, msg->data1);
  return 1;
}

static int api_get_data2(lua_State *state)
{
  struct midi_msg *msg = luaL_checkudata(state, 1, s_midi_mt);
  lua_pushinteger(state, msg->data2);
  return 1;
}

void api_define_midi_msg(lua_State *state)
{
  luaL_Reg methods[] = {
    { "status", api_get_status, },
    { "data1", api_get_data1, },
    { "data2", api_get_data2, },
    { NULL, NULL },
  };

  luaL_newmetatable(state, s_midi_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);

  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
