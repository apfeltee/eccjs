#!/usr/bin/ruby

begin
  macs = []
  File.foreach('interface.h') do |line|
    m = line.match(/^\s*#\s*define\s+\b(?<name>\w+)\b/)
    next unless m
    n = m['name']
    macs.push(n)
  end
  print('\b(' +  macs.join('|') + ')\b')
end


