#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid1, pid2;
  
  printf(1, "\n=== Lock Test 1: Basic Lock/Unlock ===\n\n");
  
  // Process 1 - acquires lock 1
  pid1 = fork();
  if(pid1 == 0){
    int mypid = getpid();
    printf(1, "[PID %d] Attempting to acquire lock 1...\n", mypid);
    
    if(lock(1) == 0) {
      printf(1, "[PID %d] Successfully acquired lock 1\n", mypid);
      printf(1, "[PID %d] Holding lock for 3 seconds...\n", mypid);
      sleep(300);  // Hold for 3 seconds
      
      printf(1, "[PID %d] Releasing lock 1\n", mypid);
      release(1);
      printf(1, "[PID %d] Lock 1 released\n", mypid);
    } else {
      printf(1, "[PID %d] Failed to acquire lock 1\n", mypid);
    }
    exit();
  }
  
  sleep(50);  // Let first process get the lock
  
  // Process 2 - tries to acquire same lock
  pid2 = fork();
  if(pid2 == 0){
    int mypid = getpid();
    printf(1, "[PID %d] Attempting to acquire lock 1...\n", mypid);
    printf(1, "[PID %d] (Should wait since lock is held)\n", mypid);
    
    if(lock(1) == 0) {
      printf(1, "[PID %d] Successfully acquired lock 1 after waiting\n", mypid);
      printf(1, "[PID %d] Releasing lock 1\n", mypid);
      release(1);
    } else {
      printf(1, "[PID %d] Failed to acquire lock 1\n", mypid);
    }
    exit();
  }
  
  wait();
  wait();
  
  printf(1, "\n=== Lock Test 1 Complete ===\n\n");
  exit();
}