#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"    

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;
extern struct lock_t resource_locks[NLOCKS];


int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_nice(void)
{
  int pid, value;
  struct proc *p;
  int old_nice;
  
  // Get arguments
  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &value) < 0)
    return -1;
    
  // Validate nice value (0-4)
  if(value < 0 || value > 4) {
    return -1;
  }
  
  // Find the process
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      old_nice = p->nice;
      p->nice = value;
      release(&ptable.lock);
      return old_nice;  // Return old nice value
    }
  }
  release(&ptable.lock);
  
  return -1;  // Process not found
}
int
sys_lock(void)
{
  int lock_id;
  int idx;
  int holder_pid;
  struct proc *p;
  struct proc *holder;
  
  p = myproc();
  
  if(argint(0, &lock_id) < 0)
    return -1;
  
  if(lock_id < 1 || lock_id > NLOCKS)
    return -1;
  
  idx = lock_id - 1;
  
  acquire(&ptable.lock);
  
  while(resource_locks[idx].locked) {
    // Lock is held - implement priority inheritance
    holder_pid = resource_locks[idx].holder_pid;
    
    // Find the holder process and boost priority if needed
    for(holder = ptable.proc; holder < &ptable.proc[NPROC]; holder++) {
      if(holder->pid == holder_pid) {
        // Only boost if waiter has higher priority AND holder hasn't been boosted yet
        if(p->nice < holder->nice && !holder->has_inherited_priority) {
          holder->original_nice = holder->nice;  // Save current nice (could be user-set)
          holder->nice = p->nice;                // Boost to waiter's priority
          holder->has_inherited_priority = 1;    // Mark as inherited
        }
        break;
      }
    }
    
    // Sleep waiting for the lock
    sleep(&resource_locks[idx], &ptable.lock);
  }
  
  // Lock is now free, acquire it
  resource_locks[idx].locked = 1;
  resource_locks[idx].holder_pid = p->pid;
  p->holding_lock = lock_id;
  
  release(&ptable.lock);
  return 0;
}
int
sys_release(void)
{
  int lock_id;
  int idx;
  struct proc *p;
  
  p = myproc();
  
  if(argint(0, &lock_id) < 0)
    return -1;
  
  if(lock_id < 1 || lock_id > NLOCKS)
    return -1;
  
  idx = lock_id - 1;
  
  acquire(&ptable.lock);
  
  if(resource_locks[idx].holder_pid != p->pid) {
    release(&ptable.lock);
    return -1;
  }
  
  // Release the lock
  resource_locks[idx].locked = 0;
  resource_locks[idx].holder_pid = -1;
  p->holding_lock = -1;
  
  // ONLY restore priority if it was inherited (not user-set)
  if(p->has_inherited_priority) {
    p->nice = p->original_nice;
    p->has_inherited_priority = 0;  // Clear the flag
  }
  // If has_inherited_priority is 0, the nice value was set by user, don't touch it!
  
  // Wake up processes waiting
  wakeup(&resource_locks[idx]);
  
  release(&ptable.lock);
  
  return 0;
}