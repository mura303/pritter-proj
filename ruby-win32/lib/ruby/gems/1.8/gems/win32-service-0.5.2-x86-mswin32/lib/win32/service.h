#define WIN32_SERVICE_VERSION "0.5.2"

#define MAX_KEY_SIZE 24
#define MAX_SERVICES 1000
#define MAX_BUF_SIZE 4096

struct servicestruct{
   SC_HANDLE hSCManager;
};

typedef struct servicestruct SvcStruct;

static void service_free(SvcStruct *p){
   CloseServiceHandle(p->hSCManager);
   p->hSCManager = NULL;
   free(p);
}

// A list of valid keys (attributes) for the Service class.  Note that the
// 'dependencies' attribute is defined manually for type checking purposes
// so it is not included in this array.
char *keys[] = {
   "machine_name",
   "desired_access",
   "service_name",
   "display_name",
   "service_type",
   "start_type",
   "error_control",
   "tag_id",
   "binary_path_name",
   "load_order_group",
   "start_name",
   "password",
   "service_description"
};

// Return an error code as a string
LPTSTR ErrorDescription(DWORD p_dwError)
{
   HLOCAL hLocal = NULL;
   static TCHAR ErrStr[1024];
   int len;

   if (!(len=FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      p_dwError,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR)&hLocal,
      0,
      NULL)))
   {
      rb_raise(rb_eStandardError, "unable to format error message");
   }
   memset(ErrStr, 0, sizeof(ErrStr));
   strncpy(ErrStr, (LPTSTR)hLocal, len-2); // remove \r\n
   LocalFree(hLocal);
   return ErrStr;
}

static VALUE rb_get_dependencies(LPTSTR lpDependencies){
   VALUE v_dependencies = rb_ary_new();
   
   if(lpDependencies){
      TCHAR* pszDepend = 0;
      int i = 0;
      
      pszDepend = &lpDependencies[i];
 
      while(*pszDepend != 0){
         rb_ary_push(v_dependencies, rb_str_new2(pszDepend));
         i += _tcslen(lpDependencies) + 1;
         pszDepend = &lpDependencies[i];
      }    
   }
   
   if(RARRAY(v_dependencies)->len == 0)
      v_dependencies = Qnil;
   
   return v_dependencies;
}

static VALUE rb_get_error_control(DWORD dwErrorControl){
   VALUE v_error_control;
   switch(dwErrorControl){
      case SERVICE_ERROR_CRITICAL:
         v_error_control = rb_str_new2("critical");
         break;
      case SERVICE_ERROR_IGNORE:
         v_error_control = rb_str_new2("ignore");
         break;
      case SERVICE_ERROR_NORMAL:
         v_error_control = rb_str_new2("normal");
         break;
      case SERVICE_ERROR_SEVERE:
         v_error_control = rb_str_new2("severe");
         break;
      default:
         v_error_control = Qnil;  
         
   }
   
   return v_error_control;  
}

static VALUE rb_get_start_type(DWORD dwStartType){
   VALUE v_start_type;
   switch(dwStartType){
      case SERVICE_AUTO_START:
         v_start_type = rb_str_new2("auto start");
         break;
      case SERVICE_BOOT_START:
         v_start_type = rb_str_new2("boot start");
         break;
      case SERVICE_DEMAND_START:
         v_start_type = rb_str_new2("demand start");
         break;
      case SERVICE_DISABLED:
         v_start_type = rb_str_new2("disabled");
         break;
      case SERVICE_SYSTEM_START:
         v_start_type = rb_str_new2("system start");
         break;
      default:
         v_start_type = Qnil;
   }
   
   return v_start_type;
}

/* Helper function to retrieve the service type.  Note that some of these
 * values were not documented from the MSDN web site, but are listed in the
 * winnt.h header file.
 */
static VALUE rb_get_service_type(DWORD dwServiceType){
   VALUE rbServiceType;
   switch(dwServiceType){
      case SERVICE_FILE_SYSTEM_DRIVER:
         rbServiceType = rb_str_new2("filesystem driver");
         break;
      case SERVICE_KERNEL_DRIVER:
         rbServiceType = rb_str_new2("kernel driver");
         break;
      case SERVICE_WIN32_OWN_PROCESS:
         rbServiceType = rb_str_new2("own process");
         break;
      case SERVICE_WIN32_SHARE_PROCESS:
         rbServiceType = rb_str_new2("share process");
         break;
      /* There is some debate whether this is supposed to be 'RECOGNIZED' */
      case SERVICE_RECOGNIZER_DRIVER:
         rbServiceType = rb_str_new2("recognizer driver");
         break;
      case SERVICE_DRIVER:
         rbServiceType = rb_str_new2("driver");
         break;
      case SERVICE_WIN32:
         rbServiceType = rb_str_new2("win32");
         break;
      case SERVICE_TYPE_ALL:
         rbServiceType = rb_str_new2("all");
         break;
      case (SERVICE_INTERACTIVE_PROCESS | SERVICE_WIN32_OWN_PROCESS):
         rbServiceType = rb_str_new2("own process, interactive");
         break;
      case (SERVICE_INTERACTIVE_PROCESS | SERVICE_WIN32_SHARE_PROCESS):
         rbServiceType = rb_str_new2("share process, interactive");
         break;
      default:
         rbServiceType = Qnil;
   }

   return rbServiceType;
}

static VALUE rb_get_current_state(DWORD dwCurrentState){
   VALUE rbCurrentState;
   switch(dwCurrentState){
      case SERVICE_CONTINUE_PENDING:
         rbCurrentState = rb_str_new2("continue pending");
         break;
      case SERVICE_PAUSE_PENDING:
         rbCurrentState = rb_str_new2("pause pending");
         break;
      case SERVICE_PAUSED:
         rbCurrentState = rb_str_new2("paused");
         break;
      case SERVICE_RUNNING:
         rbCurrentState = rb_str_new2("running");
         break;
      case SERVICE_START_PENDING:
         rbCurrentState = rb_str_new2("start pending");
         break;
      case SERVICE_STOP_PENDING:
         rbCurrentState = rb_str_new2("stop pending");
         break;
      case SERVICE_STOPPED:
         rbCurrentState = rb_str_new2("stopped");
         break;
      default:
         rbCurrentState = Qnil;
   }

   return rbCurrentState;
}

static VALUE rb_get_controls_accepted(DWORD dwControlsAccepted){
   VALUE rbControlsAccepted = rb_ary_new();
   if(dwControlsAccepted & SERVICE_ACCEPT_NETBINDCHANGE){
      rb_ary_push(rbControlsAccepted,rb_str_new2("netbind change"));
   }

   if(dwControlsAccepted & SERVICE_ACCEPT_PARAMCHANGE){
      rb_ary_push(rbControlsAccepted,rb_str_new2("param change"));
   }

   if(dwControlsAccepted & SERVICE_PAUSE_CONTINUE){
      rb_ary_push(rbControlsAccepted,rb_str_new2("pause continue"));
   }

   if(dwControlsAccepted & SERVICE_ACCEPT_SHUTDOWN){
      rb_ary_push(rbControlsAccepted,rb_str_new2("shutdown"));
   }

   if(dwControlsAccepted & SERVICE_ACCEPT_STOP){
      rb_ary_push(rbControlsAccepted,rb_str_new2("stop"));
   }

   if(RARRAY(rbControlsAccepted)->len == 0){
      rbControlsAccepted = Qnil;
   }

   return rbControlsAccepted;
}

static void set_service_constants(VALUE klass)
{
   // Desired Access Flags
   rb_define_const(klass, "MANAGER_ALL_ACCESS",
      INT2NUM(SC_MANAGER_ALL_ACCESS));

   rb_define_const(klass, "MANAGER_CREATE_SERVICE",
      INT2NUM(SC_MANAGER_CREATE_SERVICE));

   rb_define_const(klass, "MANAGER_CONNECT",
      INT2NUM(SC_MANAGER_CONNECT));

   rb_define_const(klass, "MANAGER_ENUMERATE_SERVICE",
      INT2NUM(SC_MANAGER_ENUMERATE_SERVICE));

   rb_define_const(klass, "MANAGER_LOCK",
      INT2NUM(SC_MANAGER_LOCK));

#ifdef SC_MANAGER_BOOT_CONFIG
   rb_define_const(klass, "MANAGER_BOOT_CONFIG",
      INT2NUM(SC_MANAGER_BOOT_CONFIG));
#endif
 
   rb_define_const(klass, "MANAGER_QUERY_LOCK_STATUS",
      INT2NUM(SC_MANAGER_QUERY_LOCK_STATUS));
   
   /* Service specific access flags */   
   rb_define_const(klass, "ALL_ACCESS", INT2NUM(SERVICE_ALL_ACCESS));
   rb_define_const(klass, "CHANGE_CONFIG", INT2NUM(SERVICE_CHANGE_CONFIG));
   
   rb_define_const(klass, "ENUMERATE_DEPENDENTS",
      INT2NUM(SERVICE_ENUMERATE_DEPENDENTS));
      
   rb_define_const(klass, "INTERROGATE", INT2NUM(SERVICE_INTERROGATE));
   rb_define_const(klass, "PAUSE_CONTINUE", INT2NUM(SERVICE_PAUSE_CONTINUE));
   rb_define_const(klass, "QUERY_CONFIG", INT2NUM(SERVICE_QUERY_CONFIG));
   rb_define_const(klass, "QUERY_STATUS", INT2NUM(SERVICE_QUERY_STATUS));
   rb_define_const(klass, "STOP", INT2NUM(SERVICE_STOP));
   rb_define_const(klass, "START", INT2NUM(SERVICE_START));
   
   rb_define_const(klass, "USER_DEFINED_CONTROL",
      INT2NUM(SERVICE_USER_DEFINED_CONTROL));

   // Service Type
   rb_define_const(klass, "FILE_SYSTEM_DRIVER",
      INT2NUM(SERVICE_FILE_SYSTEM_DRIVER));

   rb_define_const(klass, "KERNEL_DRIVER",
      INT2NUM(SERVICE_KERNEL_DRIVER));

   rb_define_const(klass, "WIN32_OWN_PROCESS",
      INT2NUM(SERVICE_WIN32_OWN_PROCESS));

   rb_define_const(klass, "WIN32_SHARE_PROCESS",
      INT2NUM(SERVICE_WIN32_SHARE_PROCESS));

   rb_define_const(klass, "INTERACTIVE_PROCESS",
      INT2NUM(SERVICE_INTERACTIVE_PROCESS));

   // Start Type
   rb_define_const(klass, "AUTO_START",
      INT2NUM(SERVICE_AUTO_START));

   rb_define_const(klass, "BOOT_START",
      INT2NUM(SERVICE_BOOT_START));

   rb_define_const(klass, "DEMAND_START",
      INT2NUM(SERVICE_DEMAND_START));

   rb_define_const(klass, "DISABLED",
      INT2NUM(SERVICE_DISABLED));

   rb_define_const(klass, "SYSTEM_START",
      INT2NUM(SERVICE_SYSTEM_START));

   // Error Control
   rb_define_const(klass, "ERROR_IGNORE",
      INT2NUM(SERVICE_ERROR_IGNORE));

   rb_define_const(klass, "ERROR_NORMAL",
      INT2NUM(SERVICE_ERROR_NORMAL));

   rb_define_const(klass, "ERROR_SEVERE",
      INT2NUM(SERVICE_ERROR_SEVERE));

   rb_define_const(klass, "ERROR_CRITICAL",
      INT2NUM(SERVICE_ERROR_CRITICAL));

   // Service Status
   rb_define_const(klass, "CONTINUE_PENDING",
      INT2NUM(SERVICE_CONTINUE_PENDING));

   rb_define_const(klass, "PAUSE_PENDING",
      INT2NUM(SERVICE_PAUSE_PENDING));

   rb_define_const(klass, "PAUSED",
      INT2NUM(SERVICE_PAUSED));

   rb_define_const(klass, "RUNNING",
      INT2NUM(SERVICE_RUNNING));

   rb_define_const(klass, "START_PENDING",
      INT2NUM(SERVICE_START_PENDING));

   rb_define_const(klass, "STOP_PENDING",
      INT2NUM(SERVICE_STOP_PENDING));

   rb_define_const(klass, "STOPPED",
      INT2NUM(SERVICE_STOPPED));

   // Service Control Signals
   rb_define_const(klass, "CONTROL_STOP",
      INT2NUM(SERVICE_CONTROL_STOP));
      
   rb_define_const(klass, "CONTROL_PAUSE",
      INT2NUM(SERVICE_CONTROL_PAUSE));
      
   rb_define_const(klass, "CONTROL_CONTINUE",
      INT2NUM(SERVICE_CONTROL_CONTINUE));
      
   rb_define_const(klass, "CONTROL_INTERROGATE",
      INT2NUM(SERVICE_CONTROL_INTERROGATE));
      
   rb_define_const(klass, "CONTROL_SHUTDOWN",
      INT2NUM(SERVICE_CONTROL_SHUTDOWN));

#ifdef SERVICE_CONTROL_PARAMCHANGE      
   rb_define_const(klass, "CONTROL_PARAMCHANGE",
      INT2NUM(SERVICE_CONTROL_PARAMCHANGE));
#endif

#ifdef SERVICE_CONTROL_NETBINDADD      
   rb_define_const(klass, "CONTROL_NETBINDADD",
      INT2NUM(SERVICE_CONTROL_NETBINDADD));
#endif

#ifdef SERVICE_CONTROL_NETBINDREMOVE      
   rb_define_const(klass, "CONTROL_NETBINDREMOVE",
      INT2NUM(SERVICE_CONTROL_NETBINDREMOVE));
#endif

#ifdef SERVICE_CONTROL_NETBINDENABLE      
   rb_define_const(klass, "CONTROL_NETBINDENABLE",
      INT2NUM(SERVICE_CONTROL_NETBINDENABLE));
#endif

#ifdef SERVICE_CONTROL_NETBINDDISABLE      
   rb_define_const(klass, "CONTROL_NETBINDDISABLE",
      INT2NUM(SERVICE_CONTROL_NETBINDDISABLE));
#endif
}

// The Daemon class only needs a subset of the Service constants
void set_daemon_constants(VALUE klass){
   rb_define_const(klass, "CONTINUE_PENDING",
      INT2NUM(SERVICE_CONTINUE_PENDING));

   rb_define_const(klass, "PAUSE_PENDING",
      INT2NUM(SERVICE_PAUSE_PENDING));

   rb_define_const(klass, "PAUSED",
      INT2NUM(SERVICE_PAUSED));

   rb_define_const(klass, "RUNNING",
      INT2NUM(SERVICE_RUNNING));

   rb_define_const(klass, "START_PENDING",
      INT2NUM(SERVICE_START_PENDING));

   rb_define_const(klass, "STOP_PENDING",
      INT2NUM(SERVICE_STOP_PENDING));

   rb_define_const(klass, "STOPPED",
      INT2NUM(SERVICE_STOPPED));
      
   rb_define_const(klass, "IDLE", INT2NUM(0));  
}

