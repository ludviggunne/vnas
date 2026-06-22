package.path = "examples/?.lua;" .. package.path

require 'lib.expression'
local i = expr_input

load_plugin_bindings 'caps.so:White'

local plug = load_c__white___noise_generator {}
local oscil = oscil_init(0.1, 1.0)
local filter = filter_init(i(0,0) * (1 + i(1,0)) / 100, { plug:out(0), oscil:out(0) })

plug:volume(1.0)

local port = audio_out('output', filter:out(0))
port:select 'left'
port:select 'right'
