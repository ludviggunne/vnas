#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <assert.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/statistics.h>

#include "log.h"
#include "port.h"
#include "sample.h"
#include "api.h"
#include "node.h"
#include "event.h"

extern jack_client_t *client;

enum port_type {
  PORT_AUDIO_IN,
  PORT_AUDIO_OUT,
  PORT_MIDI_IN,
  PORT_MIDI_OUT,
};

struct port {
  node_id_t       node;
  enum port_type  type;
  jack_port_t    *handle;
  float          *jbuf;
  int             cb;
  int             arg;
  struct port    *next;
};

static unsigned long  s_sample_rate = 0;

static const char *const s_port_type_map[] = {
  [PORT_AUDIO_IN] = JACK_DEFAULT_AUDIO_TYPE,
  [PORT_AUDIO_OUT] = JACK_DEFAULT_AUDIO_TYPE,
  [PORT_MIDI_IN] = JACK_DEFAULT_MIDI_TYPE,
  [PORT_MIDI_OUT] = JACK_DEFAULT_MIDI_TYPE,
};

static const unsigned long s_port_flags_map[] = {
  [PORT_AUDIO_IN] = JackPortIsInput,
  [PORT_AUDIO_OUT] = JackPortIsOutput,
  [PORT_MIDI_IN] = JackPortIsInput,
  [PORT_MIDI_OUT] = JackPortIsOutput,
};

static const unsigned long s_port_peer_flags_map[] = {
  [PORT_AUDIO_IN] = JackPortIsOutput,
  [PORT_AUDIO_OUT] = JackPortIsInput,
  [PORT_MIDI_IN] = JackPortIsOutput,
  [PORT_MIDI_OUT] = JackPortIsInput,
};

static struct port *s_port_lists[] = {
  [PORT_AUDIO_IN] = NULL,
  [PORT_AUDIO_OUT] = NULL,
  [PORT_MIDI_IN] = NULL,
  [PORT_MIDI_OUT] = NULL,
};

static const char *const s_port_mts[] = {
  [PORT_AUDIO_IN] = "audio input port",
  [PORT_AUDIO_OUT] = "audio output port",
  [PORT_MIDI_IN] = "midi input port",
  [PORT_MIDI_OUT] = "midi output port",
};

static void process_fn(void *data, unsigned long n, float **in, int num_in, float **out, int num_out)
{
  struct port *port = data;

  switch (port->type) {
  case PORT_AUDIO_IN:
    if (port->jbuf) {
      memcpy(out[0], port->jbuf, sizeof(*out[0]) * n);
    } else {
      memset(out[0], 0, sizeof(*out[0]) * n);
    }
    return;

  case PORT_AUDIO_OUT:
    if (!port->jbuf)
      return;
    memcpy(port->jbuf, in[0], sizeof(*port->jbuf) * n);
    return;

  default:
    assert(0);
  }
}

static void process_range(jack_nframes_t from, jack_nframes_t to)
{
  jack_nframes_t n = to - from;

  if (n == 0)
    return;

  process_nodes(n);

  for (struct port *port = s_port_lists[PORT_AUDIO_IN]; port; port = port->next)
    if (port->jbuf)
      port->jbuf += n;

  for (struct port *port = s_port_lists[PORT_AUDIO_OUT]; port; port = port->next)
    if (port->jbuf)
      port->jbuf += n;
}

static int process_callback(jack_nframes_t n, void *args)
{
  resize_buffers(n);
  convert_user_event_timestamps(get_sample_rate());
  process_main_thread_events(n);

  for (struct port *port = s_port_lists[PORT_AUDIO_IN]; port; port = port->next)
    port->jbuf = jack_port_get_buffer(port->handle, n);

  for (struct port *port = s_port_lists[PORT_AUDIO_OUT]; port; port = port->next)
    port->jbuf = jack_port_get_buffer(port->handle, n);

  for (struct port *port = s_port_lists[PORT_MIDI_IN]; port; port = port->next) {
    if (port->cb == LUA_NOREF)
      continue;

    void *buf = jack_port_get_buffer(port->handle, n);
    int event_count = jack_midi_get_event_count(buf);

    for (int i = 0; i < event_count; i++) {
      jack_midi_event_t event;
      jack_midi_event_get(&event, buf, i);
      unsigned char *buf = event.buffer;
      push_midi_event(event.time, buf[0], buf[1], buf[2], port->cb);
    }
  }

  jack_nframes_t t0 = 0, t1, t;
  event_id_t event;

  while ((event = next_event(n - t0)) != NO_EVENT) {
    t = event_timestamp(event);

    t1 = t0 + t;
    process_range(t0, t1);
    t0 = t1;

    offset_events(t);
    run_event_callback(event);
    pop_event();
  }

  process_range(t0, n);
  offset_events(n - t0);

  return 0;
}

static int sample_rate_callback(jack_nframes_t n, void *args)
{
  s_sample_rate = n;
  resample(n);
  return 0;
}

static int xrun_callback(void *arg)
{
  float us = jack_get_xrun_delayed_usecs(client);
  loginfo("XRUN (%.2fus)\n", us);
  return 0;
}

unsigned long get_sample_rate(void)
{
  return s_sample_rate;
}

void set_jack_callbacks(void)
{
  jack_set_process_callback(client, process_callback, NULL);
  jack_set_sample_rate_callback(client, sample_rate_callback, NULL);
  jack_set_xrun_callback(client, xrun_callback, NULL);
}

static int api_create_port(lua_State *state)
{
  enum port_type type = luaL_checkinteger(state, lua_upvalueindex(1));

  const char *name = luaL_checkstring(state, 1);
  const char *type_name = s_port_type_map[type];
  unsigned long flags = s_port_flags_map[type];
  const char *mt = s_port_mts[type];
  struct port **list = &s_port_lists[type];

  jack_port_t *handle = jack_port_register(client, name, type_name, flags, 0);

  if (handle == NULL)
    luaL_error(state, "failed to register port '%s'", name);

  struct port *port = lua_newuserdata(state, sizeof(*port));
  luaL_setmetatable(state, mt);
  api_gc_protect(state, -1);

  port->type = type;
  port->handle = handle;
  port->node = -1;
  port->jbuf = NULL;
  port->cb = LUA_NOREF;
  port->arg = LUA_NOREF;
  port->next = *list;

  *list = port;

  switch (type) {
  case PORT_AUDIO_IN:
    port->node = new_node();
    node_set_data_ptr(port->node, port);
    node_set_process_fn(port->node, process_fn);
    node_set_label(port->node, "audio in");
    node_add_output(port->node);
    break;

  case PORT_AUDIO_OUT:
    port->node = new_node();
    node_set_data_ptr(port->node, port);
    node_set_process_fn(port->node, process_fn);
    node_set_label(port->node, "audio out");

    struct api_node_output *out = api_get_node_output(state, 2);
    int in_idx = node_add_input(port->node);
    node_connect(out->node, port->node, out->idx, in_idx);

    node_id_t root = get_root_node();
    int root_idx = node_add_input(root);
    int out_idx = node_add_output(port->node);
    node_connect(port->node, root, out_idx, root_idx);
    break;

  case PORT_MIDI_IN:
    break;

  case PORT_MIDI_OUT:
    assert(0 && "midi output ports are not implemented");
    break;
  }

  return 1;
}

static int api_connect_port(lua_State *state)
{
  enum port_type type = luaL_checkinteger(state, lua_upvalueindex(1));
  const char *mt = s_port_mts[type];

  struct port *port = luaL_checkudata(state, 1, mt);
  const char *peer = luaL_checkstring(state, 2);

  const char *src, *dst;

  switch (type) {
  case PORT_AUDIO_IN:
  case PORT_MIDI_IN:
    src = peer;
    dst = jack_port_name(port->handle);
    break;
  case PORT_AUDIO_OUT:
  case PORT_MIDI_OUT:
    dst = peer;
    src = jack_port_name(port->handle);
    break;
  }

  if (jack_connect(client, src, dst))
    luaL_error(state, "failed to connect ports '%s' and '%s'", src, dst);

  return 0;
}

static int api_list_peers(lua_State *state)
{
  enum port_type type = luaL_checkinteger(state, lua_upvalueindex(1));
  const char *type_name = s_port_type_map[type];
  unsigned long flags = s_port_flags_map[type];

  const char **list = jack_get_ports(client, NULL, type_name, flags);

  int i = 1;
  const char **ptr = list;
  lua_newtable(state);

  for (; ptr && *ptr; i++, ptr++) {
    lua_pushstring(state, *ptr);
    lua_rawseti(state, -2, i);
  }

  if (list)
    jack_free(list);

  return 1;
}

static int api_set_callback(lua_State *state)
{
  struct port *port = luaL_checkudata(state, 1, s_port_mts[PORT_MIDI_IN]);
  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  port->cb = luaL_ref(state, LUA_REGISTRYINDEX);
  return 0;
}

static void api_define_audio(lua_State *state)
{
  lua_newtable(state);

  lua_pushinteger(state, PORT_AUDIO_IN);
  lua_pushcclosure(state, api_create_port, 1);
  lua_setfield(state, -2, "input");

  lua_pushinteger(state, PORT_AUDIO_OUT);
  lua_pushcclosure(state, api_list_peers, 1);
  lua_setfield(state, -2, "sources");

  lua_pushinteger(state, PORT_AUDIO_OUT);
  lua_pushcclosure(state, api_create_port, 1);
  lua_setfield(state, -2, "output");

  lua_pushinteger(state, PORT_AUDIO_IN);
  lua_pushcclosure(state, api_list_peers, 1);
  lua_setfield(state, -2, "destinations");

  lua_setfield(state, -2, "Audio");
}

static void api_define_midi(lua_State *state)
{
  lua_newtable(state);

  lua_pushinteger(state, PORT_MIDI_IN);
  lua_pushcclosure(state, api_create_port, 1);
  lua_setfield(state, -2, "input");

  lua_pushinteger(state, PORT_MIDI_OUT);
  lua_pushcclosure(state, api_list_peers, 1);
  lua_setfield(state, -2, "sources");

  lua_pushinteger(state, PORT_MIDI_OUT);
  lua_pushcclosure(state, api_create_port, 1);
  lua_setfield(state, -2, "output");

  lua_pushinteger(state, PORT_MIDI_IN);
  lua_pushcclosure(state, api_list_peers, 1);
  lua_setfield(state, -2, "destinations");

  lua_setfield(state, -2, "Midi");
}

void api_define_ports(lua_State *state)
{
  lua_newtable(state);
  api_define_audio(state);
  api_define_midi(state);
  lua_setglobal(state, "Port");

  luaL_newmetatable(state, s_port_mts[PORT_AUDIO_IN]);
  lua_newtable(state);
  lua_pushinteger(state, PORT_AUDIO_IN);
  lua_pushcclosure(state, api_connect_port, 1);
  lua_setfield(state, -2, "connect");
  api_define_out_method(state, s_port_mts[PORT_AUDIO_IN]);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);

  luaL_newmetatable(state, s_port_mts[PORT_AUDIO_OUT]);
  lua_newtable(state);
  lua_pushinteger(state, PORT_AUDIO_OUT);
  lua_pushcclosure(state, api_connect_port, 1);
  lua_setfield(state, -2, "connect");
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);

  luaL_newmetatable(state, s_port_mts[PORT_MIDI_IN]);
  lua_newtable(state);
  lua_pushcfunction(state, api_set_callback);
  lua_setfield(state, -2, "callback");
  lua_pushinteger(state, PORT_MIDI_IN);
  lua_pushcclosure(state, api_connect_port, 1);
  lua_setfield(state, -2, "connect");
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);

  luaL_newmetatable(state, s_port_mts[PORT_MIDI_OUT]);
  lua_newtable(state);
  lua_pushinteger(state, PORT_MIDI_OUT);
  lua_pushcclosure(state, api_connect_port, 1);
  lua_setfield(state, -2, "connect");
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}
