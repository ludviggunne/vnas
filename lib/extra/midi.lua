local M = debug.getregistry()["midi message"].__index

function M:note_on()
  return self:status() >= 0x90 and self:status() < 0xa0
end

function M:note_off()
  return self:status() >= 0x80 and self:status() < 0x90
end

function M:control_change()
  return self:status() >= 0xb0 and self:status() < 0xc0
end

function M:channel()
  return self:status() % 16
end

M.key = M.data1
M.vel = M.data2
M.cc_num = M.data1
M.cc_val = M.data2
