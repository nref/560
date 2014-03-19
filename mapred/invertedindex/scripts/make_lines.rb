fp = open(ARGV[0],'r')
ofp = open(ARGV[1],'w')
fp.each { |x| ofp.write("#{$.} #{x}") }
fp.close
ofp.close
