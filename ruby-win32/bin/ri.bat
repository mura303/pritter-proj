@echo off
if not "%~f0" == "~f0" goto WinNT
ruby -Sx C:/LAN/ruby-1.8.6-p111/bin/ri.bat %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofruby
:WinNT
"%~d0%~p0ruby" -x "%~f0" %*
goto endofruby
#!/bin/ruby
# usage:
#
#   ri  name...
#
# where name can be 
#
#   Class | Class::method | Class#method | Class.method | method
#
# All names may be abbreviated to their minimum unbiguous form. If a name
# _is_ ambiguous, all valid options will be listed.
#
# The form '.' method matches either class or instance methods, while 
# #method matches only instance and ::method matches only class methods.
#
#
# == Installing Documentation
#
# 'ri' uses a database of documentation built by the RDoc utility.
# 
# So, how do you install this documentation on your system?
# It depends on how you installed Ruby.
#
# <em>If you installed Ruby from source files</em> (that is, if it some point
# you typed 'make' during the process :), you can install the RDoc
# documentation yourself. Just go back to the place where you have 
# your Ruby source and type
#
#    make install-doc
#
# You'll probably need to do this as a superuser, as the documentation
# is installed in the Ruby target tree (normally somewhere under 
# <tt>/usr/local</tt>.
#
# <em>If you installed Ruby from a binary distribution</em> (perhaps
# using a one-click installer, or using some other packaging system),
# then the team that produced the package probably forgot to package
# the documentation as well. Contact them, and see if they can add
# it to the next release.
#


require 'rdoc/ri/ri_driver'

######################################################################

ri = RiDriver.new
ri.process_args

__END__
:endofruby
