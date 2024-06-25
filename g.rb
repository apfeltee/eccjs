#!/usr/bin/ruby

source = <<__eos__

    truth,       integer,  binary,    buffer,         key,       text,         chars,       object,      error,    string,      regexp,        number,
    boolean,     date,     function,  host,           reference, isPrimitive,  isBoolean,   isNumber,    isString, isObject,    isDynamic,     isTrue,
    toPrimitive, toBinary, toInteger, binaryToString, toString,  stringLength, stringBytes, textOf,      toObject, objectValue, objectIsArray, toType,
    equals,      same,     add,       subtract,       less,      more,         lessOrEqual, moreOrEqual, typeName, maskName,    dumpTo,


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
  syms = source.split(/,/).map(&:strip).reject(&:empty?)
  if ARGV.length > 0 then
    syms = readsymsfrom(ARGV[0])
  end
  syms.each do |n|
    next if n.match(/\b\w+fn_\w+\b/)
    names.push(n)
  end
  
  print('\b(' + names.join('|') + ')\b')
end


