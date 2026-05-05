local function tabs(level, ...)
  local s = {}
  local t = table.pack(...)
  for i, v in ipairs(t) do s[i] = tostring(v) end
  log.raw(level, table.concat(s, '\t') .. '\n')
end

function log.info(...) tabs(0, ...) end
function log.debug(...) tabs(1, ...) end
function log.trace(...) tabs(2, ...) end

log.fmt = {}

local function fmt(level, ...)
  log.raw(level, string.format(...))
end

function log.fmt.info(...) fmt(0, ...) end
function log.fmt.debug(...) fmt(1, ...) end
function log.fmt.trace(...) fmt(2, ...) end

print = function(...)
  error('print(...) is disabled. Use log.info(...) instead.')
end
