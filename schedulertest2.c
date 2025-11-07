#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid1, pid2;
  
  printf(1, "\n=== Scheduler Test 2: CPU Time Distribution ===\n");
  printf(1, "Two processes counting - higher priority should count more\n\n");
  
  // High priority process
  pid1 = fork();
  if(pid1 == 0){
    int mypid = getpid();
    nice(mypid, 0);  // High priority
    
    int counter = 0;
    int i;
    for(i = 0; i < 1000000; i++){
      counter++;
    }
    
    printf(1, "PID %d (Priority 0): Counted to %d\n", mypid, counter);
    exit();
  }
  
  // Low priority process
  pid2 = fork();
  if(pid2 == 0){
    int mypid = getpid();
    nice(mypid, 4);  // Low priority
    
    int counter = 0;
    int i;
    for(i = 0; i < 1000000; i++){
      counter++;
    }
    
    printf(1, "PID %d (Priority 4): Counted to %d\n", mypid, counter);
    exit();
  }
  
  wait();
  wait();
  
  printf(1, "\n=== Test Complete ===\n\n");
  exit();
}