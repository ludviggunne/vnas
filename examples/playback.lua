function init_port(path, name)
  local samp = sample(path)
  samp:downmix()

  local pb = playback(samp)
  pb:play()
  pb:loop()

  local port = audio_out(name, pb:out(0))
  port:select(name)
end

init_port('examples/playback_L.wav', 'outputL')
init_port('examples/playback_R.wav', 'outputR')
