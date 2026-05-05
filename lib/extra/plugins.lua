function Plugin.load_bindings(spec)
  return (load(Plugin.bindings(spec)))()
end
