-- plugin         C* White - Noise generator
-- label          White
-- maker          Tim Goetze <tim@quitte.de>
-- copyright      GPLv3

local M = {}
M.__index = M

function M.load (inputs)
  local plugin, out_0 = Plugin('caps.so:White', inputs)
  return setmetatable({ plugin = plugin }, M), out_0
end


-- lo: 0.00, hi: 1.00
function M.volume (self, value) self.plugin:cntl(0, value) end

return M
