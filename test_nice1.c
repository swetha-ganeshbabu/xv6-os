#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid = getpid();
  int old_value;
  
  printf(1, "\n=== Test 1: Basic Nice Value Changes ===\n");
  printf(1, "Current PID: %d\n\n", pid);
  
  // Test changing to priority 0 (highest)
  printf(1, "Setting nice value to 0 (highest priority)...\n");
  old_value = nice(pid, 0);
  printf(1, "  PID: %d, Old value: %d, New value: 0\n\n", pid, old_value);
  
  // Test changing to priority 4 (lowest)
  printf(1, "Setting nice value to 4 (lowest priority)...\n");
  old_value = nice(pid, 4);
  printf(1, "  PID: %d, Old value: %d, New value: 4\n\n", pid, old_value);
  
  // Test changing to priority 2 (medium)
  printf(1, "Setting nice value to 2 (medium priority)...\n");
  old_value = nice(pid, 2);
  printf(1, "  PID: %d, Old value: %d, New value: 2\n\n", pid, old_value);
  
  printf(1, "=== Test 1 Complete ===\n\n");
  exit();
}