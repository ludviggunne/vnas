function left(node)
  if node:numout() ~= 2 then
    error('node is not stereo')
  end
  return node:out(0)
end

function right(node)
  if node:numout() ~= 2 then
    error('node is not stereo')
  end
  return node:out(1)
end

function mono(node)
  if node:numout() ~= 1 then
    error('node is not mono')
  end
  return node:out(0)
end
