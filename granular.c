#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "api.h"
#include "port.h"
#include "sample.h"

static const char *const s_synth_mt = "granular synth";

enum {
  PARAM_NUM_SLOTS,
  PARAM_MIN_LENGTH,
  PARAM_MAX_LENGTH,
  PARAM_MIN_COOLDOWN,
  PARAM_MAX_COOLDOWN,
  PARAM_MIN_GAIN,
  PARAM_MAX_GAIN,
  PARAM_MIN_OFFSET,
  PARAM_MAX_OFFSET,
  PARAM_MIN_MULT,
  PARAM_MAX_MULT,
  NUM_PARAMS,
};

static const char *param_names[] = {
  [PARAM_NUM_SLOTS]    = "num_slots",
  [PARAM_MIN_LENGTH]   = "min_length",
  [PARAM_MAX_LENGTH]   = "max_length",
  [PARAM_MIN_COOLDOWN] = "min_cooldown",
  [PARAM_MAX_COOLDOWN] = "max_cooldown",
  [PARAM_MIN_GAIN]     = "min_gain",
  [PARAM_MAX_GAIN]     = "max_gain",
  [PARAM_MIN_OFFSET]   = "min_offset",
  [PARAM_MAX_OFFSET]   = "max_offset",
  [PARAM_MIN_MULT]     = "min_multiplier",
  [PARAM_MAX_MULT]     = "max_multiplier",
};

struct slot {
  unsigned long length;
  unsigned long cooldown;
  unsigned long cursor;
  unsigned long offset;
  int           reverse;
  float         mult;
  float         gain;
  float         pan;
};

struct synth {
  node_id_t      node;
  float          params[NUM_PARAMS];
  node_id_t      cntl[NUM_PARAMS];
  struct sample *sample;
  struct slot   *slots;
  unsigned long  prev_num_slots;
  unsigned long  slots_capac;
  int            lock;
  int            classes[12];
};

static inline float random_param(struct synth *synth, int min_param, int max_param)
{
  float min_bound = synth->params[min_param];
  float max_bound = synth->params[max_param];

  return min_bound + (rand() / (float) RAND_MAX) * (max_bound - min_bound);
}

static inline void init_slot(struct synth *synth, struct slot *slot)
{
  unsigned long frames = sample_frames(synth->sample);
  unsigned long sample_rate = get_sample_rate();

  slot->gain = random_param(synth, PARAM_MIN_GAIN, PARAM_MAX_GAIN);
  slot->mult = random_param(synth, PARAM_MIN_MULT, PARAM_MAX_MULT);
  slot->offset = frames * random_param(synth, PARAM_MIN_OFFSET, PARAM_MAX_OFFSET);
  slot->length = sample_rate * random_param(synth, PARAM_MIN_LENGTH, PARAM_MAX_LENGTH);
  slot->cooldown = sample_rate * random_param(synth, PARAM_MIN_COOLDOWN, PARAM_MAX_COOLDOWN);
  slot->pan = rand() / (float) RAND_MAX;
  // slot->reverse = rand() / (float) RAND_MAX > .5f;
  slot->cursor = 0;

  if (!synth->lock)
    return;

  int empty = 1;
  for (int i = 0; i < 12; i++) {
    if (synth->classes[i]) {
      empty = 0;
      break;
    }
  }

  if (empty) {
    slot->gain = 0.f;
    return;
  }

  int i = 0;
  int r = rand() % 12;
  for (;;) {
    if (synth->classes[i]) {
      if (r == 0) {
        break;
      }
      r--;
    }
    i = (i+1) % 12;
  }

  float m = powf(2.f, i / 12.f);

  while (m < slot->mult)
    m *= 2.f;

  while (m > slot->mult)
    m /= 2.f;

  slot->mult = m;
}

static void process_fn(void *data_ptr, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  struct synth *synth = data_ptr;
  unsigned long num_slots = synth->params[PARAM_NUM_SLOTS];
  unsigned long frames = sample_frames(synth->sample);
  float *data = sample_data(synth->sample, 0);

  if (synth->prev_num_slots < num_slots) {

    if (synth->slots_capac < num_slots) {
      /* allocate new slots */
      synth->slots = realloc(synth->slots, sizeof(*synth->slots) * num_slots);
      synth->slots_capac = num_slots;
    }

    /* init new slots */
    for (int i = synth->prev_num_slots; i < num_slots; i++)
      init_slot(synth, &synth->slots[i]);
  }

  synth->prev_num_slots = num_slots;

  for (int i = 0; i < n; i++) {

    /* buffer controlled parameter */
    for (int j = 0; j < num_in; j++) {
      int param = synth->cntl[j];
      synth->params[param] = in[j][i];
    }

    float left = 0.f;
    float right = 0.f;

    for (int j = 0; j < synth->slots_capac; j++) {
      struct slot *s = &synth->slots[j];

      if (s->cooldown) {
        if (j >= num_slots)
          /* dead slot */
          continue;

        s->cooldown--;
        continue;
      }

      if (s->cursor == s->length) {
        if (j < num_slots)
          init_slot(synth, s);

        continue;
      }

      unsigned long cursor = s->cursor;
      if (s->reverse) {
        cursor = s->length - cursor;
      }

      /* interpolate */
      float fcursor = cursor * s->mult;
      cursor = fcursor;

      float interp = fcursor - cursor;
      unsigned si0 = cursor + frames + s->offset;

      while (si0 >= frames)
        si0 -= frames;

      unsigned si1 = si0 + 1;

      while (si1 >= frames)
        si1 -= frames;

      float s0 = data[si0];
      float s1 = data[si1];
      float sx = s0 + interp * (s1 - s0);

      /* envelope */
      float t = s->cursor / (float) s->length;
      float m = .05f;
      float env = t < m ? t / m : (1.f - t) / (1.f - m);
      float v = sx * s->gain * env;

      /* accumulate */
      left += s->pan * v;
      right += (1.f - s->pan) * v;

      s->cursor++;
    }

    out[0][i] = left;
    out[1][i] = right;
  }
}

static int api_create_synth(lua_State *state)
{
  struct sample *sample = luaL_checkudata(state, 1, sample_mt);

  struct synth *synth = lua_newuserdata(state, sizeof(*synth));
  luaL_setmetatable(state, s_synth_mt);
  api_gc_protect(state, -1);

  memset(synth, 0, sizeof(*synth));
  synth->sample = sample;
  synth->node = new_node();

  node_set_data_ptr(synth->node, synth);
  node_set_process_fn(synth->node, process_fn);
  node_set_label(synth->node, "granular");

  node_add_output(synth->node);
  node_add_output(synth->node);

  return 1;
}

static int api_set_param(lua_State *state)
{
  struct synth *synth = luaL_checkudata(state, 1, s_synth_mt);
  float value = luaL_checknumber(state, 2);

  /* parameter setters use params as upvalues */
  int param = luaL_checkinteger(state, lua_upvalueindex(1));

  synth->params[param] = value;

  return 0;
}

static int api_cntl(lua_State *state)
{
  struct synth *synth = luaL_checkudata(state, 1, s_synth_mt);
  struct api_node_output *src = api_get_node_output(state, 2);
  int param = luaL_checkinteger(state, lua_upvalueindex(1));


  int dst = node_add_input(synth->node);
  // assert(node_is_cntl(src->node));
  assert(dst < NUM_PARAMS);

  node_connect(src->node, synth->node, src->idx, dst);
  synth->cntl[dst] = param;

  return 0;
}

static int lock(lua_State *state, int b)
{
  struct synth *synth = luaL_checkudata(state, 1, s_synth_mt);
  synth->lock = b;
  return 0;
}

static int set_class(lua_State *state, int b)
{
  struct synth *synth = luaL_checkudata(state, 1, s_synth_mt);
  int c = luaL_checkinteger(state, 2) % 12;
  synth->classes[c] += b;

  if (synth->classes[c] < 0)
    synth->classes[c] = 0;

  return 0;
}

static int api_lock(lua_State *state)
{
  return lock(state, 1);
}

static int api_unlock(lua_State *state)
{
  return lock(state, 0);
}

static int api_class_on(lua_State *state)
{
  return set_class(state, 1);
}

static int api_class_off(lua_State *state)
{
  return set_class(state, -1);
}

static int api_class_clear(lua_State *state)
{
  struct synth *synth = luaL_checkudata(state, 1, s_synth_mt);
  memset(&synth->classes, 0, sizeof synth->classes);
  return 0;
}

void api_define_granular(lua_State *state)
{
  const luaL_Reg methods[] = {
    { "lock",        api_lock, },
    { "unlock",      api_unlock, },
    { "class_on",    api_class_on, },
    { "class_off",   api_class_off, },
    { "class_clear", api_class_clear, },
    { NULL,          NULL, },
  };

  const luaL_Reg funcs[] = {
    { "init", api_create_synth, },
    { NULL,    NULL, },
  };

  lua_newtable(state);
  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setfield(state, -2, reg->name);
  }
  lua_setglobal(state, "Granular");

  luaL_newmetatable(state, s_synth_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);

  /* parameter setters use params as upvalues */
  for (int param = 0; param < NUM_PARAMS; param++) {
    lua_pushinteger(state, param);
    lua_pushcclosure(state, api_set_param, 1);
    lua_setfield(state, -2, param_names[param]);

    char buf[512];
    snprintf(buf, sizeof buf, "cntl_%s", param_names[param]);
    lua_pushinteger(state, param);
    lua_pushcclosure(state, api_cntl, 1);
    lua_setfield(state, -2, buf);
  }

  api_define_out_method(state, s_synth_mt);

  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
