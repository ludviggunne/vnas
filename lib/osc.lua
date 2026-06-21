local function fail()
    error 'failed to parse osc message'
end

local function read_byte(state)
  if #state.str == 0 then
    fail()
  end

  local c = state.str:sub(1,1)
  state.str = state.str:sub(2)
  state.i = state.i + 1

  return c
end

local function read_string(state)
  local str = ''

  while true do
    local c = read_byte(state)

    if c == '\0' then
      break
    end

    str = str .. c
  end

  while state.i % 4 ~= 0 do
    if read_byte(state) ~= '\0' then
      fail()
    end
  end

  return str
end

local function read_integer(state)
  if #state.str < 4 then
    fail()
  end

  local bytes = state.str:sub(1,4)
  state.str = state.str:sub(5)
  state.i = state.i + 4

  return string.unpack('>i4', bytes)
end

function osc_decode(str)
  local state = {
    str = str,
    i = 0,
  }

  local addr = read_string(state)
  local ttag = read_string(state)

  local args = {}

  for i = 2, #ttag do
    local c = ttag:sub(i,i)

    if c == 's' then
      local s = read_string(state)
      table.insert(args,s)
    elseif c == 'i' then
      local i = read_integer(state)
      table.insert(args,i)
    else
      fail()
    end
  end

  return {
    addr = addr,
    args = args,
  }
end
