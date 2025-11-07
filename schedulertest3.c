#include "types.h"
#include "stat.h"
#include "user.h"

int
isprime(int n)
{
  int i;
  if(n < 2) return 0;
  if(n == 2) return 1;
  if(n % 2 == 0) return 0;
  
  for(i = 3; i * i <= n; i += 2){
    if(n % i == 0)
      return 0;
  }
  return 1;
}

int
main(int argc, char *argv[])
{
  int pid1, pid2, pid3;
  
  printf(1, "\n=== Scheduler Test 3: Progress Tracking ===\n");
  printf(1, "Watching which process makes more progress\n\n");
  
  // Nice value 0 (highest priority)
  pid1 = fork();
  if(pid1 == 0){
    int mypid = getpid();
    nice(mypid, 0);
    
    int progress = 0;
    for(int i = 2; i < 8000; i++){
      if(isprime(i)){
        progress++;
        if(progress % 100 == 0){
          printf(1, "[PID %d, Nice=0] Progress: %d primes\n", mypid, progress);
        }
      }
    }
    printf(1, "[PID %d, Nice=0] FINAL: %d primes\n", mypid, progress);
    exit();
  }
  
  // Nice value 2 (medium priority)
  pid2 = fork();
  if(pid2 == 0){
    int mypid = getpid();
    nice(mypid, 2);
    
    int progress = 0;
    for(int i = 2; i < 8000; i++){
      if(isprime(i)){
        progress++;
        if(progress % 100 == 0){
          printf(1, "[PID %d, Nice=2] Progress: %d primes\n", mypid, progress);
        }
      }
    }
    printf(1, "[PID %d, Nice=2] FINAL: %d primes\n", mypid, progress);
    exit();
  }
  
  // Nice value 4 (lowest priority)
  pid3 = fork();
  if(pid3 == 0){
    int mypid = getpid();
    nice(mypid, 4);
    
    int progress = 0;
    for(int i = 2; i < 8000; i++){
      if(isprime(i)){
        progress++;
        if(progress % 100 == 0){
          printf(1, "[PID %d, Nice=4] Progress: %d primes\n", mypid, progress);
        }
      }
    }
    printf(1, "[PID %d, Nice=4] FINAL: %d primes\n", mypid, progress);
    exit();
  }
  
  wait();
  wait();
  wait();
  
  printf(1, "\n=== Test Complete ===\n");
  printf(1, "Higher priority (Nice=0) should show more progress updates\n\n");
  exit();
}