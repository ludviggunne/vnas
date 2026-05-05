#ifndef API_H
#define API_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "node.h"

struct api_node_output {
  node_id_t node;
  int       idx;
};

void api_init(lua_State *state);
int api_did_exit(void);
void api_gc_protect(lua_State *state, int i);
struct api_node_output *api_get_node_output(lua_State *state, int index);
void api_define_out_method(lua_State *state, const char *mt);

void api_define_ports(lua_State *state);
void api_define_plugin(lua_State *state);
void api_define_sample(lua_State *state);
void api_define_playback(lua_State *state);
void api_define_oscil(lua_State *state);
void api_define_granular(lua_State *state);
void api_define_interp(lua_State *state);
void api_define_filter(lua_State *state);
void api_define_user_events(lua_State *state);
void api_define_midi_msg(lua_State *state);
void api_define_key_callback(lua_State *state);
void api_define_osc_socket(lua_State *state);
void api_define_log(lua_State *state);

#endif
