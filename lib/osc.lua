local function read_byte(state)
  if #state.str == 0 then
    return nil
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

    if c == nil then
      return nil
    end

    if c == '\0' then
      break
    end

    str = str .. c
  end

  while state.i % 4 ~= 0 do
    if read_byte(state) == nil then
      return nil
    end
  end

  return str
end

function osc_decode(str)
  local state = {
    str = str,
    i = 0,
  }

  local addr = read_string(state)
  if addr == nil then
    return nil
  end

  local ttag = read_string(state)
  if ttag == nil then
    return nil
  end

  local args = {}

  for i = 2, #ttag do
    local c = ttag:sub(i,i)

    if c == 's' then
      local s = read_string(state)
      if s == nil then
        return nil
      end
      table.insert(args,s)
    else
      return nil
    end
  end

  return {
    addr = addr,
    args = args,
  }
end
