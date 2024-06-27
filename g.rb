#!/usr/bin/ruby

require 'ostruct'
require 'optparse'

IGNNAMES = %w(__bswap_16 __bswap_32 __uint16_identity __uint32_identity __uint64_identity)

source = <<__eos__
    setup,
    teardown,
    create,
__eos__

def readsymsfrom(file, opts)
  rt = []
  IO.popen(['cproto', '-si', file], 'rb') do |io|
    io.each_line do |line|
      m = line.match(/\b(?<name>\w+)\b\s*\(/)
      next unless m
      name = m['name']
      next if IGNNAMES.include?(name)
      rt.push(name)
    end
  end
  return rt
end


begin
  names = []
  opts = OpenStruct.new({
    dumpnames: false,
  })
  OptionParser.new{|prs|
    prs.on('-d'){
      opts.dumpnames = true
    }
  }.parse!
  syms = source.split(/,/).map(&:strip).reject(&:empty?)
  if ARGV.length > 0 then
    syms = readsymsfrom(ARGV[0], opts)
  end
  syms.each do |n|
    if !opts.dumpnames then
      next if (n.match(/\b\w+fn_\w+\b/) || n.match(/ecc/))
    end
    names.push(n)
  end
  if opts.dumpnames then
    names.sort_by{|n| n.length }.each do |n|
      printf("  %s\n", n)
    end
  else
    if names.length > 0 then
      print('\b(' + names.join('|') + ')\b')
    end
  end
end


