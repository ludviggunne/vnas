local Expression = require 'lib.expression'
local i = Expression.input

function Filter.mix(inputs)
  local expr = 0
  for k = 0, #inputs - 1 do
    expr = expr + i(k,0)
  end
  expr = expr / #inputs
  return Filter.init(expr, inputs)
end

function Filter.gain(input, gain)
  return Filter.init(i(0,0) * gain, {input})
end

function Filter.map(input, min, max)
  return Filter.init((i(0,0) + 1) * (max - min) + min, {input})
end
