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
  
  print_header("schedulertest1 - Three Processes with Different Priorities");
  printf(1, "Expected: Process with nice=0 should print most primes\n");
  printf(1, "          Process with nice=4 should print least primes\n\n");
  
  // High priority child (nice = 0)
  pid1 = fork();
  if(pid1 == 0){
    int mypid = getpid();
    nice(mypid, 0);  // Highest priority
    printf(1, "PID %d: Priority 0 (HIGH) - Starting...\n", mypid);
    
    int count = 0;
    for(int i = 2; i < 10000; i++){
      if(isprime(i)) count++;
    }
    printf(1, "PID %d: Priority 0 - Found %d primes\n", mypid, count);
    exit();
  }
  
  // Medium priority child (nice = 2)
  pid2 = fork();
  if(pid2 == 0){
    int mypid = getpid();
    nice(mypid, 2);  // Medium priority
    printf(1, "PID %d: Priority 2 (MEDIUM) - Starting...\n", mypid);
    
    int count = 0;
    for(int i = 2; i < 10000; i++){
      if(isprime(i)) count++;
    }
    printf(1, "PID %d: Priority 2 - Found %d primes\n", mypid, count);
    exit();
  }
  
  // Low priority child (nice = 4)
  pid3 = fork();
  if(pid3 == 0){
    int mypid = getpid();
    nice(mypid, 4);  // Lowest priority
    printf(1, "PID %d: Priority 4 (LOW) - Starting...\n", mypid);
    
    int count = 0;
    for(int i = 2; i < 10000; i++){
      if(isprime(i)) count++;
    }
    printf(1, "PID %d: Priority 4 - Found %d primes\n", mypid, count);
    exit();
  }
  
  // Parent waits for all children
  wait();
  wait();
  wait();
  
  printf(1, "\n========================================\n");
  printf(1, "Test Complete\n");
  printf(1, "End Time: %d ticks\n", uptime());
  printf(1, "Note: Higher priority processes should complete first\n");
  printf(1, "========================================\n\n");
  exit();
}