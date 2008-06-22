#########################################################################
# tc_daemon.rb
#
# Test suite for the Daemon class
#########################################################################
if File.basename(Dir.pwd) == "test"
	require "ftools"
	Dir.chdir ".."
	Dir.mkdir("win32") unless File.exists?("win32")
   	File.copy("service.so","win32")
   	$LOAD_PATH.unshift Dir.pwd
end

require "win32/service"
require "test/unit"
include Win32

class TC_Daemon < Test::Unit::TestCase
   def setup
      @d = Daemon.new
   end
   
   def test_version
      assert_equal("0.5.2", Daemon::VERSION)
   end
   
   def test_constructor
      assert_respond_to(Daemon, :new)
      assert_nothing_raised{ Daemon.new }
      assert_raises(ArgumentError){ Daemon.new(1) } # No arguments by default
   end
   
   def test_mainloop
      assert_respond_to(@d, :mainloop)
   end
   
   def test_state
      assert_respond_to(@d, :state)
   end
   
   def test_running
      assert_respond_to(@d, :running?)
   end
   
   def test_constants
      assert_not_nil(Daemon::CONTINUE_PENDING)
      assert_not_nil(Daemon::PAUSE_PENDING)
      assert_not_nil(Daemon::PAUSED)
      assert_not_nil(Daemon::RUNNING)
      assert_not_nil(Daemon::START_PENDING)
      assert_not_nil(Daemon::STOP_PENDING)
      assert_not_nil(Daemon::STOPPED)
      assert_not_nil(Daemon::IDLE) 
   end
   
   def teardown
      @d = nil
   end
end