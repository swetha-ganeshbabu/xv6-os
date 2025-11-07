# xv6 Priority-Based Scheduling with Priority Inheritance
## Operating Systems Homework 3

**Student Names:** Swetha Ganesh Babu, Sriram Madhiyalagan
**Net IDs:** sg8554, sm12155

---

## Compilation Instructions

To compile and run xv6:
```bash
make clean
make qemu-nox
```

This will boot xv6 in text-only mode. To exit xv6, press `Ctrl+A` then `X`.

---

## Task 1: Nice System Call 

### Implementation
- Modified `proc.h` to add `nice` field to process structure
- Implemented `sys_nice()` system call in `sysproc.c`
- Created `nice.c` user program for setting process priorities
- Nice values range from 0-4 (0 = highest priority, 4 = lowest priority)

### Test

After booting xv6, run these commands:

#### Basic Usage:
```bash
nice 1 3        # Set process 1 to nice value 3
nice 2          # Set current process to nice value 2
```

#### Test Programs:
```bash
test_nice1      # Basic nice value changes (0→4→2)
test_nice2      # Edge cases (invalid values, invalid PIDs)
test_nice3      # Multiple processes with different priorities
```

### Expected Results
- `test_nice1`: Shows nice values changing successfully
- `test_nice2`: All 5 tests should PASS (rejects invalid values/PIDs)
- `test_nice3`: Three processes run with different nice values

---

## Task 2: Priority Scheduler 

### Implementation
- Modified `scheduler()` function in `proc.c`
- Scheduler now selects process with **lowest nice value** (highest priority) first
- Processes with same priority use round-robin scheduling

### Test

After booting xv6, run these commands:
```bash
schedulertest1  # Three processes with different priorities computing primes
schedulertest2  # CPU time distribution between high/low priority
schedulertest3  # Progress tracking showing priority order
```

### Expected Results
- Processes with **nice=0** (high priority) complete **first**
- Processes with **nice=4** (low priority) complete **last**
- `schedulertest3` shows nice=0 process printing all progress before nice=4 starts

---

## Extra Credit: Priority Inheritance 

### Implementation
- Implemented `lock()` and `release()` system calls
- 7 resource locks (IDs 1-7) available for processes
- **Priority Inheritance:** When high-priority process waits for lock held by low-priority process, the low-priority process temporarily inherits the high priority
- Priority is restored after lock release

### Test

After booting xv6, run these commands:
```bash
locktest1       # Basic lock functionality (acquire/release)
locktest2       # Priority inversion demonstration
locktest3       # Priority inheritance mechanism
```

### Expected Results

#### locktest1 (Basic Lock):
- Process 1 acquires lock 1
- Process 2 waits for lock 1
- After Process 1 releases, Process 2 acquires it

#### locktest2 (Priority Inversion):
- Low priority (nice=4) holds lock
- High priority (nice=0) has to wait
- Demonstrates the problem priority inheritance solves

#### locktest3 (Priority Inheritance):
- Low priority (nice=4) holds lock
- High priority (nice=0) waits
- **Low priority gets boosted** to help it finish faster
- Priority restored after release

---

## Files Modified

### Kernel Files:
- `proc.h` - Added nice, original_nice, holding_lock fields; added lock_t structure
- `proc.c` - Modified scheduler(), added resource_locks array, initialized locks
- `syscall.h` - Added SYS_nice, SYS_lock, SYS_release
- `syscall.c` - Registered new system calls
- `sysproc.c` - Implemented sys_nice(), sys_lock(), sys_release()
- `user.h` - Added user-space declarations
- `usys.S` - Added assembly stubs
- `Makefile` - Added new user programs, compiler flags

### User Programs Created:
- `nice.c` - CLI for nice system call
- `test_nice1.c`, `test_nice2.c`, `test_nice3.c` - Task 1 tests
- `schedulertest1.c`, `schedulertest2.c`, `schedulertest3.c` - Task 2 tests
- `locktest1.c`, `locktest2.c`, `locktest3.c` - Extra credit tests

---

## Important Notes

1. **Compile flags:** Modified Makefile to add `-Wno-infinite-recursion` and `-Wno-array-bounds` for compatibility with newer GCC
2. **Filesystem size:** Removed large test programs (forktest, stressfs, usertests) to fit all new tests
3. **Priority values:** Lower nice value = Higher priority (0 is highest, 4 is lowest)
4. **Lock IDs:** Valid lock IDs are 1-7

