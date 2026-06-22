-- Plugin:         C* White - Noise generator
-- Label:          White
-- Maker:          Tim Goetze <tim@quitte.de>
-- Copyright:      GPLv3

local M = {}
M.__index = M

function load_c__white___noise_generator(inputs)
  local plugin, out_0 = plugin_load('caps.so:White', inputs)
  return setmetatable({ plugin = plugin }, M), out_0
end

function M:out(index) return self.plugin:out(index) end
function M:numout(index) return self.plugin:numout(index) end

-- volume
function M:volume(value) self.plugin:cntl(0, value) end
function M.min_volume() return 0.00 end
function M.max_volume() return 1.00 end

