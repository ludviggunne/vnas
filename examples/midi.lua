local function callback(msg)
  print(msg:status(), msg:data1(), msg:data2())
end

local port = Port.Midi.input('input', callback)
port:select('input')
