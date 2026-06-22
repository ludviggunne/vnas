require 'lib.osc'

udp_bind(9000, function(raw)
  local status, msg = pcall(osc_decode, raw)

  if not status then
    log.info(msg)
    return
  end

  log.info(msg.addr)

  for _, v in ipairs(msg.args) do
    log.info('', type(v), v)
  end
end)
