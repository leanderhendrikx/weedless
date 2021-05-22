#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include "value.h"

typedef int (*ptrace_ptr_t)(int _request, pid_t _pid, caddr_t _addr, int _data);

int main (int argc, char* argv[]) 
{
   /* Example: strlen logging
    * Hooks strlen and logs all calls to console.  
    */ 
  const char* input = "abcdefg";
  printf("strlen(\"%s\") -> %lx\n", input, strlen(input)); 

   /* Example: changing return values. 
    * Hooks GetValue() returns 0 if hooked, 42 if not.
    */ 
  printf("GetValue() -> %d\n", GetValue());

   /* Example: disable debugger prevention. 
    * Hooks ptrace() and reimplements it to do nothing.
    */ 
  printf("Running ptrace anti_dbg\n");
  ptrace_ptr_t ptrace_ptr = (ptrace_ptr_t)dlsym(RTLD_SELF, "ptrace");
  ptrace_ptr(31, 0, 0, 0); // PTRACE_DENY_ATTACH = 31

  return 0;
}
