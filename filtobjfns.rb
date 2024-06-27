#!/usr/bin/ruby

def fromio(hnd, &b)
  seen = []
  hnd.each_line do |l|
    m=l.match(/eccvalue_t\s+(?<name>\w+)\(\s*eccstate_t\s*\*\s*\w+\)/)
    next unless m
    name = m['name']
    next if (name.match?(/fn_/))
    if not seen.include?(name) then
      b.call(name)
      seen.push(name)
    end
  end
end

def fromfile(file, &b)
  File.open(file, 'rb') do |fh|
    fromio(fh, &b)
  end
end

begin
  n=[]
  if ARGV.length > 0 then
    ARGV.each do |file|
      fromfile(file) do |thing|
        $stderr.printf("from %p: %p\n", file, thing)
        n.push(thing)
      end
    end
  else
    fromio($stdin) do |thing|
      n.push(thing)
    end
  end
  if n.length > 0 then
    print('\b(' + n.join('|') + ')\b')
  else
    $stderr.printf("could not extract any symbols! nothing to do.\n")
  end
end

