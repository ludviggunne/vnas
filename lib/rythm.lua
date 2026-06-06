local M = {}
M.__index = M

local Clock = require 'lib.clock'

function M.init(bpm, pattern, callback)
  local clock

  clock = Clock.init(bpm, function(c)
    if c == #pattern then
      clock:reset()
      c = 0
    end

    local i = c + 1
    local p = pattern:sub(i,i)

    if p ~= ' ' then
      callback(p)
    end
  end)

  return setmetatable({ clock = clock }, M)
end

function M:start()
  self.clock:start()
end

function M:stop()
  self.clock:stop()
end

function M:reset()
  self.clock:reset()
end

return M
