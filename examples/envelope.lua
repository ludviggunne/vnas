local Clock = require 'lib.clock'
local Envelope = require 'lib.envelope'

local f = 440
local oscil = Oscil.init(f,0)
local env = Envelope.init(0.05,0.1,0.2,0.2)

local clock = Clock.init(120, function(c)
  if f == 440 then
    f = 660
  else
    f = 440
  end

  oscil:freq(f)
  env:hold(0.2)
end)

clock:start()
oscil:cntl_amp(env:out())

local port = Port.Audio.output('out', oscil:out())
port:select 'left'
port:select 'right'
