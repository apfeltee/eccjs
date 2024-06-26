#!/usr/bin/ruby

begin
  n=[]
  $stdin.each_line do |l|
    m=l.match(/eccvalue_t\s+(?<name>\w+)\(\s*eccstate_t\s*\*\s*\w+\)/)
    next unless m
    name = m['name']
    next if (name.match?(/fn_/))
    n.push(name) unless n.include?(name)
  end
  print('\b(' + n.join('|') + ')\b')
end

