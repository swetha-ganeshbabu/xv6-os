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
  int pid1, pid2, pid3;
  
  print_header("test_nice3 - Multiple Processes with Different Nice Values");
  
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
  
  printf(1, "\n========================================\n");
  printf(1, "Test 3 Complete\n");
  printf(1, "End Time: %d ticks\n", uptime());
  printf(1, "========================================\n\n");
  
  exit();
}