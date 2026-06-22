require 'lib.expression'
local i = input

local in_port = audio_in('in')
in_port:select('input')

local expr = 0
local d = 20
for k = 0, d do expr = expr + i(0,k) end
expr = expr / d

local f = filter(expr, {in_port:out(0)})

local out1 = audio_out('out1', f:out(0))
local out2 = audio_out('out2', f:out(0))

out1:select('left')
out2:select('right')
