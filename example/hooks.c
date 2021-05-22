#include <stdio.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <os/log.h>

int ptrace(int _request, pid_t _pid, caddr_t _addr, int _data) {
  printf("hooked_ptrace() returning 0!\n"); 
  return 0;
}

int GetValue(void) {
  return 0;
}

unsigned long (*orig_strlen)(const char* str);
unsigned long strlen(const char* str) {
  os_log(os_log_create("test", "LOGGER"), "strlen(\"%s\")\n", str);
   
  void* handle = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
  orig_strlen = dlsym(handle, "strlen");
  
  return orig_strlen(str);
}

