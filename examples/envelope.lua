require 'lib.clock'
require 'lib.envelope'

local f = 440
local oscil = oscil_init(f,0)
local env = env_init(0.05,0.1,0.2,0.2)

local clock = clock_init(120, function(i)
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

local port = audio_out('out', oscil:out())
port:select 'left'
port:select 'right'
