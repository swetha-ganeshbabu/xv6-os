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
  int pid_low, pid_high;
  
  print_header("locktest3 - Priority Inheritance");
  printf(1, "This test shows priority inheritance in action:\n");
  printf(1, "- Low priority process holds lock\n");
  printf(1, "- High priority process waits for lock\n");
  printf(1, "- Low priority gets boosted to high priority\n");
  printf(1, "- After release, priority is restored\n\n");
  
  pid_low = fork();
  if(pid_low == 0){
    int mypid = getpid();
    nice(mypid, 4);
    
    printf(1, "[PID %d] Original priority: Nice=4 (LOW)\n", mypid);
    printf(1, "[PID %d] Acquiring lock 1...\n", mypid);
    lock(1);
    printf(1, "[PID %d] Lock 1 acquired\n", mypid);
    printf(1, "[PID %d] Working with lock...\n", mypid);
    
    sleep(500);
    
    printf(1, "[PID %d] Releasing lock 1\n", mypid);
    printf(1, "[PID %d] Priority should be restored to Nice=4\n", mypid);
    release(1);
    printf(1, "[PID %d] Lock released, priority restored\n", mypid);
    exit();
  }
  
  sleep(200);
  
  pid_high = fork();
  if(pid_high == 0){
    int mypid = getpid();
    nice(mypid, 0);
    
    printf(1, "[PID %d] High priority process (Nice=0) waiting for lock...\n", mypid);
    printf(1, "[PID %d] This should boost the lock holder's priority!\n", mypid);
    
    lock(1);
    printf(1, "[PID %d] Lock acquired\n", mypid);
    release(1);
    printf(1, "[PID %d] Done\n", mypid);
    exit();
  }
  
  wait();
  wait();
  
  printf(1, "\n========================================\n");
  printf(1, "Lock Test 3 Complete\n");
  printf(1, "End Time: %d ticks\n", uptime());
  printf(1, "========================================\n\n");
  exit();
}