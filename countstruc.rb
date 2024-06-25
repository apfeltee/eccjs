#!/usr/bin/ruby

begin
  types = {}
  allfiles = Dir.glob('*.c')
  data = File.read('ecc.h')
  data.enum_for(:scan, /\b(?<all>(?<kind>struct|enum)\b\s+\b(?<name>\w+)\b)/).map{ Regexp.last_match }.each do |m|
    a = m['all']
    types[a] = {kind: m['kind'], name: m['name']}
  end
  counted =  Hash.new{|hash, key| hash[key] = 0 }
  allfiles.each do |file|
    data = File.read(file)
    types.each do |full, bits|
      data.enum_for(:scan, /\b#{bits[:kind]}\b\s+\b#{bits[:name]}\b/).map{ Regexp.last_match }.each do |m|
        counted[full] += 1
      end
    end
  end
  counted.sort_by{|_, cnt| cnt}.each do |full, cnt|
    printf("%d\t%s\n", cnt, full)
  end
end

