require 'lib.clock'
require 'lib.envelope'

local f = 440
local o = oscil(f,0)
local e = envelope(0.05,0.1,0.2,0.2)

local c = clock(120, function(i)
  if f == 440 then
    f = 660
  else
    f = 440
  end

  o:freq(f)
  e:hold(0.2)
end)

c:start()
o:cntl_amp(e:out())

local port = audio_out('out', o:out())
port:select 'left'
port:select 'right'
