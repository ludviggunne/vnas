require 'lib.expression'
local i = input

function mix(inputs)
  local expr = 0
  for k = 0, #inputs - 1 do
    expr = expr + i(k,0)
  end
  expr = expr / #inputs
  return filter(expr, inputs)
end

function gain(input, gain)
  return filter(i(0,0) * gain, {input})
end

function map(input, min, max)
  return filter((i(0,0) + 1) * (max - min) + min, {input})
end
