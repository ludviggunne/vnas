#ifndef MIDI_H
#define MIDI_H

#include "api.h"

void api_push_midi_message(lua_State *state, int status, int data1, int data2);

#endif
