#include <math.h>

#include "node.h"
#include "api.h"
#include "port.h"

static const char *s_oscil_mt = "oscil";

struct oscil {
  node_id_t     node;
  float         freq;
  float         amp;
  int           freq_cntl;
  int           amp_cntl;
  unsigned long counter;
};

static void process_fn(void *data, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  unsigned long sample_rate = get_sample_rate();
  struct oscil *oscil = data;

  for (int i = 0; i < n; i++) {
    float freq = oscil->freq_cntl == -1 ? oscil->freq : in[oscil->freq_cntl][i];
    float amp = oscil->amp_cntl == -1 ? oscil->amp : in[oscil->amp_cntl][i];
    float arg = 2.f * M_PI * freq * oscil->counter / (float) sample_rate;

    if (arg >= 2.f * M_PI)
      oscil->counter = 0;

    out[0][i] = amp * sinf(arg);
    oscil->counter++;
  }
}

static int api_create_oscil(lua_State *state)
{
  float freq = luaL_checknumber(state, 1);
  float amp = luaL_checknumber(state, 2);

  struct oscil *oscil = lua_newuserdata(state, sizeof(*oscil));
  luaL_setmetatable(state, s_oscil_mt);
  api_gc_protect(state, -1);

  oscil->freq = freq;
  oscil->amp = amp;
  oscil->node = new_node();
  oscil->counter = 0;
  oscil->freq_cntl = -1;
  oscil->amp_cntl = -1;

  node_set_data_ptr(oscil->node, oscil);
  node_set_process_fn(oscil->node, process_fn);
  node_set_label(oscil->node, "oscil");
  node_add_output(oscil->node);

  return 1;
}

static int api_cntl_freq(lua_State *state)
{
  struct oscil *oscil = luaL_checkudata(state, 1, s_oscil_mt);
  struct api_node_output *out = api_get_node_output(state, 2);

  oscil->freq_cntl = node_add_input(oscil->node);
  node_connect(out->node, oscil->node, out->idx, oscil->freq_cntl);

  return 0;
}

static int api_cntl_amp(lua_State *state)
{
  struct oscil *oscil = luaL_checkudata(state, 1, s_oscil_mt);
  struct api_node_output *out = api_get_node_output(state, 2);

  oscil->amp_cntl = node_add_input(oscil->node);
  node_connect(out->node, oscil->node, out->idx, oscil->amp_cntl);

  return 0;
}

static int api_set_freq(lua_State *state)
{
  struct oscil *oscil = luaL_checkudata(state, 1, s_oscil_mt);
  oscil->freq = luaL_checknumber(state, 2);
  return 0;
}

static int api_set_amp(lua_State *state)
{
  struct oscil *oscil = luaL_checkudata(state, 1, s_oscil_mt);
  oscil->amp = luaL_checknumber(state, 2);
  return 0;
}

void api_define_oscil(lua_State *state)
{
  const luaL_Reg methods[] = {
    { "cntl_freq", api_cntl_freq, },
    { "cntl_amp",  api_cntl_amp, },
    { "freq",      api_set_freq, },
    { "amp",       api_set_amp, },
    { NULL,        NULL, },
  };

  const luaL_Reg funcs[] = {
    { "oscil_init", api_create_oscil, },
    { NULL,    NULL, },
  };

  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setglobal(state, reg->name);
  }

  luaL_newmetatable(state, s_oscil_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  api_define_out_method(state, s_oscil_mt);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
