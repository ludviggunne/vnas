#include "api.h"

const char *s_api_node_output_mt = "node output";
static int  s_did_exit = 0;

static int api_exit(lua_State *state)
{
  s_did_exit = 1;
  return 0;
}

static int api_get_num_out(lua_State *state)
{
  const char *mt = luaL_checkstring(state, lua_upvalueindex(1));
  node_id_t id = *(node_id_t*) luaL_checkudata(state, 1, mt);
  lua_pushinteger(state, node_get_num_outputs(id));
  return 1;
}

static int api_get_out(lua_State *state)
{
  const char *mt = luaL_checkstring(state, lua_upvalueindex(1));
  node_id_t id = *(node_id_t*) luaL_checkudata(state, 1, mt);
  int index = luaL_optinteger(state, 2, 0);

  int max = node_get_num_outputs(id);
  if (index < 0 || index >= max)
    luaL_error(state, "invalid output index %d/%d", index, max);

  struct api_node_output *out = lua_newuserdata(state, sizeof(*out));
  luaL_setmetatable(state, s_api_node_output_mt);

  out->node = id;
  out->idx = index;

  return 1;
}

void api_define_out_method(lua_State *state, const char *mt)
{
  lua_pushstring(state, mt);
  lua_pushcclosure(state, api_get_out, 1);
  lua_setfield(state, -2, "out");

  lua_pushstring(state, mt);
  lua_pushcclosure(state, api_get_num_out, 1);
  lua_setfield(state, -2, "numout");
}

void api_init(lua_State *state)
{
  luaL_newmetatable(state, s_api_node_output_mt);

  lua_pushcfunction(state, api_exit);
  lua_setglobal(state, "exit");

  api_define_ports(state);
  api_define_plugin(state);
  api_define_sample(state);
  api_define_playback(state);
  api_define_oscil(state);
  api_define_granular(state);
  api_define_interp(state);
  api_define_filter(state);
  api_define_user_events(state);
  api_define_midi_msg(state);
  api_define_key_callback(state);
  api_define_log(state);
  api_define_serial_stream(state);
  api_define_udp_socket(state);
}

int api_did_exit(void)
{
  return s_did_exit;
}

void api_gc_protect(lua_State *state, int i)
{
  lua_pushvalue(state, i);
  (void) luaL_ref(state, LUA_REGISTRYINDEX);
}

struct api_node_output *api_get_node_output(lua_State *state, int index)
{
  return luaL_checkudata(state, index, s_api_node_output_mt);
}
