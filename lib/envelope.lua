local M = {}
M.__index = M

function env_init(attack, decay, sustain, release)
  return setmetatable({
    interp = interp_init(0),
    _attack = attack,
    _decay = decay,
    _sustain = sustain,
    _release = release,
    _active = false,
  }, M)
end

function M:cancel()
  cancel(self.env_id)
  cancel(self.release_id)
  cancel(self.deact_ud)
end

function M:attack()
  self:cancel()
  self.interp:target(1, self._attack)
  self.env_id = schedule(self._attack, self.decay, self)
  self._active = true
end

function M:decay()
  cancel(self.env_id)
  self.interp:target(self._decay, self._sustain)
end

function M:release()
  self:cancel()
  self.interp:target(0, self._release)
  self.deact_id = schedule(self._release, self.deactivate, self)
end

function M:hold(t)
  self:cancel()
  self:attack()
  self.release_id = schedule(t, self.release, self)
end

function M:deactivate()
  self._active = false
end

function M:active()
  return self._active
end

function M:out()
  return self.interp:out()
end
