#!/usr/bin/ruby

require 'optparse'

source = <<__eos__

    setup,
    teardown,
    createSized,
    createWithCList,


__eos__

def readsymsfrom(file)
  rt = []
  IO.popen(['cproto', '-si', file], 'rb') do |io|
    io.each_line do |line|
      m = line.match(/\b(?<name>\w+)\b\s*\(/)
      next unless m
      rt.push(m['name'])
    end
  end
  return rt
end


begin
  names = []
  dumpnames = false
  OptionParser.new{|prs|
    prs.on('-d'){
      dumpnames = true
    }
  }.parse!
  syms = source.split(/,/).map(&:strip).reject(&:empty?)
  if ARGV.length > 0 then
    syms = readsymsfrom(ARGV[0])
  end
  syms.each do |n|
    next if (n.match(/\b\w+fn_\w+\b/) || n.match(/ecc/))
    names.push(n)
  end
  if dumpnames then
    names.each do |n|
      printf("  %s\n", n)
    end
  else
    print('\b(' + names.join('|') + ')\b')
  end
end


