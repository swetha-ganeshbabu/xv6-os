# xv6 User-Level Threading Library
## CS-GY 6233 Operating Systems - Final Project

**Student Names:** Swetha Ganesh Babu, Sriram Madhiyalagan
**Net IDs:** sg8554, sm12155

---

## Project Overview

This project implements a complete **user-level threading library** for xv6 from scratch. The library provides N:1 threading (multiple user threads mapped to a single kernel process) with cooperative scheduling and comprehensive synchronization primitives.

---

## Compilation Instructions

```bash
make clean
make qemu-nox
```

To exit xv6: Press `Ctrl+A` then `X`

---

## Project Structure

```
xv6-os/
├── user_threading_library_core/
│   ├── src/
│   │   ├── uthreads.h          # Thread library interface
│   │   ├── uthreads.c          # Thread library implementation
│   │   ├── uthreads_swtch.S    # x86 context switch assembly
│   │   ├── uthreads_io.h       # Thread-safe I/O interface
│   │   └── uthreads_io.c       # Thread-safe I/O implementation
│   ├── tests/
│   │   ├── t_basic_test.c      # Basic threading test
│   │   └── t_shared_counter.c  # Mutex/race condition test
│   └── examples/
│       ├── t_producer_consumer_sem.c   # Producer-Consumer (semaphores)
│       ├── t_producer_consumer_chan.c  # Producer-Consumer (channels)
│       ├── t_reader_writer.c           # Reader-Writer lock
│       └── t_file_producer_consumer.c  # Thread-safe file I/O
├── Makefile                    # Modified for threading library
└── param.h                     # FSSIZE increased to 2000
```

---

## Part 1: Threading Foundation (60 points)

### Thread States
| State | Value | Description |
|-------|-------|-------------|
| T_UNUSED | 0 | Thread slot is available |
| T_RUNNABLE | 1 | Ready to run, waiting for scheduler |
| T_RUNNING | 2 | Currently executing on CPU |
| T_SLEEPING | 3 | Blocked (waiting on mutex, join, etc.) |
| T_ZOMBIE | 4 | Finished but not yet joined |

### Thread Structure
```c
struct thread {
    int tid;                        // Thread ID
    int state;                      // Current state
    char stack[STACK_SIZE];         // 4KB private stack
    void *sp;                       // Saved stack pointer
    void *(*start_routine)(void*);  // Function to execute
    void *arg;                      // Argument to function
    void *retval;                   // Return value
    int join_tid;                   // TID of joining thread
};
```

### Thread API
| Function | Description |
|----------|-------------|
| `thread_init()` | Initialize threading system, main becomes thread 0 |
| `thread_create(func, arg)` | Create new thread, returns TID |
| `thread_join(tid)` | Wait for thread to finish, returns its retval |
| `thread_exit(retval)` | Terminate current thread |
| `thread_self()` | Get current thread's TID |
| `thread_yield()` | Voluntarily give up CPU |

### Scheduler
- **Round-robin** scheduling algorithm
- Searches for next T_RUNNABLE thread starting from current+1
- Wraps around thread table for fairness

### Context Switch (x86 Assembly)
- Saves callee-saved registers: `%ebp, %ebx, %esi, %edi`
- Saves/restores stack pointer via `SP_OFFSET` (4104 bytes)
- Uses `ret` instruction to jump to new thread's saved location

### Test
```bash
t_basic_test
```
**Expected Output:** Creates 3 threads, each iterates 5 times with interleaved output, returns values 10, 20, 30.

---

## Part 2: Synchronization Primitives (35 points)

### Mutex
```c
typedef struct {
    int locked;
    int owner_tid;
    struct wait_queue waiters;
} mutex_t;
```

| Function | Description |
|----------|-------------|
| `mutex_init(m)` | Initialize mutex to unlocked |
| `mutex_lock(m)` | Acquire mutex, blocks if held |
| `mutex_unlock(m)` | Release mutex, wakes one waiter |

### Shared Counter Test
- **Configuration:** 3 threads, 1000 increments each
- **Test 1 (No Mutex):** Demonstrates race condition - counter < 3000
- **Test 2 (With Mutex):** Counter = 3000 (correct)

```bash
t_shared_counter
```
**Expected Output:**
```
Without mutex: 1000/3000 increments recorded (2000 lost)
With mutex:    3000/3000 increments recorded
Mutex implementation: WORKING CORRECTLY
```

---

## Part 2 Extra Credit: Additional Primitives (+15 points)

### Semaphores (+5 points)
```c
typedef struct {
    int count;
    struct wait_queue waiters;
} sem_t;
```

| Function | Description |
|----------|-------------|
| `sem_init(s, value)` | Initialize with count |
| `sem_wait(s)` | P operation - decrement, block if < 0 |
| `sem_post(s)` | V operation - increment, wake waiter |

### Condition Variables (+5 points)
```c
typedef struct {
    struct wait_queue waiters;
} cond_t;
```

| Function | Description |
|----------|-------------|
| `cond_init(c)` | Initialize condition variable |
| `cond_wait(c, m)` | Atomically release mutex and wait |
| `cond_signal(c)` | Wake one waiting thread |
| `cond_broadcast(c)` | Wake all waiting threads |

### Channels (+5 points)
```c
typedef struct {
    void **buffer;
    int capacity, count, head, tail;
    int closed;
    mutex_t lock;
    cond_t not_empty, not_full;
} channel_t;
```

| Function | Description |
|----------|-------------|
| `channel_create(cap)` | Create bounded buffer channel |
| `channel_send(ch, data)` | Send data, blocks if full |
| `channel_recv(ch, &data)` | Receive data, blocks if empty |
| `channel_close(ch)` | Close channel, wake all waiters |
| `channel_destroy(ch)` | Free channel resources |

---

## Part 3: Concurrency Problems (Grade Upgrade)

### Producer-Consumer with Semaphores
- **Configuration:** 3 producers, 2 consumers, buffer size 5
- **Items:** Each producer creates 10 items (30 total)
- **Semaphores:** `empty_slots` (init=5), `filled_slots` (init=0)

```bash
t_producer_consumer_sem
```
**Expected Output:** All 30 items produced and consumed correctly.

### Producer-Consumer with Channels
- Same configuration using `channel_t` instead of semaphores
- Channel handles all synchronization internally

```bash
t_producer_consumer_chan
```
**Expected Output:** All 30 items produced and consumed correctly.

### Reader-Writer Lock with Writer Priority
- **Configuration:** 3 readers (5 reads each), 2 writers (3 writes each)
- **Writer Priority:** Once a writer is waiting, no new readers can start
- **Implementation:** Custom `rwlock_t` using mutex + condition variables

```c
typedef struct {
    int readers_reading;
    int writers_waiting;
    int writer_writing;
    mutex_t lock;
    cond_t readers_ok, writers_ok;
} rwlock_t;
```

```bash
t_reader_writer
```
**Expected Output:** Multiple readers read simultaneously, writers get exclusive access, writer priority prevents writer starvation.

---

## Part 4: Thread-Safe File I/O (Extra Credit)

### Thread-Safe I/O API
```c
typedef struct {
    int fd;
    int owner_tid;
    mutex_t mutex;
} tio_file_t;
```

| Function | Description |
|----------|-------------|
| `tio_init(file, fd)` | Initialize with file descriptor |
| `tio_open(path, mode)` | Open file (O_RDONLY, O_WRONLY, O_CREATE) |
| `tio_close(file)` | Close file |
| `tio_read(file, buf, n)` | Thread-safe read |
| `tio_write(file, buf, n)` | Thread-safe write |
| `tio_pipe(r, w)` | Create thread-safe pipe |

### Async I/O Demo
- Uses `fork()` to create separate producer and consumer processes
- Parent: 2 producer threads write to pipe
- Child: 2 consumer threads read from pipe
- Demonstrates non-blocking I/O with threads

```bash
t_file_producer_consumer
```
**Expected Output:** 10 items transferred via pipe between processes.

---

## Test Results Summary

| Test | Status | Result |
|------|--------|--------|
| t_basic_test | PASS | All thread operations working |
| t_shared_counter | PASS | Race condition detected, mutex works |
| t_producer_consumer_sem | PASS | 30/30 items processed |
| t_producer_consumer_chan | PASS | 30/30 items processed |
| t_reader_writer | PASS | 15 reads, 6 writes completed |
| t_file_producer_consumer | PASS | 10/10 items via pipe |

---

## Implementation Details

### Key Design Decisions

1. **N:1 Threading Model:** All threads run in user space within a single process. The kernel is unaware of threads.

2. **Cooperative Scheduling:** Threads must explicitly yield (`thread_yield()`) or block on synchronization primitives. No preemption.

3. **Stack Layout:** Each thread has a 4KB stack. Stack pointer offset calculated as:
   ```
   SP_OFFSET = sizeof(tid) + sizeof(state) + sizeof(stack)
             = 4 + 4 + 4096 = 4104 bytes
   ```

4. **Wait Queues:** Circular buffer implementation for FIFO ordering of blocked threads.

5. **Thread Entry Wrapper:** `thread_entry()` calls user's function and automatically calls `thread_exit()` on return.

### Files Modified
- `Makefile` - Added UTHREAD_LIB, _t_% pattern rule
- `param.h` - Increased FSSIZE from 1000 to 2000

### Files Created
| File | Lines | Description |
|------|-------|-------------|
| uthreads.h | 381 | Complete API definitions |
| uthreads.c | 716 | All implementations |
| uthreads_swtch.S | 60 | x86 context switch |
| uthreads_io.h | 100 | I/O interface |
| uthreads_io.c | 213 | I/O implementation |
| t_basic_test.c | 117 | Threading test |
| t_shared_counter.c | 172 | Mutex test |
| t_producer_consumer_sem.c | 203 | Semaphore demo |
| t_producer_consumer_chan.c | 206 | Channel demo |
| t_reader_writer.c | 262 | RW lock demo |
| t_file_producer_consumer.c | 300 | I/O demo |

---

## Points Summary

| Component | Points | Status |
|-----------|--------|--------|
| Part 1: Threading Foundation | 60 | Complete |
| Part 2: Mutex + Shared Counter | 35 | Complete |
| Part 2 EC: Semaphores | +5 | Complete |
| Part 2 EC: Condition Variables | +5 | Complete |
| Part 2 EC: Channels | +5 | Complete |
| Part 3: Producer-Consumer (Sem) | Grade↑ | Complete |
| Part 3: Producer-Consumer (Chan) | Grade↑ | Complete |
| Part 3: Reader-Writer | Grade↑ | Complete |
| Part 4: Thread-Safe I/O | EC | Complete |

**Total: All requirements implemented and tested.**

---

## References

- xv6 kernel context switch: `swtch.S`
- x86 calling conventions (callee-saved registers)
- POSIX threads API design
- Go channels for bounded buffer design
