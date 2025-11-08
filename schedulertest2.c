#include "types.h"
#include "stat.h"
#include "user.h"

void print_header(char* testname) {
    printf(1, "\n========================================\n");
    printf(1, "User: swetha-ganeshbabu\n");
    printf(1, "Test: %s\n", testname);
    printf(1, "PID: %d\n", getpid());
    printf(1, "Start Time: %d ticks\n", uptime());
    printf(1, "========================================\n\n");
}

int
main(int argc, char *argv[])
{
  int pid1, pid2;
  
  print_header("schedulertest2 - CPU Time Distribution");
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
  
  printf(1, "\n========================================\n");
  printf(1, "Test Complete\n");
  printf(1, "End Time: %d ticks\n", uptime());
  printf(1, "========================================\n\n");
  exit();
}