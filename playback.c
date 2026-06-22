#include <string.h>

#include "node.h"
#include "api.h"
#include "sample.h"

static const char *s_playback_mt = "playback";

struct playback {
  node_id_t      node;
  struct sample *sample;
  unsigned long  offset;
  int            play;
  int            loop;
};

static void process_fn(void *data_ptr, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  struct playback *pb = data_ptr;
  int channels = sample_channels(pb->sample);
  int frames = sample_frames(pb->sample);

  if (!pb->play) {
    for (int i = 0; i < channels; i++)
      memset(out[i], 0, sizeof(*out[i]) * n);

    return;
  }

  for (int i = 0; i < channels; i++) {

    if (i == 0 && !pb->play)
      break;

    float *data = sample_data(pb->sample, i);
    unsigned long offset = pb->offset;

    for (int j = 0; j < n; j++) {
      out[i][j] = data[offset++];

      if (offset == frames) {
        offset = 0;
        if (!pb->loop)
          pb->play = 0;
      }
    }
  }

  pb->offset = (pb->offset + n) % frames;
}

static int api_create_playback(lua_State *state)
{
  struct sample *sample = luaL_checkudata(state, 1, sample_mt);

  struct playback *pb = lua_newuserdata(state, sizeof(*pb));
  luaL_setmetatable(state, s_playback_mt);

  pb->sample = sample;
  pb->offset = 0;
  pb->node = new_node();
  pb->play = 0;
  pb->loop = 0;

  node_set_data_ptr(pb->node, pb);
  node_set_process_fn(pb->node, process_fn);
  node_set_label(pb->node, "playback");

  int channels = sample_channels(sample);
  for (int i = 0; i < channels; i++) {
    node_add_output(pb->node);
  }

  return 1;
}

static int api_play(lua_State *state)
{
  struct playback *pb = luaL_checkudata(state, 1, s_playback_mt);
  pb->play = 1;
  return 0;
}

static int api_pause(lua_State *state)
{
  struct playback *pb = luaL_checkudata(state, 1, s_playback_mt);
  pb->play = 0;
  return 0;
}

static int api_loop(lua_State *state)
{
  struct playback *pb = luaL_checkudata(state, 1, s_playback_mt);
  pb->loop = 1;
  return 0;
}

static int api_restart(lua_State *state)
{
  struct playback *pb = luaL_checkudata(state, 1, s_playback_mt);
  pb->offset = 0;
  return 0;
}

void api_define_playback(lua_State *state)
{
  const luaL_Reg methods[] = {
    { "play",    api_play, },
    { "pause",   api_pause, },
    { "loop",    api_loop, },
    { "restart", api_restart, },
    { NULL,      NULL, },
  };

  const luaL_Reg funcs[] = {
    { "playback_init", api_create_playback, },
    { NULL,       NULL, },
  };

  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setglobal(state, reg->name);
  }

  luaL_newmetatable(state, s_playback_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  api_define_out_method(state, s_playback_mt);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
