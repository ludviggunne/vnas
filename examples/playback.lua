function init_port(path, name)
  local samp = sample_load(path)
  samp:downmix()

  local pb = playback_init(samp)
  pb:play()
  pb:loop()

  local port = audio_out(name, pb:out(0))
  port:select(name)
end

init_port('examples/playback_L.wav', 'outputL')
init_port('examples/playback_R.wav', 'outputR')
