local M = {}
M.__index = M

function M.init(attack, decay, sustain, release)
  return setmetatable({
    interp = Interp.init(0),
    _attack = attack,
    _decay = decay,
    _sustain = sustain,
    _release = release,
  }, M)
end

function M:attack()
  cancel(self.env_id)
  cancel(self.release_id)
  self.interp:target(1, self._attack)
  self.env_id = schedule(self._attack, self.decay, self)
end

function M:decay()
  cancel(self.env_id)
  self.interp:target(self._decay, self._sustain)
end

function M:release()
  cancel(self.env_id)
  cancel(self.release_id)
  self.interp:target(0, self._release)
end

function M:hold(t)
  cancel(self.env_id)
  cancel(self.release_id)
  self:attack()
  self.release_id = schedule(t, self.release, self)
end

function M:out()
  return self.interp:out()
end

return M
