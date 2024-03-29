@echo off
if not "%~f0" == "~f0" goto WinNT
ruby -Sx C:/LAN/ruby-1.8.6-p111/bin/rdoc.bat %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofruby
:WinNT
"%~d0%~p0ruby" -x "%~f0" %*
goto endofruby
#!/bin/ruby
#
#  RDoc: Documentation tool for source code
#        (see lib/rdoc/rdoc.rb for more information)
#
#  Copyright (c) 2003 Dave Thomas
#  Released under the same terms as Ruby
#
#  $Revision: 11708 $

## Transitional Hack ####
#
#  RDoc was initially distributed independently, and installed
#  itself into <prefix>/lib/ruby/site_ruby/<ver>/rdoc...
#
#  Now that RDoc is part of the distribution, it's installed into
#  <prefix>/lib/ruby/<ver>, which unfortunately appears later in the
#  search path. This means that if you have previously installed RDoc,
#  and then install from ruby-lang, you'll pick up the old one by
#  default. This hack checks for the condition, and readjusts the
#  search path if necessary.

def adjust_for_existing_rdoc(path)
  
  $stderr.puts %{
  It seems as if you have a previously-installed RDoc in
  the directory #{path}.

  Because this is now out-of-date, you might want to consider
  removing the directories:

    #{File.join(path, "rdoc")}

  and

    #{File.join(path, "markup")}

  }

  # Move all the site_ruby directories to the end
  p $:
  $:.replace($:.partition {|path| /site_ruby/ !~ path}.flatten)
  p $:
end

$:.each do |path|
  if /site_ruby/ =~ path 
    rdoc_path = File.join(path, 'rdoc', 'rdoc.rb')
    if File.exists?(rdoc_path)
      adjust_for_existing_rdoc(path)
      break
    end
  end
end

## End of Transitional Hack ##


require 'rdoc/rdoc'

begin
  r = RDoc::RDoc.new
  r.document(ARGV)
rescue RDoc::RDocError => e
  $stderr.puts e.message
  exit(1)
end
__END__
:endofruby
