serial_open(
  '/dev/ttyACM0', -- Path
  9600,           -- Baud rate
  'cstr',         -- Protocol (cstr = null terminated string)
  log.info        -- Message callback
)
