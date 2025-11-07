#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid_low, pid_high;
  
  printf(1, "\n=== Lock Test 2: Priority Inversion Demonstration ===\n\n");
  
  // Low priority process acquires lock
  pid_low = fork();
  if(pid_low == 0){
    int mypid = getpid();
    nice(mypid, 4);  // Set to lowest priority
    
    printf(1, "[Time 0] [PID %d, Nice=4 LOW] Starting\n", mypid);
    printf(1, "[Time 0] [PID %d, Nice=4 LOW] Acquiring lock 1...\n", mypid);
    lock(1);
    printf(1, "[Time 0] [PID %d, Nice=4 LOW] Lock 1 acquired\n", mypid);
    printf(1, "[Time 0] [PID %d, Nice=4 LOW] Working for 5 seconds...\n", mypid);
    
    sleep(500);  // Work for 5 seconds
    
    printf(1, "[Time 5] [PID %d, Nice=4 LOW] Releasing lock 1\n", mypid);
    release(1);
    printf(1, "[Time 5] [PID %d, Nice=4 LOW] Done\n", mypid);
    exit();
  }
  
  sleep(300);  // Wait 3 seconds
  
  // High priority process tries to acquire same lock
  pid_high = fork();
  if(pid_high == 0){
    int mypid = getpid();
    nice(mypid, 0);  // Set to highest priority
    
    printf(1, "[Time 3] [PID %d, Nice=0 HIGH] Starting\n", mypid);
    printf(1, "[Time 3] [PID %d, Nice=0 HIGH] Trying to acquire lock 1...\n", mypid);
    printf(1, "[Time 3] [PID %d, Nice=0 HIGH] (Will wait - priority inversion!)\n", mypid);
    
    lock(1);
    printf(1, "[Time 5+] [PID %d, Nice=0 HIGH] Lock 1 acquired after waiting\n", mypid);
    release(1);
    printf(1, "[Time 5+] [PID %d, Nice=0 HIGH] Done\n", mypid);
    exit();
  }
  
  wait();
  wait();
  
  printf(1, "\n=== Lock Test 2 Complete ===\n");
  printf(1, "Notice: High priority had to wait for low priority!\n\n");
  exit();
}