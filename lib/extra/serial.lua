function serial_open2(settings)
  local path = settings.path
  local baud = settings.baud or 9600
  local format = settings.format or '8N1'
  local encoding = settings.encoding or 'cstr'
  local handler = settings.handler or function(_)end
  local size = tonumber(format:sub(1,1))
  local parity = format:sub(2,2)
  local stop = tonumber(format:sub(3,3))

  if path == nil then
    error 'missing path'
  end

  return serial_open(path,baud,size,parity,stop,encoding,handler)
end
