require 'lib.extra'

local samp = sample 'examples/playback_L.wav'
local synth = granular(samp)
local m = mix({synth:out(0), synth:out(1)})
local p = audio_out('out', m:out(0))

synth:num_slots(128)
synth:min_length(0.1)
synth:max_length(1.0)
synth:min_cooldown(0.1)
synth:max_cooldown(1.0)
synth:min_gain(0.1)
synth:max_gain(1.0)
synth:min_offset(0.0)
synth:max_offset(1.0)
synth:min_multiplier(0.25)
synth:max_multiplier(8.0)

p:select 'left'
p:select 'right'
