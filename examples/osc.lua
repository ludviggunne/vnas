OSC.Socket.init(9000, function(msg)
  log.info(msg.addr)
  for _, v in ipairs(msg.args) do
    log.info('', type(v), v)
  end
end)
