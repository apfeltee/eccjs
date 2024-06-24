#!/usr/bin/ruby

begin
  tdefs = {}
  file = ARGV[0]
  File.foreach(file) do |line|
    m = line.match(/\b(?<k>struct|enum)\b\s+\b(?<name>\w+)\b/)
    next unless m
    k = m['k']
    n = m['name']
    if tdefs.key?(n) then
      if tdefs[n] != k then
        $stderr.printf("possible conflict: stored type %p is a %p, but also saw a %p\n", n, tdefs[n], k)
        exit(1)
      end
    else
      tdefs[n] = k
    end
  end
  tdefs.each do |name, kind|
    printf("typedef %s %s %s;\n", kind, name, name)
  end
end


