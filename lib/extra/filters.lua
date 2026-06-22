require 'lib.expression'
local i = expr_input

function mix_init(inputs)
  local expr = 0
  for k = 0, #inputs - 1 do
    expr = expr + i(k,0)
  end
  expr = expr / #inputs
  return filter_init(expr, inputs)
end

function gain_init(input, gain)
  return filter_init(i(0,0) * gain, {input})
end

function map_init(input, min, max)
  return filter_init((i(0,0) + 1) * (max - min) + min, {input})
end
