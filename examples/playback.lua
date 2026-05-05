
function init_port(path, name, dest)
  local sample = Sample.load(path)
  sample:downmix()

  local playback = Playback.init(sample)
  playback:play()
  playback:loop()

  local port = Port.Audio.output(name, playback:out(0))
  port:connect(dest)
end

init_port('examples/playback_L.wav', 'outputL', 'Built-in Audio Analog Stereo:playback_FL')
init_port('examples/playback_R.wav', 'outputR', 'Built-in Audio Analog Stereo:playback_FR')
