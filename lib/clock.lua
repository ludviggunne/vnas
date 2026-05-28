local M = {}
M.__index = M

function M.init(bpm, callback)
  local self = {
    user_cb = callback,
    interval = 60.0 / bpm,
    counter = 0,
    active = false,
  }

  self.event_cb = function()
    if self.active then
      self.user_cb(self.counter)
    end
    self.counter = self.counter + 1
    schedule(self.interval, self.event_cb, nil)
  end

  schedule(0, self.event_cb, nil)

  return setmetatable(self, M)
end

function M:activate()
  self.active = true
end

function M:deactivate()
  self.active = false
end

function M:reset()
  self.counter = 0
end

return M
