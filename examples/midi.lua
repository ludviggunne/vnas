local function callback(msg)
  log.info(msg:status(), msg:data1(), msg:data2())
end

local port = midi_in('input', callback)
port:select 'input'
