require 'lib.expression'
local i = expr_input

local port = audio_in('in')
port:select('input')

local expr = 0
local d = 20
for k = 0, d do expr = expr + i(0,k) end
expr = expr / d

local filter = filter_init(expr, {port:out(0)})

local out1 = audio_out('out1', filter:out(0))
local out2 = audio_out('out2', filter:out(0))

out1:select('left')
out2:select('right')
