local M = {}
M.__index = M

function event_queue_init(callback)
  local self = {}

  self.queue = {}
  self.callback = function(arg)
    table.remove(self.queue, 1)
    callback(arg)
    if #self.queue ~= 0 then
      local t, arg = table.unpack(self.queue[1])
      schedule(t, self.callback, arg)
    end
  end

  return setmetatable(self, M)
end

function M:append(t, arg)
  local event = { t, arg }
  table.insert(self.queue, #self.queue+1, event)
  if #self.queue == 1 then
    schedule(t, self.callback, arg)
  end
end

function M:append_list(list)
  for _, event in ipairs(list) do
    local t, arg = table.unpack(event)
    self:append(t, arg)
  end
end
