#include "types.h"
#include "stat.h"
#include "user.h"

// Check if a number is prime
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
  int i;
  int count = 0;
  int limit = 5000;  // Check numbers up to 5000
  
  printf(1, "PID %d: Starting prime calculation...\n", getpid());
  
  for(i = 2; i < limit; i++){
    if(isprime(i)){
      count++;
    }
  }
  
  printf(1, "PID %d: Found %d primes up to %d\n", getpid(), count, limit);
  exit();
}