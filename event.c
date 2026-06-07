#include <stdlib.h>
#include <assert.h>

#include <jack/ringbuffer.h>
#include <jack/jack.h>

#include "port.h"
#include "api.h"
#include "midi.h"
#include "event.h"
#include "log.h"

extern jack_client_t *client;
extern lua_State *lua_state;
extern int verbose;

static const char *const s_event_handle_mt = "event handle";

enum {
  EVENT_MIDI,
  EVENT_USER,
  EVENT_MAIN_THREAD,
};

struct event {
  int           type;
  int           cb;
  unsigned long t;
  unsigned long seq;
  event_id_t    next_free;
  int           canceled;

  /* midi event data */
  int           midi_status;
  int           midi_data1;
  int           midi_data2;

  /* user event data */
  int           user_arg;

  /* main thread event data */
  int           main_thread_arg;
};

/* user facing handle for cancelation of events */
struct api_event_handle {
  event_id_t    id;
  unsigned long seq;
};


/* user events scheduled before activation
 * need to have their timestamp set later,
 * when the sample rate is known */
struct conv {
  event_id_t   id;
  float        real_t;
  struct conv *next;
};

/* event pool */
static struct event  *s_pool = NULL;
static event_id_t     s_free = NO_EVENT;
static unsigned long  s_pool_size = 0;
static unsigned long  s_pool_capacity = 0;
static unsigned long  s_seq_counter = 0;

/* pending event queue */
static event_id_t    *s_queue = NULL;
static unsigned long  s_queue_len = 0;
static unsigned long  s_queue_capacity = 0;

/* user events to convert after activation */
static struct conv *s_conv = NULL;

/* unhandled main thread events events */
static jack_ringbuffer_t *s_main_thread_events = NULL;
static char              *s_main_thread_event_data = NULL;
static size_t             s_main_thread_event_data_capacity = 0;

static void dump_event(struct event *e)
{
  switch (e->type) {
  case EVENT_USER:
    logtrace("user event(%ld)", e->seq);
    return;
  case EVENT_MIDI:
    logtrace("midi event(%ld)\t%d\t%d\t%d",
            e->seq, e->midi_status, e->midi_data1, e->midi_data2);
    return;
  case EVENT_MAIN_THREAD:
    logtrace("main thread event(%ld)", e->seq);
    return;
  }
}

static int event_cmp(event_id_t a_id, event_id_t b_id)
{
  struct event *a = &s_pool[a_id];
  struct event *b = &s_pool[b_id];

  if (a->t < b->t)
    return -1;

  if (a->t > b->t)
    return 1;

  if (a->seq < b->seq)
    return -1;

  if (a->seq > b->seq)
    return 1;

  return 0;
}

void init_events(void)
{
  assert(s_main_thread_events = jack_ringbuffer_create(1024));
}

static event_id_t alloc_event(void)
{
  event_id_t id;

  if (s_pool_capacity == 0) {
    s_pool_capacity = 32;
    s_pool = malloc(sizeof(*s_pool) * s_pool_capacity);
    id = s_pool_size++;
    goto done;
  }

  if (s_free != NO_EVENT) {
    id = s_free;
    s_free = s_pool[id].next_free;
    goto done;
  }

  if (s_pool_size == s_pool_capacity) {
    s_pool_capacity *= 2;
    s_pool = realloc(s_pool, sizeof(*s_pool) * s_pool_capacity);
  }

  id = s_pool_size++;

done:
  s_pool[id].seq = s_seq_counter++;
  s_pool[id].canceled = 0;
  return id;
}

static void free_event(event_id_t id)
{
  s_pool[id].next_free = s_free;
  s_free = id;
}

static void push_event(event_id_t id)
{
  logtrace("push event (qlen=%ld)", s_queue_len);

  if (s_queue_capacity == 0) {
    s_queue_capacity = 32;
    s_queue = malloc(sizeof(*s_queue) * s_queue_capacity);
    logtrace("init event queue: %ld", s_queue_capacity);
  }

  if (s_queue_len == s_queue_capacity) {
    s_queue_capacity *= 2;
    s_queue = realloc(s_queue, sizeof(*s_queue) * s_queue_capacity);
    logtrace("realloc event queue: %ld", s_queue_capacity);
  }

  s_queue[s_queue_len++] = id;

  /* heapify-up */
  size_t child = s_queue_len - 1;
  while (child > 0) {
    unsigned long parent = (child+1) / 2 -1;

    if (event_cmp(s_queue[child], s_queue[parent]) < 0) {
      event_id_t tmp = s_queue[child];
      s_queue[child] = s_queue[parent];
      s_queue[parent] = tmp;
      child = parent;
      continue;
    }

    break;
  }
}

void pop_event(void)
{
  assert(s_queue_len > 0);

  free_event(s_queue[0]);

  s_queue_len--;
  s_queue[0] = s_queue[s_queue_len];

  /* heapify-down */
  size_t parent = 0;
  while (parent < s_queue_len - 1) {
    unsigned long child = parent;
    unsigned long left = 2 * (parent+1) - 1;
    unsigned long right = left + 1;

    if (left < s_queue_len && event_cmp(s_queue[left], s_queue[child]) < 0)
      child = left;

    if (right < s_queue_len && event_cmp(s_queue[right], s_queue[child]) < 0)
      child = right;

    if (child == parent)
      return;

    event_id_t tmp = s_queue[child];
    s_queue[child] = s_queue[parent];
    s_queue[parent] = tmp;
  }
}

void push_midi_event(unsigned long t, int status, int data1, int data2, int cb)
{
  event_id_t id = alloc_event();
  struct event *e = &s_pool[id];

  e->type = EVENT_MIDI;
  e->midi_status = status;
  e->midi_data1 = data1;
  e->midi_data2 = data2;
  e->cb = cb;
  e->t = t;

  push_event(id);
}

void push_main_thread_event(int cb, main_thread_event_push_arg_t push_arg, void *data, size_t size)
{
  unsigned long t = jack_frame_time(client);
  jack_ringbuffer_write(s_main_thread_events, (const char*) &t, sizeof t);
  jack_ringbuffer_write(s_main_thread_events, (const char*) &cb, sizeof cb);
  jack_ringbuffer_write(s_main_thread_events, (const char*) &push_arg, sizeof push_arg);
  jack_ringbuffer_write(s_main_thread_events, (const char*) &size, sizeof size);
  jack_ringbuffer_write(s_main_thread_events, (const char*) data, size);
}

void process_main_thread_events(unsigned long n)
{
  unsigned long t0 = jack_last_frame_time(client);

  unsigned long t;
  int cb;
  main_thread_event_push_arg_t push_arg;
  size_t size;

  while (jack_ringbuffer_read(s_main_thread_events, (char*) &t, sizeof t)) {

    jack_ringbuffer_read(s_main_thread_events, (char*) &cb, sizeof cb);
    jack_ringbuffer_read(s_main_thread_events, (char*) &push_arg, sizeof push_arg);
    jack_ringbuffer_read(s_main_thread_events, (char*) &size, sizeof size);

    if (size > s_main_thread_event_data_capacity) {
      s_main_thread_event_data_capacity = size;
      s_main_thread_event_data = realloc(
        s_main_thread_event_data,
        s_main_thread_event_data_capacity
      );
    }

    event_id_t id = alloc_event();
    struct event *e = &s_pool[id];

    e->type = EVENT_MAIN_THREAD;
    e->cb = cb;

    jack_ringbuffer_read(s_main_thread_events, (char*) s_main_thread_event_data, size);
    push_arg(lua_state, s_main_thread_event_data, size);

    e->main_thread_arg = luaL_ref(lua_state, LUA_REGISTRYINDEX);

    if (t < t0)
      e->t = 0;
    else if (t >= n)
      e->t = n - 1;
    else
      e->t = t;

    push_event(id);
  }
}

void run_event_callback(event_id_t id)
{
  char key[2] = {0};
  struct event *e = &s_pool[id];

  if (e->cb == LUA_NOREF)
    return;

  lua_rawgeti(lua_state, LUA_REGISTRYINDEX, e->cb);

  switch (e->type) {
  case EVENT_USER:
    lua_rawgeti(lua_state, LUA_REGISTRYINDEX, e->user_arg);
    break;
  case EVENT_MIDI:
    api_push_midi_message(lua_state, e->midi_status,
                          e->midi_data1, e->midi_data2);
    break;
  case EVENT_MAIN_THREAD:
    lua_rawgeti(lua_state, LUA_REGISTRYINDEX, e->main_thread_arg);
    break;
  }

  if (lua_pcall(lua_state, 1, 0, 0) != LUA_OK) {
    const char *err = luaL_checkstring(lua_state, -1);
    fprintf(stderr, "%s\n", err);
  }

  /* Event pool may have been reallocated during callback */
  e = &s_pool[id];

  switch (e->type) {
  case EVENT_USER:
    luaL_unref(lua_state, e->user_arg, LUA_REGISTRYINDEX);
    break;

  case EVENT_MAIN_THREAD:
    luaL_unref(lua_state, e->main_thread_arg, LUA_REGISTRYINDEX);
    break;

  default:
    break;
  }
}

event_id_t next_event(unsigned long tmax)
{
  for (;;) {
    if (s_queue_len == 0)
      return NO_EVENT;

    event_id_t id = s_queue[0];
    struct event *e = &s_pool[id];

    if (e->t >= tmax)
      return NO_EVENT;

    if (verbose >= 2)
      dump_event(e);

    if (e->canceled) {
      pop_event();
      continue;
    }

    return id;
  }
}

unsigned long event_timestamp(int id)
{
  return s_pool[id].t;
}

void offset_events(unsigned long n)
{
  for (int i = 0; i < s_queue_len; i++) {
    struct event *e = &s_pool[s_queue[i]];
    if (e->t < n) {
      // logtrace("offset event (qlen=%ld): WARNING: negative event offset -%ld clamped to zero", s_queue_len, n - e->t);
      logtrace("offset event (qlen=%ld): %ld -> %ld", s_queue_len, e->t, 0);
      e->t = 0;
    } else {
      logtrace("offset event (qlen=%ld): %ld -> %ld", s_queue_len, e->t, e->t - n);
      e->t = e->t < n ? 0 : e->t - n;
    }
  }
}

void convert_user_event_timestamps(unsigned long sr)
{
  while (s_conv != NULL) {
    struct event *e = &s_pool[s_conv->id];
    e->t = s_conv->real_t * sr;
    push_event(s_conv->id);
    struct conv *tmp = s_conv;
    s_conv = s_conv->next;
    free(tmp);
  }
}

static int api_cancel(lua_State *state)
{
  if (lua_isnil(state, 1))
    return 0;

  struct api_event_handle *h = luaL_checkudata(state, 1, s_event_handle_mt);

  if (s_pool[h->id].seq == h->seq)
    s_pool[h->id].canceled = 1;

  return 0;
}

static int api_schedule(lua_State *state)
{
  float real_t = luaL_checknumber(state, 1);

  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  int cb = luaL_ref(state, LUA_REGISTRYINDEX);

  lua_pushvalue(state, 3);
  int arg = luaL_ref(state, LUA_REGISTRYINDEX);

  event_id_t id = alloc_event();
  struct event *e = &s_pool[id];

  e->type = EVENT_USER;
  e->user_arg = arg;
  e->cb = cb;

  unsigned long sr = get_sample_rate();
  if (sr == 0) {
    struct conv *head = malloc(sizeof(*head));
    head->id = id;
    head->real_t = real_t;
    head->next = s_conv;
    s_conv = head;
  } else {
    e->t = real_t * sr;
    push_event(id);
  }

  struct api_event_handle *h = lua_newuserdata(state, sizeof(*h));
  luaL_setmetatable(state, s_event_handle_mt);

  h->id = id;
  h->seq = e->seq;

  return 1;
}

void api_define_user_events(lua_State *state)
{
  const luaL_Reg funcs[] = {
    { "schedule",     api_schedule, },
    { "cancel",       api_cancel, },
    { NULL,           NULL, },
  };

  for (const luaL_Reg *reg = funcs; reg->name; reg++) {
    lua_pushcfunction(state, reg->func);
    lua_setglobal(state, reg->name);
  }

  luaL_newmetatable(state, s_event_handle_mt);
}
