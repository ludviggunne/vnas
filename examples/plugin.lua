package.path = "examples/?.lua;" .. package.path

local Expression = require 'lib.expression'
local i = Expression.input

local White = Plugin.load_bindings 'caps.so:White'

local plug = White.load {}
local oscil = Oscil.init(0.1, 1.0)
local filter = Filter.init(i(0,0) * (1 + i(1,0)) / 100, { plug:out(0), oscil:out(0) })

plug:volume(1.0)

local port = Port.Audio.output('output', filter:out(0))
port:select 'left'
port:select 'right'
