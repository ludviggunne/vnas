local Expression = require 'lib.expression'
local i = Expression.input

local input = Port.Audio.input('in')
input:select('input')

local expr = 0
local d = 20
for k = 0, d do expr = expr + i(0,k) end
expr = expr / d

local filter = Filter.init(expr, {input:out(0)})

local out1 = Port.Audio.output('out1', filter:out(0))
local out2 = Port.Audio.output('out2', filter:out(0))

out1:select('left')
out2:select('right')
