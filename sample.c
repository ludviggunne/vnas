#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <sndfile.h>
#include <samplerate.h>

#include "sample.h"
#include "api.h"
#include "log.h"

const char *const sample_mt = "sample";

struct sample {
  char          *path;
  unsigned long  frames;
  unsigned long  sample_rate;
  int            channels;
  float         *data[4];
  struct sample *next;
};

static struct sample *s_sample_list;

static void resample_channel(struct sample *sample, int channel, unsigned long sample_rate)
{
  if (sample->sample_rate == sample_rate)
    return;
}

void resample(unsigned long sample_rate)
{
  SRC_DATA src_data = {0};

  for (struct sample *sample = s_sample_list; sample; sample = sample->next) {

    if (sample->sample_rate == sample_rate)
      continue;

    logdebug("%s\t%lull\t%lull", sample->path, sample->sample_rate, sample_rate);

    src_data.src_ratio = sample_rate / (double) sample->sample_rate;

    src_data.input_frames = sample->frames;
    src_data.output_frames = sample->frames * src_data.src_ratio;

    sample->frames = src_data.output_frames;
    sample->sample_rate = sample_rate;

    for (int i = 0; i < sample->channels; i++) {
      src_data.data_in = sample->data[i];
      src_data.data_out = malloc(sizeof(float) * src_data.output_frames);

      (void) src_simple(&src_data, SRC_SINC_FASTEST, 1);

      free(sample->data[i]);
      sample->data[i] = src_data.data_out;
    }
  }
}

int sample_channels(struct sample *sample)
{
  return sample->channels;
}

unsigned long sample_frames(struct sample *sample)
{
  return sample->frames;
}

float *sample_data(struct sample *sample, int channel)
{
  return sample->data[channel];
}

static int api_load_sample(lua_State *state)
{
  const char *path = luaL_checkstring(state, 1);

  SF_INFO info = {0};
  SNDFILE *f = sf_open(path, SFM_READ, &info);

  if (f == NULL)
    luaL_error(state, "failed to open %s: %s", path, sf_strerror(f));

  assert(info.channels <= 2);

  struct sample *sample = lua_newuserdata(state, sizeof(*sample));
  luaL_setmetatable(state, sample_mt);
  api_gc_protect(state, -1);

  sample->path = strdup(path);
  sample->frames = info.frames;
  sample->sample_rate = info.samplerate;
  sample->channels = info.channels;
  sample->next = s_sample_list;

  float *data = malloc(sample->frames * sample->channels * sizeof(float));
  unsigned long offset = 0;

  while (offset < sample->frames)
    offset += sf_readf_float(f, &data[offset], sample->frames - offset);

  for (int i = 0; i < sample->channels; i++) {
    sample->data[i] = malloc(sample->frames * sizeof(*sample->data[i]));

    for (int j = 0; j < sample->frames; j++)
      sample->data[i][j] = data[j * sample->channels + i];
  }

  free(data);
  sf_close(f);

  return 1;
}

static int api_downmix(lua_State *state)
{
  struct sample *sample = luaL_checkudata(state, 1, sample_mt);

  if (sample->channels == 1)
    return 0;

  for (int i = 0; i < sample->frames; i++) {
    float acc = 0.f;

    for (int j = 0; j < sample->channels; j++) {
      acc += sample->data[j][i];
    }

    sample->data[0][i] = acc / sample->channels;
  }

  for (int i = 1; i < sample->channels; i++)
    free(sample->data[i]);

  sample->channels = 1;

  return 0;
}

static int api_get_length(lua_State *state)
{
  struct sample *sample = luaL_checkudata(state, 1, sample_mt);
  float length = sample->frames / (float) sample->sample_rate;
  lua_pushnumber(state, length);
  return 1;
}

static int api_get_channels(lua_State *state)
{
  struct sample *sample = luaL_checkudata(state, 1, sample_mt);
  lua_pushinteger(state, sample->channels);
  return 1;
}

void api_define_sample(lua_State *state)
{
  const luaL_Reg methods[] = {
    { "downmix",  api_downmix, },
    { "length",   api_get_length, },
    { "channels", api_get_channels, },
    { NULL,       NULL, },
  };

  const luaL_Reg funcs[] = {
    { "load",    api_load_sample, },
    { NULL,        NULL, },
  };

  lua_newtable(state);
  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setfield(state, -2, reg->name);
  }
  lua_setglobal(state, "Sample");

  luaL_newmetatable(state, sample_mt);
  lua_newtable(state);
  luaL_setfuncs(state, methods, 0);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
