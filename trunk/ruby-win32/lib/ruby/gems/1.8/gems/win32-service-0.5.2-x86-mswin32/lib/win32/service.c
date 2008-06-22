#include "ruby.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <tchar.h>
#include "service.h"

#ifndef UNICODE
#define UNICODE
#endif

static VALUE cServiceError;
static VALUE cDaemonError;
static VALUE v_service_struct, v_service_status_struct;

static HANDLE hStartEvent;
static HANDLE hStopEvent;
static HANDLE hStopCompletedEvent;
static SERVICE_STATUS_HANDLE   ssh;
static DWORD dwServiceState;
static TCHAR error[1024];

static VALUE EventHookHash;
static VALUE thread_group;
static int   cAdd;
static int   cList;
static int   cSize;

CRITICAL_SECTION csControlCode;
// I happen to know from looking in the header file
// that 0 is not a valid service control code
// so we will use it, the value does not matter
// as long as it will never show up in ServiceCtrl
// - Patrick Hurley
#define IDLE_CONTROL_CODE 0
static int   waiting_control_code = IDLE_CONTROL_CODE;

static VALUE service_close(VALUE);
void  WINAPI  Service_Main(DWORD dwArgc, LPTSTR *lpszArgv);
void  WINAPI  Service_Ctrl(DWORD dwCtrlCode);
void  ErrorStopService();
void  SetTheServiceStatus(DWORD dwCurrentState,DWORD dwWin32ExitCode,
                          DWORD dwCheckPoint,  DWORD dwWaitHint);

// Called by the service control manager after the call to
// StartServiceCtrlDispatcher.
void WINAPI Service_Main(DWORD dwArgc, LPTSTR *lpszArgv)
{
   int i;

   // Obtain the name of the service.
   LPTSTR lpszServiceName = lpszArgv[0];

   // Register the service ctrl handler.
   ssh = RegisterServiceCtrlHandler(lpszServiceName,
           (LPHANDLER_FUNCTION)Service_Ctrl);

   if(ssh == (SERVICE_STATUS_HANDLE)0){
      ErrorStopService();
      rb_raise(cDaemonError,"RegisterServiceCtrlHandler failed");
   }

   // wait for sevice initialization
   for(i=1;TRUE;i++)
   {
    if(WaitForSingleObject(hStartEvent, 1000) == WAIT_OBJECT_0)
        break;

       SetTheServiceStatus(SERVICE_START_PENDING, 0, i, 1000);
   }

   // The service has started.
   SetTheServiceStatus(SERVICE_RUNNING, NO_ERROR, 0, 0);

   // Main loop for the service.
   while(WaitForSingleObject(hStopEvent, 1000) != WAIT_OBJECT_0)
   {
   }

   // Stop the service.
   SetTheServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
}

VALUE Service_Event_Dispatch(VALUE val)
{
   VALUE func,self;
   VALUE result = Qnil;

   if(val!=Qnil) {
      self = RARRAY(val)->ptr[0];
      func = NUM2INT(RARRAY(val)->ptr[1]);

      result = rb_funcall(self,func,0);
   }

   return result;
}

VALUE Ruby_Service_Ctrl()
{
   while (WaitForSingleObject(hStopEvent,0) == WAIT_TIMEOUT)
	{
      __try
      {
         EnterCriticalSection(&csControlCode);

         // Check to see if anything interesting has been signaled
         if (waiting_control_code != IDLE_CONTROL_CODE)
         {
            // if there is a code, create a ruby thread to deal with it
            // this might be over engineering the solution, but I don't
            // want to block Service_Ctrl longer than necessary and the
            // critical section will block it.
            VALUE val = rb_hash_aref(EventHookHash, INT2NUM(waiting_control_code));
            if(val!=Qnil) {
              VALUE thread = rb_thread_create(Service_Event_Dispatch, (void*) val);
              rb_funcall(thread_group, cAdd, 1, thread);
            }

            // some seriously ugly flow control going on in here
            if (waiting_control_code == SERVICE_CONTROL_STOP)
               break;

            waiting_control_code = IDLE_CONTROL_CODE;
         }
      }
      __finally
      {
         LeaveCriticalSection(&csControlCode);
      }

      // This is an ugly polling loop, be as polite as possible
	   rb_thread_polling();
	}

   for (;;)
   {
      VALUE list = rb_funcall(thread_group, cList, 0);
      VALUE size = rb_funcall(list, cSize, 0);
      if (NUM2INT(size) == 0)
        break;

      // This is another ugly polling loop, be as polite as possible
      rb_thread_polling();
   }
   SetEvent(hStopCompletedEvent);

   return Qnil;
}

// Handles control signals from the service control manager.
void WINAPI Service_Ctrl(DWORD dwCtrlCode)
{
   DWORD dwState = SERVICE_RUNNING;

   // hard to image this code ever failing, so we probably
   // don't need the __try/__finally wrapper
   __try
   {
      EnterCriticalSection(&csControlCode);
      waiting_control_code = dwCtrlCode;
   }
   __finally
   {
      LeaveCriticalSection(&csControlCode);
   }

   switch(dwCtrlCode)
   {
      case SERVICE_CONTROL_STOP:
         dwState = SERVICE_STOP_PENDING;
         break;

      case SERVICE_CONTROL_SHUTDOWN:
         dwState = SERVICE_STOP_PENDING;
         break;

      case SERVICE_CONTROL_PAUSE:
         dwState = SERVICE_PAUSED;
        break;

      case SERVICE_CONTROL_CONTINUE:
         dwState = SERVICE_RUNNING;
        break;

      case SERVICE_CONTROL_INTERROGATE:
         break;

      default:
         break;
   }

   // Set the status of the service.
   SetTheServiceStatus(dwState, NO_ERROR, 0, 0);

   // Tell service_main thread to stop.
   if ((dwCtrlCode == SERVICE_CONTROL_STOP) ||
       (dwCtrlCode == SERVICE_CONTROL_SHUTDOWN))
   {
      // how long should we give ruby to clean up?
      // right now we give it forever :-)
      while (WaitForSingleObject(hStopCompletedEvent, 500) == WAIT_TIMEOUT)
      {
         SetTheServiceStatus(dwState, NO_ERROR, 0, 0);
      }

      if (!SetEvent(hStopEvent))
         ErrorStopService();
         // Raise an error here?
   }
}

//  Wraps SetServiceStatus.
void SetTheServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                         DWORD dwCheckPoint,   DWORD dwWaitHint)
{
   SERVICE_STATUS ss;  // Current status of the service.

   // Disable control requests until the service is started.
   if (dwCurrentState == SERVICE_START_PENDING){
      ss.dwControlsAccepted = 0;
   }
   else{
      ss.dwControlsAccepted =
         SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN|
         SERVICE_ACCEPT_PAUSE_CONTINUE|SERVICE_ACCEPT_SHUTDOWN;
   }

   // Initialize ss structure.
   ss.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
   ss.dwServiceSpecificExitCode = 0;
   ss.dwCurrentState            = dwCurrentState;
   ss.dwWin32ExitCode           = dwWin32ExitCode;
   ss.dwCheckPoint              = dwCheckPoint;
   ss.dwWaitHint                = dwWaitHint;

   dwServiceState = dwCurrentState;

   // Send status of the service to the Service Controller.
   if(!SetServiceStatus(ssh, &ss)){
      ErrorStopService();
   }
}

//  Handle API errors or other problems by ending the service
void ErrorStopService(){

   // If you have threads running, tell them to stop. Something went
   // wrong, and you need to stop them so you can inform the SCM.
   SetEvent(hStopEvent);

   // Stop the service.
   SetTheServiceStatus(SERVICE_STOPPED, GetLastError(), 0, 0);
}

DWORD WINAPI ThreadProc(LPVOID lpParameter){
    SERVICE_TABLE_ENTRY ste[] =
      {{TEXT(""),(LPSERVICE_MAIN_FUNCTION)Service_Main}, {NULL, NULL}};

    if (!StartServiceCtrlDispatcher(ste)){
       ErrorStopService();
       strcpy(error,ErrorDescription(GetLastError()));
       // Very questionable here, we should generate an event
       // and be polling in a green thread for the event, but
       // this really should not happen so here we go
       rb_raise(cDaemonError,error);
    }

    return 0;
}

static VALUE daemon_allocate(VALUE klass){
   EventHookHash = rb_hash_new();

   thread_group = rb_class_new_instance(0, 0,
      rb_const_get(rb_cObject, rb_intern("ThreadGroup")));

   return Data_Wrap_Struct(klass, 0, 0, 0);
}

/*
 * This is the method that actually puts your code into a loop and allows it
 * to run as a service.  The code that is actually run while in the mainloop
 * is what you defined in your own Daemon#service_main method.
 */
static VALUE
daemon_mainloop(VALUE self)
{
    DWORD ThreadId;
    HANDLE hThread;

    dwServiceState = 0;

    // Save a couple symbols
    cAdd = rb_intern("add");
    cList = rb_intern("list");
    cSize = rb_intern("size");

    // Event hooks
    if(rb_respond_to(self,rb_intern("service_stop"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_STOP),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_stop"))));
    }

    if(rb_respond_to(self,rb_intern("service_pause"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_PAUSE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_pause"))));
    }

    if(rb_respond_to(self,rb_intern("service_resume"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_CONTINUE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_resume"))));
    }

    if(rb_respond_to(self,rb_intern("service_interrogate"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_INTERROGATE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_interrogate"))));
    }

    if(rb_respond_to(self,rb_intern("service_shutdown"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_SHUTDOWN),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_shutdown"))));
    }

#ifdef SERVICE_CONTROL_PARAMCHANGE
    if(rb_respond_to(self,rb_intern("service_paramchange"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_PARAMCHANGE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_paramchange"))));
    }
#endif

#ifdef SERVICE_CONTROL_NETBINDADD
    if(rb_respond_to(self,rb_intern("service_netbindadd"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_NETBINDADD),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_netbindadd"))));
    }
#endif

#ifdef SERVICE_CONTROL_NETBINDREMOVE
    if(rb_respond_to(self,rb_intern("service_netbindremove"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_NETBINDREMOVE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_netbindremove"))));
    }
#endif

#ifdef SERVICE_CONTROL_NETBINDENABLE
    if(rb_respond_to(self,rb_intern("service_netbindenable"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_NETBINDENABLE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_netbindenable"))));
    }
#endif

#ifdef SERVICE_CONTROL_NETBINDDISABLE
    if(rb_respond_to(self,rb_intern("service_netbinddisable"))){
       rb_hash_aset(EventHookHash,INT2NUM(SERVICE_CONTROL_NETBINDDISABLE),
          rb_ary_new3(2,self,INT2NUM(rb_intern("service_netbinddisable"))));
    }
#endif

    // Create the event to signal the service to start.
    hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(hStartEvent == NULL){
       strcpy(error,ErrorDescription(GetLastError()));
       ErrorStopService();
       rb_raise(cDaemonError,error);
    }

    // Create the event to signal the service to stop.
    hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(hStopEvent == NULL){
       strcpy(error,ErrorDescription(GetLastError()));
       ErrorStopService();
       rb_raise(cDaemonError,error);
    }

    // Create the event to signal the service that stop has completed
    hStopCompletedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(hStopCompletedEvent == NULL){
       strcpy(error,ErrorDescription(GetLastError()));
       ErrorStopService();
       rb_raise(cDaemonError,error);
    }

    // Create the green thread to poll for Service_Ctrl events
    rb_thread_create(Ruby_Service_Ctrl, 0);

    // Create Thread for service main
    hThread = CreateThread(NULL,0,ThreadProc,0,0,&ThreadId);
    if(hThread == INVALID_HANDLE_VALUE){
       strcpy(error,ErrorDescription(GetLastError()));
       ErrorStopService();
       rb_raise(cDaemonError,error);
    }

    if(rb_respond_to(self,rb_intern("service_init"))){
       rb_funcall(self,rb_intern("service_init"),0);
    }

 SetEvent(hStartEvent);

    // Call service_main method
    if(rb_respond_to(self,rb_intern("service_main"))){
       rb_funcall(self,rb_intern("service_main"),0);
    }

    while(WaitForSingleObject(hStopEvent, 1000) != WAIT_OBJECT_0)
    {
    }

    // Close the event handle and the thread handle.
    if(!CloseHandle(hStopEvent)){
       strcpy(error,ErrorDescription(GetLastError()));
       ErrorStopService();
       rb_raise(cDaemonError,error);
    }

    // Wait for Thread service main
    WaitForSingleObject(hThread, INFINITE);

    return self;
}

/*
 * Returns the state of the service (as an constant integer) which can be any
 * of the service status constants, e.g. RUNNING, PAUSED, etc.
 * 
 * This method is typically used within your service_main method to setup the
 * loop.  For example:
 * 
 * class MyDaemon < Daemon
 *    def service_main
 * 		while state == RUNNING || state == PAUSED || state == IDLE
 * 			# Your main loop here
 * 		end
 * 	end
 * end
 * 
 * See the Daemon#running? method for an abstraction of the above code.
 */
static VALUE daemon_state(VALUE self){
   return UINT2NUM(dwServiceState);
}

/*
 * Returns whether or not the service is in a running state, i.e. the service
 * status is either RUNNING, PAUSED or IDLE.
 * 
 * This is typically used within your service_main method to setup the main
 * loop.  For example:
 * 
 * class MyDaemon < Daemon
 *    def service_main
 *       while running?
 *          # Your main loop here
 *       end
 *    end
 * end
 */
static VALUE daemon_is_running(VALUE self){
   VALUE v_bool = Qfalse;
   if(
      (dwServiceState == SERVICE_RUNNING) ||
      (dwServiceState == SERVICE_PAUSED) ||
      (dwServiceState == 0)
   ){
      v_bool = Qtrue;
   }
	
   return v_bool;	
}

static VALUE service_allocate(VALUE klass){
   SvcStruct* ptr = malloc(sizeof(SvcStruct));
   return Data_Wrap_Struct(klass,0,service_free,ptr);
}

/* call-seq:
 * 	Service.new(host=nil, desired_access=nil)
 * 	Service.new(host=nil, desired_access=nil){ |svc| ... }
 *
 * Creates and returns a new Win32::Service handle on +host+ with the
 * +desired_access+.  If no host is specified, your local machine is
 * used.  If no desired access is specified, then
 * Service::MANAGER_CREATE_SERVICE is used.
 *
 * If a block is provided then the object is yielded back to the block and
 * automatically closed at the end of the block.
 */
static VALUE service_init(int argc, VALUE *argv, VALUE self){
   VALUE v_machine_name, v_desired_access;
   TCHAR* lpMachineName;
   DWORD dwDesiredAccess;
   SvcStruct* ptr;

   Data_Get_Struct(self, SvcStruct, ptr);

   rb_scan_args(argc, argv, "02", &v_machine_name, &v_desired_access);

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   if(NIL_P(v_desired_access))
      dwDesiredAccess = SC_MANAGER_CREATE_SERVICE;
   else
      dwDesiredAccess = NUM2INT(v_desired_access);

   ptr->hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      dwDesiredAccess
   );

   if(!ptr->hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   rb_iv_set(self, "@machine_name", v_machine_name);
   rb_iv_set(self, "@desired_access", v_desired_access);
   rb_iv_set(self, "@service_type",
      INT2FIX(SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS));

   rb_iv_set(self, "@start_type", INT2FIX(SERVICE_DEMAND_START));
   rb_iv_set(self, "@error_control", INT2FIX(SERVICE_ERROR_NORMAL));

   if(rb_block_given_p())
      rb_ensure(rb_yield, self, service_close, self);

   return self;
}

/*
 * call-seq:
 *    Service#close
 *
 * Closes the service handle.  This is the polite way to do things, although
 * the service handle should automatically be closed when it goes out of
 * scope.
 */
static VALUE service_close(VALUE self){
   SvcStruct* ptr;
   int rv;

   Data_Get_Struct(self, SvcStruct, ptr);

   rv = CloseServiceHandle(ptr->hSCManager);

   if(ptr->hSCManager){
      if(0 == rv){
         rb_raise(cServiceError, ErrorDescription(GetLastError()));
      }
   }

   return self;
}

/*
 * call-seq:
 *    Service#configure_service{ |service| ... }
 *
 * Configures the service object.  Valid methods for the service object are
 * as follows:
 *
 * * desired_access=
 * * service_name=
 * * display_name=
 * * service_type=
 * * start_type=
 * * error_control=
 * * tag_id=
 * * binary_path_name=
 * * load_order_group=
 * * start_name=
 * * password=
 * * dependencies=
 * * service_description=
 *
 * See the docs for individual instance methods for more details.
 */
static VALUE service_configure(VALUE self){
   SvcStruct* ptr;
   SC_HANDLE hSCService;
   DWORD dwServiceType, dwStartType, dwErrorControl;
   TCHAR* lpServiceName;
   TCHAR* lpDisplayName;
   TCHAR* lpBinaryPathName;
   TCHAR* lpLoadOrderGroup;
   TCHAR* lpServiceStartName;
   TCHAR* lpPassword;
   TCHAR* lpDependencies;
   int rv;

   Data_Get_Struct(self,SvcStruct,ptr);

   rb_yield(self); /* block is mandatory */

   if(NIL_P(rb_iv_get(self, "@service_name"))){
      rb_raise(cServiceError, "No service name specified");
   }
   else{
      VALUE v_tmp = rb_iv_get(self, "@service_name");
      lpServiceName = TEXT(StringValuePtr(v_tmp));
   }

   hSCService = OpenService(
      ptr->hSCManager,
      lpServiceName,
      SERVICE_CHANGE_CONFIG
   );

   if(!hSCService)
      rb_raise(cServiceError, ErrorDescription(GetLastError()));

   if(NIL_P(rb_iv_get(self, "@service_type")))
      dwServiceType = SERVICE_NO_CHANGE;
   else
      dwServiceType = NUM2INT(rb_iv_get(self, "@service_type"));

   if(NIL_P(rb_iv_get(self, "@start_type")))
      dwStartType = SERVICE_NO_CHANGE;
   else
      dwStartType = NUM2INT(rb_iv_get(self, "@start_type"));

   if(NIL_P(rb_iv_get(self, "@error_control")))
      dwErrorControl = SERVICE_NO_CHANGE;
   else
      dwErrorControl = NUM2INT(rb_iv_get(self, "@error_control"));

   if(NIL_P(rb_iv_get(self, "@binary_path_name"))){
      lpBinaryPathName = NULL;
   }
   else{
      VALUE v_tmp = rb_iv_get(self, "@binary_path_name");
      lpBinaryPathName = TEXT(StringValuePtr(v_tmp));
   }

   if(NIL_P(rb_iv_get(self, "@load_order_group"))){
      lpLoadOrderGroup = NULL;
   }
   else{
      VALUE v_tmp = rb_iv_get(self, "@load_order_group");
      lpLoadOrderGroup = TEXT(StringValuePtr(v_tmp));
   }

   /* There are 3 possibilities for dependencies - Some, none, or unchanged:
    *
    * null        => don't change
    * empty array => no dependencies (deletes any existing dependencies)
    * array       => sets dependencies (deletes any existing dependencies)
    */
   if(NIL_P(rb_iv_get(self, "@dependencies"))){
      lpDependencies = NULL;
   }
   else{
      int i,size=1;
      TCHAR* ptr;
      VALUE rbDepArray = rb_iv_get(self, "@dependencies");

      if(0 == RARRAY(rbDepArray)->len){
         lpDependencies = TEXT("");
      }
      else{
      	 for(i = 0; i< RARRAY(rbDepArray)->len; i++)
      	 {
      	    size += strlen(StringValueCStr(RARRAY(rbDepArray)->ptr[i]))+1;
      	 }
         lpDependencies = malloc(size);
         memset(lpDependencies, 0x00, size);
         ptr = lpDependencies;
         for(i = 0; i < RARRAY(rbDepArray)->len; i++){
            VALUE v_tmp = rb_ary_entry(rbDepArray,i);
            TCHAR* string = TEXT(StringValuePtr(v_tmp));
            memcpy(ptr,string,strlen(string));
            ptr+=strlen(string)+1;
         }
      }
   }

   if(NIL_P(rb_iv_get(self, "@start_name"))){
      lpServiceStartName = NULL;
   }
   else{
      VALUE v_tmp = rb_iv_get(self, "@start_name");
      lpServiceStartName = TEXT(StringValuePtr(v_tmp));
   }

   if(NIL_P(rb_iv_get(self, "@password"))){
      lpPassword = NULL;
   }
   else{
      VALUE v_tmp = rb_iv_get(self, "@password");
      lpPassword = TEXT(StringValuePtr(v_tmp));
   }

   if(NIL_P(rb_iv_get(self, "@display_name"))){
      lpDisplayName = NULL;
   }
   else{
      VALUE v_tmp = rb_iv_get(self, "@display_name");
      lpDisplayName = TEXT(StringValuePtr(v_tmp));
   }

   rv = ChangeServiceConfig(
      hSCService,
      dwServiceType,
      dwStartType,
      dwErrorControl,
      lpBinaryPathName,
      lpLoadOrderGroup,
      NULL, // TagID
      lpDependencies,
      lpServiceStartName,
      lpPassword,
      lpDisplayName
   );

   if(lpDependencies)
      free(lpDependencies);

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCService);
      rb_raise(cServiceError,error);
   }

   if(!NIL_P(rb_iv_get(self, "@service_description"))){
      SERVICE_DESCRIPTION servDesc;
      VALUE v_desc = rb_iv_get(self, "@service_description");

      servDesc.lpDescription = TEXT(StringValuePtr(v_desc));

      if(!ChangeServiceConfig2(
         hSCService,
         SERVICE_CONFIG_DESCRIPTION,
         &servDesc
      )){
         rb_raise(cServiceError,ErrorDescription(GetLastError()));
      }
   }

   CloseServiceHandle(hSCService);

   return self;
}

/*
 * call-seq:
 *    Service#create_service{ |service| ... }
 *
 * Creates the specified service.  In order for this to work, the
 * 'service_name' and 'binary_path_name' attributes must be defined
 * or ServiceError will be raised.

 * See the Service#configure_service method for a list of valid methods to
 * pass to the service object.  See the individual methods for more
 * information, including default values.
*/
static VALUE service_create(VALUE self){
   VALUE v_tmp;
   SvcStruct* ptr;
   SC_HANDLE hSCService;
   DWORD dwDesiredAccess, dwServiceType, dwStartType, dwErrorControl;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   TCHAR* lpDisplayName;
   TCHAR* lpBinaryPathName;
   TCHAR* lpLoadOrderGroup;
   TCHAR* lpServiceStartName;
   TCHAR* lpPassword;
   TCHAR* lpDependencies;

   if(rb_block_given_p())
      rb_yield(self);

   Data_Get_Struct(self, SvcStruct, ptr);

   // The service name and exe name must be set to create a service
   if(NIL_P(rb_iv_get(self, "@service_name")))
      rb_raise(cServiceError, "Service Name must be defined");

   if(NIL_P(rb_iv_get(self, "@binary_path_name")))
      rb_raise(cServiceError, "Executable Name must be defined");

   // If the display name is not set, set it to the same as the service name
   if(NIL_P(rb_iv_get(self, "@display_name")))
      rb_iv_set(self,"@display_name", rb_iv_get(self,"@service_name"));

   v_tmp = rb_iv_get(self, "@service_name");
   lpServiceName = TEXT(StringValuePtr(v_tmp));

   v_tmp = rb_iv_get(self, "@display_name");
   lpDisplayName = TEXT(StringValuePtr(v_tmp));

   v_tmp = rb_iv_get(self, "@binary_path_name");
   lpBinaryPathName = TEXT(StringValuePtr(v_tmp));

   if(NIL_P(rb_iv_get(self, "@machine_name"))){
      lpMachineName = NULL;
   }
   else{
      v_tmp = rb_iv_get(self, "@machine_name");
      lpMachineName = TEXT(StringValuePtr(v_tmp));
   }

   if(NIL_P(rb_iv_get(self, "@load_order_group"))){
      lpLoadOrderGroup = NULL;
   }
   else{
      v_tmp = rb_iv_get(self, "@load_order_group");
      lpLoadOrderGroup = TEXT(StringValuePtr(v_tmp));
   }

   if(NIL_P(rb_iv_get(self, "@start_name"))){
      lpServiceStartName = NULL;
   }
   else{
      v_tmp = rb_iv_get(self,"@start_name");
      lpServiceStartName =
         TEXT(StringValuePtr(v_tmp));
   }

   if(NIL_P(rb_iv_get(self, "@password"))){
      lpPassword = NULL;
   }
   else{
      v_tmp = rb_iv_get(self,"@password");
      lpPassword = TEXT(StringValuePtr(v_tmp));
   }

   // There are 3 possibilities for dependencies - Some, none, or unchanged
   // null = don't change
   // empty array = no dependencies (deletes any existing dependencies)
   // array = sets dependencies (deletes any existing dependencies)
   if(NIL_P(rb_iv_get(self, "@dependencies"))){
      lpDependencies = NULL;
   }
   else{
      int i,size=1;
      TCHAR* ptr;
      VALUE rbDepArray = rb_iv_get(self, "@dependencies");

      if(0 == RARRAY(rbDepArray)->len){
         lpDependencies = TEXT("");
      }
      else{
      	 for(i = 0; i< RARRAY(rbDepArray)->len; i++)
      	 {
      	    size += strlen(StringValueCStr(RARRAY(rbDepArray)->ptr[i]))+1;
      	 }
         lpDependencies = malloc(size);
         memset(lpDependencies,0x00,size);
         ptr = lpDependencies;
         for(i = 0; i < RARRAY(rbDepArray)->len; i++){
            VALUE v_tmp = rb_ary_entry(rbDepArray,i);
            TCHAR* string = TEXT(StringValuePtr(v_tmp));
            memcpy(ptr,string,strlen(string));
            ptr+=strlen(string)+1;
         }
      }
   }

   if(NIL_P(rb_iv_get(self, "@desired_access"))){
      dwDesiredAccess = SERVICE_ALL_ACCESS;
   }
   else{
      dwDesiredAccess = NUM2INT(rb_iv_get(self, "@desired_access"));
   }

   if(NIL_P(rb_iv_get(self,"@service_type"))){
      dwServiceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS;
   }
   else{
      dwServiceType = NUM2INT(rb_iv_get(self, "@service_type"));
   }

   if(NIL_P(rb_iv_get(self,"@start_type"))){
      dwStartType = SERVICE_DEMAND_START;
   }
   else{
      dwStartType = NUM2INT(rb_iv_get(self, "@start_type"));
   }

   if(NIL_P(rb_iv_get(self, "@error_control"))){
      dwErrorControl = SERVICE_ERROR_NORMAL;
   }
   else{
      dwErrorControl = NUM2INT(rb_iv_get(self, "@error_control"));
   }

   // Add support for tag id and dependencies
   hSCService = CreateService(
      ptr->hSCManager,
      lpServiceName,
      lpDisplayName,
      dwDesiredAccess,
      dwServiceType,
      dwStartType,
      dwErrorControl,
      lpBinaryPathName,
      lpLoadOrderGroup,
      NULL,                                              // Tag ID
      lpDependencies,
      lpServiceStartName,
      lpPassword
   );

   if(lpDependencies)
      free(lpDependencies);

   if(!hSCService)
      rb_raise(cServiceError, ErrorDescription(GetLastError()));

   // Set the description after the fact if specified, since we can't set it
   // in CreateService().
   if(!NIL_P(rb_iv_get(self, "@service_description"))){
      SERVICE_DESCRIPTION servDesc;
      VALUE v_desc = rb_iv_get(self, "@service_description");

      servDesc.lpDescription = TEXT(StringValuePtr(v_desc));

      if(!ChangeServiceConfig2(
         hSCService,
         SERVICE_CONFIG_DESCRIPTION,
         &servDesc
      )){
         rb_raise(cServiceError,ErrorDescription(GetLastError()));
      }
   }

   CloseServiceHandle(hSCService);
   return self;
}

// CLASS METHODS

/*
 * call-seq:
 *    Service.delete(name, host=localhost)
 *
 * Deletes the service +name+ from +host+, or the localhost if none is
 * provided.
 */
static VALUE service_delete(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager, hSCService;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   VALUE v_service_name, v_machine_name;

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CREATE_SERVICE
   );

   if(!hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      DELETE
   );

   if(!hSCService){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   if(!DeleteService(hSCService)){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCService);
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   CloseServiceHandle(hSCService);
   CloseServiceHandle(hSCManager);

   return klass;
}

/*
 * call-seq:
 *    Service.services(host=nil, group=nil){ |struct| ... }
 *
 * Enumerates over a list of service types on host, or the local
 * machine if no host is specified, yielding a Win32Service struct for each
 * service.
 *
 * If a 'group' is specified, then only those services that belong to
 * that group are enumerated.  If an empty string is provided, then only
 * services that do not belong to any group are enumerated. If this parameter
 * is nil, group membership is ignored and all services are enumerated.
 *
 * The 'group' option is only available on Windows 2000 or later, and only
 * if compiled with VC++ 7.0 or later, or the .NET SDK.
 *
 * The Win32 service struct contains the following members.
 *
 * * service_name
 * * display_name
 * * service_type
 * * current_state
 * * controls_accepted
 * * win32_exit_code
 * * service_specific_exit_code
 * * check_point
 * * wait_hint
 * * binary_path_name
 * * start_type
 * * error_control
 * * load_order_group
 * * tag_id
 * * start_name
 * * dependencies
 * * description
 * * interactive
 * * pid           (Win2k or later)
 * * service_flags (Win2k or later)
*/
static VALUE service_services(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager = NULL;
   SC_HANDLE hSCService = NULL;
   DWORD dwBytesNeeded = 0;
   DWORD dwServicesReturned = 0;
   DWORD dwResumeHandle = 0;
   LPQUERY_SERVICE_CONFIG lpqscConf;
   LPSERVICE_DESCRIPTION lpqscDesc;
   TCHAR* lpMachineName;
   VALUE v_machine_name = Qnil;
   VALUE v_dependencies = Qnil;
   VALUE v_struct;
   VALUE v_array = Qnil;
   int rv = 0;

#ifdef HAVE_ENUMSERVICESSTATUSEX
	TCHAR* pszGroupName;
	VALUE v_group = Qnil;
   ENUM_SERVICE_STATUS_PROCESS svcArray[MAX_SERVICES];
   rb_scan_args(argc, argv, "02", &v_machine_name, &v_group);
#else
   ENUM_SERVICE_STATUS svcArray[MAX_SERVICES];
   rb_scan_args(argc, argv, "01", &v_machine_name);
#endif

   // If no block is provided, return an array of struct's.
   if(!rb_block_given_p())
      v_array = rb_ary_new();

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

#ifdef HAVE_ENUMSERVICESSTATUSEX
   if(NIL_P(v_group)){
      pszGroupName = NULL;
   }
   else{
      SafeStringValue(v_group);
      pszGroupName = TEXT(StringValuePtr(v_group));
   }
#endif

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_ENUMERATE_SERVICE
   );

   if(NULL == hSCManager){
      sprintf(error, "OpenSCManager() call failed: %s",
         ErrorDescription(GetLastError()));
      rb_raise(cServiceError,error);
   }

   lpqscConf = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, MAX_BUF_SIZE);
   lpqscDesc = (LPSERVICE_DESCRIPTION) LocalAlloc(LPTR, MAX_BUF_SIZE);

#ifdef HAVE_ENUMSERVICESSTATUSEX
   rv = EnumServicesStatusEx(
      hSCManager,                       // SC Manager
      SC_ENUM_PROCESS_INFO,             // Info level (only possible value)
      SERVICE_WIN32 | SERVICE_DRIVER,   // Service type
      SERVICE_STATE_ALL,                // Service state
      (LPBYTE)svcArray,                 // Array of structs
      sizeof(svcArray),
      &dwBytesNeeded,
      &dwServicesReturned,
      &dwResumeHandle,
      pszGroupName
   );
#else
   rv = EnumServicesStatus(
      hSCManager,                       // SC Manager
      SERVICE_WIN32 | SERVICE_DRIVER,   // Service type
      SERVICE_STATE_ALL,                // Service state
      svcArray,                         // Array of structs
      sizeof(svcArray),
      &dwBytesNeeded,
      &dwServicesReturned,
      &dwResumeHandle
   );
#endif

   if(rv != 0)
   {
      unsigned i;
      int rv;
      VALUE v_service_type, v_current_state, v_controls_accepted;
      VALUE v_binary_path_name, v_error_control, v_load_order_group;
      VALUE v_start_type, v_service_start_name, v_description, v_interactive;

      for(i = 0; i < dwServicesReturned; i++){
         DWORD dwBytesNeeded;
         v_controls_accepted = rb_ary_new();
         v_interactive = Qfalse;

         hSCService = OpenService(
            hSCManager,
            svcArray[i].lpServiceName,
            SERVICE_QUERY_CONFIG
         );

         if(!hSCService){
            sprintf(error, "OpenService() call failed: %s",
               ErrorDescription(GetLastError()));
            CloseServiceHandle(hSCManager);
            rb_raise(cServiceError, error);
         }

         // Retrieve a QUERY_SERVICE_CONFIG structure for the Service, from
         // which we can gather the service type, start type, etc.
         rv = QueryServiceConfig(
            hSCService,
            lpqscConf,
            MAX_BUF_SIZE,
            &dwBytesNeeded
         );

         if(0 == rv){
            sprintf(error, "QueryServiceConfig() call failed: %s",
               ErrorDescription(GetLastError()));
            CloseServiceHandle(hSCManager);
            rb_raise(cServiceError, error);
         }

         // Get the description for the Service
         rv = QueryServiceConfig2(
            hSCService,
            SERVICE_CONFIG_DESCRIPTION,
            (LPBYTE)lpqscDesc,
            MAX_BUF_SIZE,
            &dwBytesNeeded
         );

         if(0 == rv){
            sprintf(error,"QueryServiceConfig2() call failed: %s",
               ErrorDescription(GetLastError()));
            CloseServiceHandle(hSCManager);
            rb_raise(cServiceError, error);
         }

#ifdef HAVE_ENUMSERVICESSTATUSEX
         if(svcArray[i].ServiceStatusProcess.dwServiceType
            & SERVICE_INTERACTIVE_PROCESS){
            v_interactive = Qtrue;
         }
#else
         if(svcArray[i].ServiceStatus.dwServiceType
            & SERVICE_INTERACTIVE_PROCESS){
            v_interactive = Qtrue;
         }
#endif

#ifdef HAVE_ENUMSERVICESSTATUSEX
         v_service_type =
            rb_get_service_type(svcArray[i].ServiceStatusProcess.dwServiceType);

         v_current_state =
            rb_get_current_state(
               svcArray[i].ServiceStatusProcess.dwCurrentState);

         v_controls_accepted =
            rb_get_controls_accepted(
               svcArray[i].ServiceStatusProcess.dwControlsAccepted);
#else
         v_service_type =
            rb_get_service_type(svcArray[i].ServiceStatus.dwServiceType);

         v_current_state =
            rb_get_current_state(svcArray[i].ServiceStatus.dwCurrentState);

         v_controls_accepted =
            rb_get_controls_accepted(
               svcArray[i].ServiceStatus.dwControlsAccepted);
#endif

         if(_tcslen(lpqscConf->lpBinaryPathName) > 0)
            v_binary_path_name = rb_str_new2(lpqscConf->lpBinaryPathName);
         else
            v_binary_path_name = Qnil;

         if(_tcslen(lpqscConf->lpLoadOrderGroup) > 0)
            v_load_order_group = rb_str_new2(lpqscConf->lpLoadOrderGroup);
         else
            v_load_order_group = Qnil;

         if(_tcslen(lpqscConf->lpServiceStartName) > 0)
            v_service_start_name = rb_str_new2(lpqscConf->lpServiceStartName);
         else
            v_service_start_name = Qnil;

         if(lpqscDesc->lpDescription != NULL)
            v_description = rb_str_new2(lpqscDesc->lpDescription);
         else
            v_description = Qnil;

         v_start_type    = rb_get_start_type(lpqscConf->dwStartType);
         v_error_control = rb_get_error_control(lpqscConf->dwErrorControl);
         v_dependencies  = rb_get_dependencies(lpqscConf->lpDependencies);

         CloseServiceHandle(hSCService);

#ifdef HAVE_ENUMSERVICESSTATUSEX
         v_struct = rb_struct_new(v_service_struct,
            rb_str_new2(svcArray[i].lpServiceName),
            rb_str_new2(svcArray[i].lpDisplayName),
            v_service_type,
            v_current_state,
            v_controls_accepted,
            INT2FIX(svcArray[i].ServiceStatusProcess.dwWin32ExitCode),
            INT2FIX(svcArray[i].ServiceStatusProcess.dwServiceSpecificExitCode),
            INT2FIX(svcArray[i].ServiceStatusProcess.dwCheckPoint),
            INT2FIX(svcArray[i].ServiceStatusProcess.dwWaitHint),
            v_binary_path_name,
            v_start_type,
            v_error_control,
            v_load_order_group,
            INT2FIX(lpqscConf->dwTagId),
            v_service_start_name,
            v_dependencies,
            v_description,
            v_interactive,
            INT2FIX(svcArray[i].ServiceStatusProcess.dwProcessId),
            INT2FIX(svcArray[i].ServiceStatusProcess.dwServiceFlags)
         );
#else
         v_struct = rb_struct_new(v_service_struct,
            rb_str_new2(svcArray[i].lpServiceName),
            rb_str_new2(svcArray[i].lpDisplayName),
            v_service_type,
            v_current_state,
            v_controls_accepted,
            INT2FIX(svcArray[i].ServiceStatus.dwWin32ExitCode),
            INT2FIX(svcArray[i].ServiceStatus.dwServiceSpecificExitCode),
            INT2FIX(svcArray[i].ServiceStatus.dwCheckPoint),
            INT2FIX(svcArray[i].ServiceStatus.dwWaitHint),
            v_binary_path_name,
            v_start_type,
            v_error_control,
            v_load_order_group,
            INT2FIX(lpqscConf->dwTagId),
            v_service_start_name,
            v_dependencies,
            v_description,
            v_interactive
         );
#endif
         if(rb_block_given_p()){
            rb_yield(v_struct);
         }
         else{
            rb_ary_push(v_array, v_struct);
         }
      }
   }
   else{
      sprintf(error,"EnumServiceStatus() call failed: %s",
         ErrorDescription(GetLastError()));
      LocalFree(lpqscConf);
      LocalFree(lpqscDesc);
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   LocalFree(lpqscConf);
   LocalFree(lpqscDesc);
   CloseServiceHandle(hSCManager);
   return v_array; // Nil if a block was given
}

/*
 * call-seq:
 *    Service.stop(name, host=localhost)
 *
 * Stop a service.  Attempting to stop an already stopped service raises
 * a ServiceError.
 */
static VALUE service_stop(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager, hSCService;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   SERVICE_STATUS serviceStatus;
   VALUE v_service_name, v_machine_name;
   int rv;

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CONNECT
   );

   if(!hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      SERVICE_STOP
   );

   if(!hSCService){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   rv = ControlService(
      hSCService,
      SERVICE_CONTROL_STOP,
      &serviceStatus
   );

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCService);
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   CloseServiceHandle(hSCService);
   CloseServiceHandle(hSCManager);

   return klass;
}

/*
 * call-seq:
 *    Service.pause(name, host=localhost)
 *
 * Pause a service.  Attempting to pause an already paused service will raise
 * a ServiceError.
 *
 * Note that not all services are configured to accept a pause (or resume)
 * command.
 */
static VALUE service_pause(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager, hSCService;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   SERVICE_STATUS serviceStatus;
   VALUE v_service_name, v_machine_name;
   int rv;

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CONNECT
   );

   if(!hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      SERVICE_PAUSE_CONTINUE
   );

   if(!hSCService){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   rv = ControlService(
      hSCService,
      SERVICE_CONTROL_PAUSE,
      &serviceStatus
   );

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCService);
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   CloseServiceHandle(hSCService);
   CloseServiceHandle(hSCManager);

   return klass;
}

/*
 * call-seq:
 *    Service.resume(name, host=localhost)
 *
 * Resume a service.  Attempting to resume a service that isn't paused will
 * raise a ServiceError.
 *
 * Note that not all services are configured to accept a resume (or pause)
 * command.  In that case, a ServiceError will be raised.
 */
static VALUE service_resume(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager, hSCService;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   SERVICE_STATUS serviceStatus;
   VALUE v_service_name, v_machine_name;
   int rv;

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CONNECT
   );

   if(!hSCManager){
      rb_raise(cServiceError,ErrorDescription(GetLastError()));
   }

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      SERVICE_PAUSE_CONTINUE
   );

   if(!hSCService){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   rv = ControlService(
      hSCService,
      SERVICE_CONTROL_CONTINUE,
      &serviceStatus
   );

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCService);
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   CloseServiceHandle(hSCService);
   CloseServiceHandle(hSCManager);

   return klass;
}

/*
 * call-seq:
 *    Service.start(name, host=localhost, args=nil)
 *
 * Attempts to start service +name+ on +host+, or the local machine if no
 * host is provided.  If +args+ are provided, they are passed to the service's
 * Service_Main() function.
 *
 *-- Note that the WMI interface does not allow you to pass arguments to the
 *-- Service_Main function.
 */
static VALUE service_start(int argc, VALUE *argv, VALUE klass){
   SC_HANDLE hSCManager, hSCService;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   TCHAR** lpServiceArgVectors;
   VALUE v_service_name, v_machine_name, rbArgs;
   int rv;

   rb_scan_args(argc, argv, "11*", &v_service_name, &v_machine_name, &rbArgs);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   if( (NIL_P(rbArgs)) || (RARRAY(rbArgs)->len == 0) ){
      lpServiceArgVectors = NULL;
   }
   else{
      int i;
      lpServiceArgVectors =
         malloc(RARRAY(rbArgs)->len * sizeof(*lpServiceArgVectors));

      for(i = 0; i < RARRAY(rbArgs)->len; i++){
         VALUE v_tmp = rb_ary_entry(rbArgs, i);
         TCHAR* string = TEXT(StringValuePtr(v_tmp));
         lpServiceArgVectors[i] = malloc(*string);
         lpServiceArgVectors[i] = string;
      }
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CONNECT
   );

   if(!hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      SERVICE_START
   );

   if(!hSCService){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

   rv = StartService(
      hSCService,
      0,
      lpServiceArgVectors
   );

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      CloseServiceHandle(hSCService);
      if(lpServiceArgVectors){
         free(lpServiceArgVectors);
      }
      rb_raise(cServiceError,error);
   }

   CloseServiceHandle(hSCManager);
   CloseServiceHandle(hSCService);

   if(lpServiceArgVectors)
      free(lpServiceArgVectors);

   return klass;
}

/*
 * call-seq:
 *    Service.getservicename(display_name, host=localhost)
 *
 * Returns the service name for the corresponding +display_name+ on +host+, or
 * the local machine if no host is specified.
 */
static VALUE service_get_service_name(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager;
   TCHAR* lpMachineName;
   TCHAR* lpDisplayName;
   TCHAR szRegKey[MAX_PATH];
   DWORD dwKeySize = sizeof(szRegKey);
   VALUE v_machine_name, rbDisplayName;
   int rv;

   rb_scan_args(argc, argv, "11", &rbDisplayName, &v_machine_name);

   SafeStringValue(rbDisplayName);
   lpDisplayName = TEXT(StringValuePtr(rbDisplayName));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CONNECT
   );

   if(!hSCManager)
      rb_raise(rb_eArgError,ErrorDescription(GetLastError()));

   rv = GetServiceKeyName(
      hSCManager,
      lpDisplayName,
      szRegKey,
      &dwKeySize
   );

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(rb_eArgError,error);
   }

   CloseServiceHandle(hSCManager);

   return rb_str_new2(szRegKey);
}

/*
 * call-seq:
 *    Service.getdisplayname(service_name, host=localhost)
 *
 * Returns the display name for the service +service_name+ on +host+, or the
 * localhost if no host is specified.
 */
static VALUE service_get_display_name(int argc, VALUE *argv, VALUE klass)
{
   SC_HANDLE hSCManager;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   TCHAR szRegKey[MAX_PATH];
   DWORD dwKeySize = sizeof(szRegKey);
   VALUE v_machine_name, v_service_name;
   int rv;

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_CONNECT
   );

   if(!hSCManager)
      rb_raise(rb_eArgError,ErrorDescription(GetLastError()));

   rv = GetServiceDisplayName(
      hSCManager,
      lpServiceName,
      szRegKey,
      &dwKeySize
   );

   if(0 == rv){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(rb_eArgError,error);
   }

   CloseServiceHandle(hSCManager);

   return rb_str_new2(szRegKey);
}

/*
 * Sets the dependencies for the given service.  Use this when you call
 * Service.create_service, if desired.
 */
static VALUE service_set_dependencies(VALUE self, VALUE array)
{
   Check_Type(array, T_ARRAY);
   rb_iv_set(self, "@dependencies", array);
   return self;
}

/*
 * Returns an array of dependencies for the given service, or nil if there
 * aren't any dependencies.
 */
static VALUE service_get_dependencies(VALUE self){
   return rb_iv_get(self, "@dependencies");
}

/*
 * call-seq:
 *    Service.status(name, host=localhost)
 *
 * Returns a ServiceStatus struct indicating the status of service +name+ on
 * +host+, or the localhost if none is provided.
 *
 * The ServiceStatus struct contains the following members:
 *
 * * service_type
 * * current_state
 * * controls_accepted
 * * win32_exit_code
 * * service_specific_exit_code
 * * check_point
 * * wait_hint
 * * pid (Win2k or later)
 * * service_flags (Win2k or later)
 */
static VALUE service_status(int argc, VALUE *argv, VALUE klass){
   SC_HANDLE hSCManager, hSCService;
   VALUE v_service_name, v_machine_name;
   VALUE v_service_type, v_current_state, v_controls_accepted;
   VALUE v_interactive = Qfalse;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   DWORD dwBytesNeeded;
   int rv;

#ifdef HAVE_QUERYSERVICESTATUSEX
   SERVICE_STATUS_PROCESS ssProcess;
#else
   SERVICE_STATUS ssProcess;
#endif

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_ENUMERATE_SERVICE
   );

   if(!hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      SERVICE_QUERY_STATUS
   );

   if(!hSCService){
      strcpy(error,ErrorDescription(GetLastError()));
      CloseServiceHandle(hSCManager);
      rb_raise(cServiceError,error);
   }

#ifdef HAVE_QUERYSERVICESTATUSEX
   rv = QueryServiceStatusEx(
      hSCService,
      SC_STATUS_PROCESS_INFO,
      (LPBYTE)&ssProcess,
      sizeof(SERVICE_STATUS_PROCESS),
      &dwBytesNeeded
   );
#else
   rv = QueryServiceStatus(
      hSCService,
      &ssProcess
   );
#endif

   v_service_type = rb_get_service_type(ssProcess.dwServiceType);
   v_current_state = rb_get_current_state(ssProcess.dwCurrentState);
   v_controls_accepted = rb_get_controls_accepted(ssProcess.dwControlsAccepted);

   if(ssProcess.dwServiceType & SERVICE_INTERACTIVE_PROCESS){
      v_interactive = Qtrue;
   }

   CloseServiceHandle(hSCService);
   CloseServiceHandle(hSCManager);

   return rb_struct_new(v_service_status_struct,
      v_service_type,
      v_current_state,
      v_controls_accepted,
      INT2FIX(ssProcess.dwWin32ExitCode),
      INT2FIX(ssProcess.dwServiceSpecificExitCode),
      INT2FIX(ssProcess.dwCheckPoint),
      INT2FIX(ssProcess.dwWaitHint),
      v_interactive
#ifdef HAVE_QUERYSERVICESTATUSEX
      ,INT2FIX(ssProcess.dwProcessId)
      ,INT2FIX(ssProcess.dwServiceFlags)
#endif
   );
}

/* call-seq:
 *    Service.exists?(name, host=localhost)
 *
 * Returns whether or not the service +name+ exists on +host+, or the localhost
 * if none is provided.
 */
static VALUE service_exists(int argc, VALUE *argv, VALUE klass){
   SC_HANDLE hSCManager, hSCService;
   TCHAR* lpMachineName;
   TCHAR* lpServiceName;
   VALUE v_service_name, v_machine_name;
   VALUE rbExists = Qtrue;

   rb_scan_args(argc, argv, "11", &v_service_name, &v_machine_name);

   SafeStringValue(v_service_name);
   lpServiceName = TEXT(StringValuePtr(v_service_name));

   if(NIL_P(v_machine_name)){
      lpMachineName = NULL;
   }
   else{
      SafeStringValue(v_machine_name);
      lpMachineName = TEXT(StringValuePtr(v_machine_name));
   }

   hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      SC_MANAGER_ENUMERATE_SERVICE
   );

   if(!hSCManager)
      rb_raise(cServiceError,ErrorDescription(GetLastError()));

   hSCService = OpenService(
      hSCManager,
      lpServiceName,
      SERVICE_QUERY_STATUS
   );

   if(!hSCService)
      rbExists = Qfalse;

   CloseServiceHandle(hSCService);
   CloseServiceHandle(hSCManager);

   return rbExists;
}

/*
 * call-seq:
 * 	Service.open(service_name, host=nil, desired_access=nil)
 * 	Service.open(service_name, host=nil, desired_access=nil){ |svc| ... }
 *
 * Opens and returns a new Service object based on +service_name+ from +host+
 * or the local machine if no host is specified.  If a block is provided, the
 * object is automatically closed at the end of the block.
 *
 * Note that the default desired access for the returned object is
 * Service::SERVICE_QUERY_CONFIG.  You will probably need to change that in
 * order to configure or delete an existing service using the returned object.
 */
static VALUE service_open(int argc, VALUE* argv, VALUE klass){
   VALUE self, v_service_name, v_host, v_desired_access;
   SvcStruct* ptr;
   SC_HANDLE hSCService;
   TCHAR* lpMachineName;
   LPQUERY_SERVICE_CONFIG lpConf;
   LPSERVICE_DESCRIPTION lpDesc;
   DWORD dwDesiredAccess = SERVICE_QUERY_CONFIG;
   DWORD dwBytesNeeded, rv;

   rb_scan_args(argc, argv, "12", &v_service_name, &v_host, &v_desired_access);

   self = Data_Make_Struct(klass, SvcStruct, 0, service_free, ptr);

   /* Set the host name, or use the localhost if no host is specified */
   if(NIL_P(v_host)){
      TCHAR name[MAX_PATH];
      lpMachineName = NULL;

      if(gethostname(name, MAX_PATH))
         rb_raise(cServiceError, "gethostname() failed");

      v_host = rb_str_new2(name);
   }
   else{
      lpMachineName = TEXT(StringValuePtr(v_host));
   }

   /* Set the desired access level.  Default to SERVICE_QUERY_CONFIG */
   if(NIL_P(v_desired_access))
      v_desired_access = INT2FIX(SERVICE_QUERY_CONFIG);
   else
      dwDesiredAccess = NUM2INT(v_desired_access);

   ptr->hSCManager = OpenSCManager(
      lpMachineName,
      NULL,
      dwDesiredAccess
   );

   if(!ptr->hSCManager)
      rb_raise(cServiceError, ErrorDescription(GetLastError()));

   lpConf = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, MAX_BUF_SIZE);
   lpDesc = (LPSERVICE_DESCRIPTION) LocalAlloc(LPTR, MAX_BUF_SIZE);

   hSCService = OpenService(
      ptr->hSCManager,
      TEXT(StringValuePtr(v_service_name)),
      SERVICE_QUERY_CONFIG
   );

   if(!hSCService)
      rb_raise(cServiceError, ErrorDescription(GetLastError()));

   rv = QueryServiceConfig(
      hSCService,
      lpConf,
      MAX_BUF_SIZE,
      &dwBytesNeeded
   );

   if(0 == rv){
      sprintf(error, "QueryServiceConfig() call failed: %s",
         ErrorDescription(GetLastError()));
      CloseServiceHandle(ptr->hSCManager);
      rb_raise(cServiceError, error);
   }

   rv = QueryServiceConfig2(
      hSCService,
      SERVICE_CONFIG_DESCRIPTION,
      (LPBYTE)lpDesc,
      MAX_BUF_SIZE,
      &dwBytesNeeded
   );

   if(0 == rv){
      sprintf(error, "QueryServiceConfig2() call failed: %s",
         ErrorDescription(GetLastError()));
      CloseServiceHandle(ptr->hSCManager);
      rb_raise(cServiceError,error);
   }

   CloseServiceHandle(hSCService);

   /* Designer's note: the original plan to convert integer constants was
    * abandoned because methods like Service#configure expect a number.
    */
   rb_iv_set(self, "@machine_name", v_host);
   rb_iv_set(self, "@desired_access", v_desired_access);
   rb_iv_set(self, "@service_name", v_service_name);
   rb_iv_set(self, "@display_name", rb_str_new2(lpConf->lpDisplayName));
   rb_iv_set(self, "@service_type", INT2FIX(lpConf->dwServiceType));
   rb_iv_set(self, "@start_type", INT2FIX(lpConf->dwStartType));
   rb_iv_set(self, "@binary_path_name", rb_str_new2(lpConf->lpBinaryPathName));
   rb_iv_set(self, "@tag_id", INT2FIX(lpConf->dwTagId));
   rb_iv_set(self, "@start_name", rb_str_new2(lpConf->lpServiceStartName));
   rb_iv_set(self, "@service_description", rb_str_new2(lpDesc->lpDescription));
   rb_iv_set(self, "@error_control", INT2FIX(lpConf->dwErrorControl));

   if(lpConf->lpLoadOrderGroup){
      rb_iv_set(self, "@load_order_group",
         rb_str_new2(lpConf->lpLoadOrderGroup));
   }

   rb_iv_set(self, "@dependencies",
      rb_get_dependencies(lpConf->lpDependencies));

   if(rb_block_given_p()){
      rb_ensure(rb_yield, self, service_close, self);
      return Qnil;
   }
   else{
   	return self;
   }
}

void Init_service()
{
   VALUE mWin32, cService, cDaemon;
   int i = 0;

   // Modules and classes
   mWin32   = rb_define_module("Win32");
   cService = rb_define_class_under(mWin32, "Service", rb_cObject);
   cDaemon  = rb_define_class_under(mWin32, "Daemon", rb_cObject);
   cServiceError = rb_define_class_under(
      mWin32, "ServiceError", rb_eStandardError);
   cDaemonError = rb_define_class_under(
      mWin32, "DaemonError", rb_eStandardError);

   // Service class and instance methods
   rb_define_alloc_func(cService,service_allocate);
   rb_define_method(cService, "initialize", service_init, -1);
   rb_define_method(cService, "close", service_close, 0);
   rb_define_method(cService, "create_service", service_create, 0);
   rb_define_method(cService, "configure_service", service_configure, 0);

   // We do type checking for these two methods, so they're defined
   // indepedently.
   rb_define_method(cService, "dependencies=", service_set_dependencies, 1);
   rb_define_method(cService, "dependencies", service_get_dependencies, 0);

   rb_define_singleton_method(cService, "open", service_open, -1);
   rb_define_singleton_method(cService, "delete", service_delete, -1);
   rb_define_singleton_method(cService, "start", service_start, -1);
   rb_define_singleton_method(cService, "stop", service_stop, -1);
   rb_define_singleton_method(cService, "pause", service_pause, -1);
   rb_define_singleton_method(cService, "resume", service_resume, -1);
   rb_define_singleton_method(cService, "services", service_services, -1);
   rb_define_singleton_method(cService, "status", service_status, -1);
   rb_define_singleton_method(cService, "exists?", service_exists, -1);

   rb_define_singleton_method(cService, "getdisplayname",
      service_get_display_name, -1);

   rb_define_singleton_method(cService, "getservicename",
      service_get_service_name, -1);

   // Daemon class and instance methods
   rb_define_alloc_func(cDaemon, daemon_allocate);
   rb_define_method(cDaemon, "mainloop", daemon_mainloop, 0);
   rb_define_method(cDaemon, "state", daemon_state, 0);
   rb_define_method(cDaemon, "running?", daemon_is_running, 0);

   // Intialize critical section used by green polling thread
   InitializeCriticalSection(&csControlCode);

   // Constants
   rb_define_const(cService, "VERSION", rb_str_new2(WIN32_SERVICE_VERSION));
   rb_define_const(cDaemon, "VERSION", rb_str_new2(WIN32_SERVICE_VERSION));
   set_service_constants(cService);
   set_daemon_constants(cDaemon);

   // Structs
   v_service_status_struct = rb_struct_define("Win32ServiceStatus",
      "service_type", "current_state", "controls_accepted", "win32_exit_code",
      "service_specific_exit_code", "check_point", "wait_hint",
      "interactive"
#ifdef HAVE_QUERYSERVICESTATUSEX
      ,"pid", "service_flags"
#endif
      ,0);

   v_service_struct = rb_struct_define("Win32Service", "service_name",
      "display_name", "service_type", "current_state", "controls_accepted",
      "win32_exit_code", "service_specific_exit_code", "check_point",
      "wait_hint", "binary_path_name", "start_type", "error_control",
      "load_order_group", "tag_id", "start_name", "dependencies",
      "description", "interactive"
#ifdef HAVE_ENUMSERVICESSTATUSEX
      ,"pid", "service_flags"
#endif
   ,0);

   // Create an attr_accessor for each valid instance method
   for(i = 0; i < sizeof(keys)/sizeof(char*); i++){
      rb_define_attr(cService,keys[i],1,1);
   }
}