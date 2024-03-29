@echo off
if not "%~f0" == "~f0" goto WinNT
ruby -Sx C:/LAN/ruby-1.8.6-p111/bin/erb.bat %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofruby
:WinNT
"%~d0%~p0ruby" -x "%~f0" %*
goto endofruby
#!/bin/ruby
# Tiny eRuby --- ERB2
# Copyright (c) 1999-2000,2002 Masatoshi SEKI 
# You can redistribute it and/or modify it under the same terms as Ruby.

require 'erb'

class ERB
  module Main
    def ARGV.switch
      return nil if self.empty?
      arg = self.shift
      return nil if arg == '--'
      if arg =~ /^-(.)(.*)/
        return arg if $1 == '-'
        raise 'unknown switch "-"' if $2.index('-')
        self.unshift "-#{$2}" if $2.size > 0
        "-#{$1}"
      else
        self.unshift arg
        nil
      end
    end
    
    def ARGV.req_arg
      self.shift || raise('missing argument')
    end

    def trim_mode_opt(trim_mode, disable_percent)
      return trim_mode if disable_percent
      case trim_mode
      when 0
        return '%'
      when 1
        return '%>'
      when 2
        return '%<>'
      when '-'
        return '%-'
      end
    end
    module_function :trim_mode_opt

    def run(factory=ERB)
      trim_mode = 0
      disable_percent = false
      begin
        while switch = ARGV.switch
          case switch
          when '-x'                        # ruby source
            output = true
          when '-n'                        # line number
            number = true
          when '-v'                        # verbose
            $VERBOSE = true
          when '--version'                 # version
            STDERR.puts factory.version
            exit
          when '-d', '--debug'             # debug
            $DEBUG = true
          when '-r'                        # require
            require ARGV.req_arg
          when '-S'                        # security level
            arg = ARGV.req_arg
            raise "invalid safe_level #{arg.dump}" unless arg =~ /^[0-4]$/
            safe_level = arg.to_i
          when '-T'                        # trim mode
            arg = ARGV.req_arg
            if arg == '-'
              trim_mode = arg 
              next
            end
            raise "invalid trim mode #{arg.dump}" unless arg =~ /^[0-2]$/
            trim_mode = arg.to_i
          when '-K'                        # KCODE
            arg = ARGV.req_arg
            case arg.downcase
            when 'e', '-e', 'euc'
              $KCODE = 'EUC'
            when 's', '-s', 'sjis'
              $KCODE = 'SJIS'
            when 'u', '-u', 'utf8'
              $KCODE = 'UTF8'
            when 'n', '-n', 'none'
              $KCODE = 'NONE'
            else
              raise "invalid KCODE #{arg.dump}"
            end
          when '-P'
            disable_percent = true
          when '--help'
            raise "print this help"
          else
            raise "unknown switch #{switch.dump}"
          end
        end
      rescue                               # usage
        STDERR.puts $!.to_s
        STDERR.puts File.basename($0) + 
          " [switches] [inputfile]"
        STDERR.puts <<EOU
  -x               print ruby script
  -n               print ruby script with line number
  -v               enable verbose mode
  -d               set $DEBUG to true
  -r [library]     load a library
  -K [kcode]       specify KANJI code-set
  -S [safe_level]  set $SAFE (0..4)
  -T [trim_mode]   specify trim_mode (0..2, -)
  -P               ignore lines which start with "%"
EOU
        exit 1
      end

      src = $<.read
      filename = $FILENAME
      exit 2 unless src
      trim = trim_mode_opt(trim_mode, disable_percent)
      erb = factory.new(src.untaint, safe_level, trim)
      erb.filename = filename
      if output
        if number
          l = 1
          for line in erb.src
            puts "%3d %s"%[l, line]
            l += 1
          end
        else
          puts erb.src
        end
      else
        erb.run(TOPLEVEL_BINDING.taint)
      end
    end
    module_function :run
  end
end

if __FILE__ == $0
  ERB::Main.run
end
__END__
:endofruby
