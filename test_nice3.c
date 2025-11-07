#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid1, pid2, pid3;
  
  printf(1, "\n=== Test 3: Multiple Processes with Different Nice Values ===\n\n");
  
  pid1 = fork();
  if(pid1 == 0) {
    // Child 1 - high priority
    int mypid = getpid();
    printf(1, "Child 1 (PID %d): Setting nice to 0 (high priority)\n", mypid);
    nice(mypid, 0);
    sleep(50);  // Do some work
    printf(1, "Child 1 (PID %d): Exiting\n", mypid);
    exit();
  }
  
  pid2 = fork();
  if(pid2 == 0) {
    // Child 2 - medium priority
    int mypid = getpid();
    printf(1, "Child 2 (PID %d): Setting nice to 2 (medium priority)\n", mypid);
    nice(mypid, 2);
    sleep(50);  // Do some work
    printf(1, "Child 2 (PID %d): Exiting\n", mypid);
    exit();
  }
  
  pid3 = fork();
  if(pid3 == 0) {
    // Child 3 - low priority
    int mypid = getpid();
    printf(1, "Child 3 (PID %d): Setting nice to 4 (low priority)\n", mypid);
    nice(mypid, 4);
    sleep(50);  // Do some work
    printf(1, "Child 3 (PID %d): Exiting\n", mypid);
    exit();
  }
  
  // Parent waits for all children
  wait();
  wait();
  wait();
  
  printf(1, "\n=== Test 3 Complete ===\n\n");
  exit();
}