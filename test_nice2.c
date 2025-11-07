#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid = getpid();
  int result;
  
  printf(1, "\n=== Test 2: Edge Cases ===\n\n");
  
  // Test invalid nice value (too high)
  printf(1, "Test 1: Invalid value 5 (out of range)...\n");
  result = nice(pid, 5);
  if(result < 0) {
    printf(1, "  PASS - Rejected invalid value\n\n");
  } else {
    printf(1, "  FAIL - Accepted invalid value\n\n");
  }
  
  // Test invalid nice value (negative)
  printf(1, "Test 2: Invalid value -1 (negative)...\n");
  result = nice(pid, -1);
  if(result < 0) {
    printf(1, "  PASS - Rejected negative value\n\n");
  } else {
    printf(1, "  FAIL - Accepted negative value\n\n");
  }
  
  // Test invalid PID
  printf(1, "Test 3: Invalid PID 9999...\n");
  result = nice(9999, 2);
  if(result < 0) {
    printf(1, "  PASS - Rejected invalid PID\n\n");
  } else {
    printf(1, "  FAIL - Accepted invalid PID\n\n");
  }
  
  // Test boundary values
  printf(1, "Test 4: Boundary value 0 (minimum)...\n");
  result = nice(pid, 0);
  if(result >= 0) {
    printf(1, "  PASS - Accepted minimum value\n\n");
  } else {
    printf(1, "  FAIL - Rejected valid minimum value\n\n");
  }
  
  printf(1, "Test 5: Boundary value 4 (maximum)...\n");
  result = nice(pid, 4);
  if(result >= 0) {
    printf(1, "  PASS - Accepted maximum value\n\n");
  } else {
    printf(1, "  FAIL - Rejected valid maximum value\n\n");
  }
  
  printf(1, "=== Test 2 Complete ===\n\n");
  exit();
}