local function internal_type(name)
  return debug.getregistry()[name].__index
end

function select_from(prompt, list)
  for i, peer in ipairs(list) do
    log.info(tostring(i) .. '.', peer)
  end
  log.raw(0, prompt .. '> ')
  local i = io.read('*n')
  if i == nil then
    return nil
  end
  return list[i]
end

internal_type('audio input port').select = function(port, prompt)
  port:connect(select_from(prompt, Port.Audio.sources()))
end

internal_type('audio output port').select = function(port, prompt)
  port:connect(select_from(prompt, Port.Audio.destinations()))
end

internal_type('midi input port').select = function(port, prompt)
  port:connect(select_from(prompt, Port.Midi.sources()))
end

internal_type('midi output port').select = function(port, prompt)
  port:connect(select_from(prompt, Port.Midi.destinations()))
end
