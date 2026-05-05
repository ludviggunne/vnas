#ifndef EVENT_H
#define EVENT_H

#include <stddef.h>

#include "api.h"

typedef unsigned long event_id_t;
typedef void (*main_thread_event_push_arg_t)(lua_State*, void*, size_t);

#define NO_EVENT ((event_id_t) -1)

void init_events(void);
void offset_events(unsigned long n);
void convert_user_event_timestamps(unsigned long sr);
void push_midi_event(unsigned long t, int status, int data1, int data2, int cb);
void push_main_thread_event(int cb, main_thread_event_push_arg_t push_arg, void *data, size_t size);
void process_main_thread_events(unsigned long n);
void run_event_callback(event_id_t id);
event_id_t next_event(unsigned long tmax);
void pop_event(void);
unsigned long event_timestamp(int id);

#endif
