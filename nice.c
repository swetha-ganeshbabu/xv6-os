#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid, value, old_value;
  
  if(argc < 2) {
    printf(2, "Usage: nice <pid> <value> OR nice <value>\n");
    exit();
  }
  
  if(argc == 2) {
    // Format: nice <value>
    // Change nice value for current process
    
    // Check for negative value in string
    if(argv[1][0] == '-') {
      printf(2, "Error: nice value must be between 0 and 4\n");
      exit();
    }
    
    value = atoi(argv[1]);
    pid = getpid();
  } else {
    // Format: nice <pid> <value>
    
    // Check for negative value in string
    if(argv[2][0] == '-') {
      printf(2, "Error: nice value must be between 0 and 4\n");
      exit();
    }
    
    pid = atoi(argv[1]);
    value = atoi(argv[2]);
  }
  
  // Validate value
  if(value < 0 || value > 4) {
    printf(2, "Error: nice value must be between 0 and 4\n");
    exit();
  }
  
  old_value = nice(pid, value);
  
  if(old_value < 0) {
    printf(2, "Error: Could not set nice value (invalid PID or value)\n");
    exit();
  }
  
  // Output format: <pid> <old_value>
  printf(1, "%d %d\n", pid, old_value);
  
  exit();
}