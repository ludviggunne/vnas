-- TODO: implement in C

local M = {}
M.__index = M

function expr_input(idx, offset)
  return setmetatable({{ 'input', idx, offset or nil }}, M)
end

function expr_const(value)
  return setmetatable({{ 'const', value, nil }}, M)
end

local function toM(value)
  if type(value) == 'number' then
    value = expr_const(value)
  end
  return value
end

local function concat(a, b)
  for _, v in ipairs(b) do
    a[#a + 1] = v
  end
end

local function binop(op, lhs, rhs)
  local result = {}
  concat(result, toM(lhs))
  concat(result, toM(rhs))
  concat(result, {{ op, nil, nil }})
  return setmetatable(result, M)
end

function M.__add(lhs, rhs)
  return binop('add', lhs, rhs)
end

function M.__sub(lhs, rhs)
  return binop('sub', lhs, rhs)
end

function M.__mul(lhs, rhs)
  return binop('mul', lhs, rhs)
end

function M.__div(lhs, rhs)
  return binop('div', lhs, rhs)
end
