# User-Level Threading Library - Design Document
## CS-GY 6233 Operating Systems - Final Project

**Authors:** Swetha Ganesh Babu (sg8554), Sriram Madhiyalagan (sm12155)

---

## 1. Introduction

### 1.1 Problem Statement
xv6 is a simple Unix-like operating system that only supports processes - it has no native thread support. The goal of this project is to implement a complete user-level threading library that enables concurrent programming within a single xv6 process.

### 1.2 Design Goals
1. **Transparency:** Threads should be invisible to the kernel
2. **Efficiency:** Minimal overhead for thread operations
3. **Correctness:** Thread-safe synchronization primitives
4. **Simplicity:** Clean API similar to POSIX threads

### 1.3 Threading Model
We implemented an **N:1 (Many-to-One)** threading model:
- Multiple user threads map to a single kernel process
- Thread scheduling happens entirely in user space
- Kernel sees only one process, unaware of threads

**Advantages:**
- No kernel modifications required
- Fast thread creation and context switching
- Portable across different kernels

**Limitations:**
- Blocking system calls block all threads
- Cannot utilize multiple CPUs

---

## 2. System Architecture

### 2.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     User Application                         │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐        │
│  │ Thread 0│  │ Thread 1│  │ Thread 2│  │ Thread 3│        │
│  │ (main)  │  │         │  │         │  │         │        │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘        │
│       │            │            │            │              │
│       └────────────┴─────┬──────┴────────────┘              │
│                          │                                   │
│  ┌───────────────────────┴───────────────────────────────┐  │
│  │              Threading Library (uthreads)              │  │
│  │  ┌──────────┐ ┌──────────┐ ┌────────────────────────┐ │  │
│  │  │ Thread   │ │Scheduler │ │ Synchronization        │ │  │
│  │  │ Manager  │ │(Round    │ │ (Mutex, Sem, Cond, Ch) │ │  │
│  │  └──────────┘ │ Robin)   │ └────────────────────────┘ │  │
│  │               └──────────┘                            │  │
│  │  ┌────────────────────────────────────────────────┐   │  │
│  │  │        Context Switch (uthreads_swtch.S)       │   │  │
│  │  └────────────────────────────────────────────────┘   │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ System Calls
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      xv6 Kernel                              │
│                 (Sees single process)                        │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Component Overview

| Component | File | Responsibility |
|-----------|------|----------------|
| Thread Manager | uthreads.c | Create, join, exit, yield threads |
| Scheduler | uthreads.c | Round-robin thread selection |
| Context Switch | uthreads_swtch.S | Save/restore CPU state |
| Synchronization | uthreads.c | Mutex, semaphore, condvar, channel |
| Thread-Safe I/O | uthreads_io.c | Protected file operations |

---

## 3. Data Structures

### 3.1 Thread Control Block (TCB)

```c
struct thread {
    int tid;                        // Unique thread identifier
    int state;                      // Current execution state
    char stack[STACK_SIZE];         // Private stack (4KB)
    void *sp;                       // Saved stack pointer
    void *(*start_routine)(void*);  // Entry function
    void *arg;                      // Argument to entry function
    void *retval;                   // Return value for join
    int join_tid;                   // TID of thread waiting to join
};
```

**Design Rationale:**
- `stack` is embedded in struct for simple memory management (no malloc for stacks)
- `sp` stores the stack pointer during context switch
- `join_tid` enables direct wake-up when thread exits

### 3.2 Thread Table

```c
struct thread threads[MAX_THREADS];  // Global thread table
struct thread *current_thread;       // Currently running thread
int next_tid;                        // Next TID to allocate
```

**Design Rationale:**
- Fixed-size array avoids dynamic allocation complexity
- MAX_THREADS = 16 is sufficient for most applications
- Global `current_thread` pointer enables O(1) access

### 3.3 Thread States

```
    ┌──────────────────────────────────────────────────┐
    │                                                  │
    ▼                                                  │
┌────────┐  thread_create()  ┌──────────┐            │
│T_UNUSED├──────────────────►│T_RUNNABLE│◄───────┐   │
└────────┘                   └─────┬────┘        │   │
    ▲                              │             │   │
    │                   scheduled  │             │   │
    │                              ▼             │   │
    │                        ┌─────────┐        │   │
    │         thread_join()  │T_RUNNING│        │   │
    │         (cleanup)      └────┬────┘        │   │
    │              ▲              │             │   │
    │              │              │ yield/      │   │
    │              │              │ schedule    │   │
    │         ┌────┴───┐          │             │   │
    │         │T_ZOMBIE│◄─────────┤             │   │
    │         └────────┘          │             │   │
    │              ▲              │ block on    │   │
    │              │              │ mutex/join  │   │
    │   thread_exit()             │             │   │
    │              │              ▼             │   │
    │              │        ┌──────────┐        │   │
    │              └────────┤T_SLEEPING├────────┘   │
    │                       └──────────┘  wake up   │
    │                             │                 │
    └─────────────────────────────┴─────────────────┘
                            (never used again after join cleanup)
```

### 3.4 Wait Queue

```c
struct wait_queue {
    int tids[MAX_WAIT_QUEUE];  // Circular buffer of TIDs
    int count;                  // Number of waiting threads
    int head;                   // Read position
    int tail;                   // Write position
};
```

**Design Rationale:**
- FIFO ordering ensures fairness
- Circular buffer is memory-efficient
- Used by mutex, semaphore, and condition variable

### 3.5 Synchronization Primitives

```c
// Mutex - Mutual Exclusion Lock
typedef struct {
    int locked;                 // 0 = unlocked, 1 = locked
    int owner_tid;              // Owner thread (-1 if none)
    struct wait_queue waiters;  // Blocked threads
} mutex_t;

// Semaphore - Counting Synchronization
typedef struct {
    int count;                  // Semaphore value
    struct wait_queue waiters;  // Blocked threads
} sem_t;

// Condition Variable - Wait/Signal
typedef struct {
    struct wait_queue waiters;  // Waiting threads
} cond_t;

// Channel - Bounded Buffer Communication
typedef struct {
    void **buffer;              // Data buffer
    int capacity;               // Max items
    int count;                  // Current items
    int head, tail;             // Read/write positions
    int closed;                 // Channel closed flag
    mutex_t lock;               // Protects channel
    cond_t not_empty;           // Signal when data available
    cond_t not_full;            // Signal when space available
} channel_t;
```

---

## 4. Algorithms

### 4.1 Thread Creation

```
thread_create(start_routine, arg):
    1. Find unused slot in thread table (state == T_UNUSED)
    2. If no slot available, return -1
    3. Assign new TID (next_tid++)
    4. Set state = T_RUNNABLE
    5. Store start_routine and arg
    6. Initialize stack:
       a. sp = stack + STACK_SIZE (top of stack)
       b. Align to 16 bytes
       c. Push thread_entry address (return address)
       d. Push dummy values for %ebp, %ebx, %esi, %edi
       e. Save sp in thread structure
    7. Return TID
```

**Stack Layout After Creation:**
```
High Address
┌─────────────────┐ ← stack + STACK_SIZE
│   (padding)     │
├─────────────────┤
│  thread_entry   │ ← Return address for first switch
├─────────────────┤
│    0 (%ebp)     │
├─────────────────┤
│    0 (%ebx)     │
├─────────────────┤
│    0 (%esi)     │
├─────────────────┤
│    0 (%edi)     │ ← sp points here
├─────────────────┤
│                 │
│  (unused)       │
│                 │
└─────────────────┘ ← stack base
Low Address
```

### 4.2 Round-Robin Scheduler

```
thread_schedule():
    1. Save reference to current thread (old)
    2. Find next runnable thread:
       - Start from (current_index + 1) % MAX_THREADS
       - Search up to MAX_THREADS slots
       - Find first thread with state == T_RUNNABLE
    3. If no runnable thread found:
       - If old is still runnable, continue with old
       - Otherwise, exit process
    4. Update states:
       - If old was RUNNING, set to RUNNABLE
       - Set next to RUNNING
    5. Update current_thread pointer
    6. Call thread_switch(old, next)
```

**Time Complexity:** O(n) where n = MAX_THREADS

### 4.3 Context Switch (x86 Assembly)

```
thread_switch(old, next):
    1. Get old thread pointer from stack [4(%esp)]
    2. Get next thread pointer from stack [8(%esp)]
    3. Save callee-saved registers:
       - push %ebp, %ebx, %esi, %edi
    4. Save stack pointer:
       - old->sp = %esp
    5. Load new stack pointer:
       - %esp = next->sp
    6. Restore callee-saved registers:
       - pop %edi, %esi, %ebx, %ebp
    7. Return (pops return address, jumps there)
```

**Why These Registers?**
- x86 cdecl calling convention requires callee to preserve: %ebp, %ebx, %esi, %edi
- Caller-saved registers (%eax, %ecx, %edx) are already saved by caller
- %esp is explicitly saved/restored

**SP_OFFSET Calculation:**
```
struct thread {
    int tid;            // offset 0,  size 4
    int state;          // offset 4,  size 4
    char stack[4096];   // offset 8,  size 4096
    void *sp;           // offset 4104
    ...
};
SP_OFFSET = 4 + 4 + 4096 = 4104
```

### 4.4 Mutex Lock/Unlock

```
mutex_lock(m):
    while m->locked:
        add current_thread to m->waiters
        current_thread->state = T_SLEEPING
        thread_schedule()
    m->locked = 1
    m->owner_tid = current_thread->tid

mutex_unlock(m):
    if m->owner_tid != current_thread->tid:
        return (error: not owner)
    m->locked = 0
    m->owner_tid = -1
    tid = pop from m->waiters
    if tid >= 0:
        wake thread with that tid (set T_RUNNABLE)
```

### 4.5 Semaphore P/V Operations

```
sem_wait(s):  // P operation
    s->count--
    if s->count < 0:
        add current to waiters
        sleep

sem_post(s):  // V operation
    s->count++
    if s->count <= 0:
        wake one waiter
```

**Invariant:** `count = initial_value - waiters + signals`

### 4.6 Condition Variable

```
cond_wait(c, m):
    add current to c->waiters
    mutex_unlock(m)          // Release lock atomically
    current->state = T_SLEEPING
    thread_schedule()        // Sleep
    mutex_lock(m)            // Re-acquire on wake

cond_signal(c):
    tid = pop from c->waiters
    if tid >= 0:
        set thread tid to T_RUNNABLE
```

**Atomicity:** In cooperative scheduling, no preemption between unlock and sleep.

### 4.7 Channel Send/Receive

```
channel_send(ch, data):
    mutex_lock(&ch->lock)
    while ch->count == ch->capacity && !ch->closed:
        cond_wait(&ch->not_full, &ch->lock)
    if ch->closed:
        mutex_unlock(&ch->lock)
        return -1
    ch->buffer[ch->tail] = data
    ch->tail = (ch->tail + 1) % ch->capacity
    ch->count++
    cond_signal(&ch->not_empty)
    mutex_unlock(&ch->lock)
    return 0

channel_recv(ch, &data):
    mutex_lock(&ch->lock)
    while ch->count == 0 && !ch->closed:
        cond_wait(&ch->not_empty, &ch->lock)
    if ch->count == 0 && ch->closed:
        mutex_unlock(&ch->lock)
        return -1
    data = ch->buffer[ch->head]
    ch->head = (ch->head + 1) % ch->capacity
    ch->count--
    cond_signal(&ch->not_full)
    mutex_unlock(&ch->lock)
    return 0
```

---

## 5. Concurrency Problem Solutions

### 5.1 Producer-Consumer (Semaphores)

**Synchronization Variables:**
- `empty_slots`: Semaphore initialized to BUFFER_SIZE
- `filled_slots`: Semaphore initialized to 0
- `buffer_lock`: Mutex protecting buffer access

**Producer Algorithm:**
```
produce(item):
    sem_wait(&empty_slots)    // Wait for empty slot
    mutex_lock(&buffer_lock)
    buffer[tail] = item
    tail = (tail + 1) % SIZE
    mutex_unlock(&buffer_lock)
    sem_post(&filled_slots)   // Signal item available
```

**Consumer Algorithm:**
```
consume():
    sem_wait(&filled_slots)   // Wait for item
    mutex_lock(&buffer_lock)
    item = buffer[head]
    head = (head + 1) % SIZE
    mutex_unlock(&buffer_lock)
    sem_post(&empty_slots)    // Signal slot available
    return item
```

### 5.2 Reader-Writer Lock (Writer Priority)

**State Variables:**
- `readers_reading`: Count of active readers
- `writers_waiting`: Count of waiting writers
- `writer_writing`: Flag if writer is active

**Reader Algorithm:**
```
reader_lock():
    mutex_lock(&lock)
    while writer_writing || writers_waiting > 0:  // Writer priority!
        cond_wait(&readers_ok, &lock)
    readers_reading++
    mutex_unlock(&lock)

reader_unlock():
    mutex_lock(&lock)
    readers_reading--
    if readers_reading == 0 && writers_waiting > 0:
        cond_signal(&writers_ok)
    mutex_unlock(&lock)
```

**Writer Algorithm:**
```
writer_lock():
    mutex_lock(&lock)
    writers_waiting++         // Announce waiting (blocks new readers)
    while readers_reading > 0 || writer_writing:
        cond_wait(&writers_ok, &lock)
    writers_waiting--
    writer_writing = 1
    mutex_unlock(&lock)

writer_unlock():
    mutex_lock(&lock)
    writer_writing = 0
    if writers_waiting > 0:
        cond_signal(&writers_ok)   // Prefer writers
    else:
        cond_broadcast(&readers_ok)
    mutex_unlock(&lock)
```

**Writer Priority:** `writers_waiting > 0` in reader_lock prevents new readers when writers are waiting.

---

## 6. Thread-Safe File I/O Design

### 6.1 Challenge
In N:1 threading, blocking I/O (read/write) blocks the entire process, starving all threads.

### 6.2 Solution
1. **Mutex Protection:** Each file has a mutex preventing concurrent access
2. **Cooperative Yielding:** Yield before and after I/O to let other threads run
3. **Process Separation:** Use fork() for true async I/O (separate processes)

```c
int tio_read(tio_file_t *file, void *buf, int n) {
    mutex_lock(&file->mutex);
    thread_yield();              // Let others run before blocking
    int result = read(file->fd, buf, n);
    thread_yield();              // Let others run after
    mutex_unlock(&file->mutex);
    return result;
}
```

### 6.3 Pipe-Based Async I/O
For true parallelism, we use separate processes:
- Parent process: Producer threads write to pipe
- Child process: Consumer threads read from pipe
- Pipe provides natural synchronization

---

## 7. Memory Layout

### 7.1 Process Address Space with Threads

```
┌─────────────────────┐ High Address (0xFFFFFFFF)
│    Kernel Space     │
├─────────────────────┤ KERNBASE
│                     │
│    Thread 15 Stack  │ (4KB)
├─────────────────────┤
│         ...         │
├─────────────────────┤
│    Thread 1 Stack   │ (4KB)
├─────────────────────┤
│    Thread 0 Stack   │ (uses process stack)
├─────────────────────┤
│                     │
│   Heap (malloc)     │
│         ↓           │
├─────────────────────┤
│         ↑           │
│   Process Stack     │ (main thread)
├─────────────────────┤
│    BSS (globals)    │
├─────────────────────┤
│    Data Segment     │
├─────────────────────┤
│    Text (code)      │
└─────────────────────┘ Low Address (0x0)
```

### 7.2 Thread Stacks
- Embedded in `struct thread` for simplicity
- 4KB per thread (STACK_SIZE = 4096)
- Total: 16 threads × 4KB = 64KB for all stacks
- Main thread (thread 0) uses the original process stack

---

## 8. Testing Strategy

### 8.1 Unit Tests

| Test | What It Verifies |
|------|------------------|
| t_basic_test | Thread lifecycle: create, yield, join, exit, self |
| t_shared_counter | Mutex correctness via race condition detection |

### 8.2 Integration Tests

| Test | What It Verifies |
|------|------------------|
| t_producer_consumer_sem | Semaphore + mutex coordination |
| t_producer_consumer_chan | Channel blocking/waking |
| t_reader_writer | RW lock + writer priority |
| t_file_producer_consumer | Thread-safe I/O + fork |

### 8.3 Test Configurations

| Test | Threads | Operations | Expected Result |
|------|---------|------------|-----------------|
| basic_test | 3+1 | 5 iterations each | Interleaved output, correct returns |
| shared_counter | 3 | 1000 increments each | 3000 with mutex, <3000 without |
| producer_consumer | 3P + 2C | 30 items total | All items processed |
| reader_writer | 3R + 2W | 15 reads, 6 writes | Exclusive writes, concurrent reads |

---

## 9. Limitations and Future Work

### 9.1 Current Limitations

1. **No Preemption:** Threads must cooperate; infinite loop blocks all threads
2. **Blocking I/O:** System calls block entire process
3. **Single CPU:** Cannot utilize multiple cores
4. **Fixed Thread Limit:** MAX_THREADS = 16 is hardcoded
5. **No Priority Scheduling:** Pure round-robin only

### 9.2 Possible Extensions

1. **Timer-Based Preemption:** Use SIGALRM for time slicing
2. **M:N Threading:** Map M user threads to N kernel threads
3. **Thread Pools:** Reusable worker threads
4. **Priority Queues:** Support thread priorities
5. **Thread-Local Storage:** Per-thread global variables

---

## 10. Conclusion

This project successfully implements a complete user-level threading library for xv6 with:

- **Full thread lifecycle management** (create, join, exit, yield)
- **Correct round-robin scheduling** with O(n) complexity
- **Efficient x86 context switching** (saves only callee-saved registers)
- **Comprehensive synchronization** (mutex, semaphore, condvar, channel)
- **Classic concurrency problems** solved correctly
- **Thread-safe file I/O** with cooperative yielding

The implementation demonstrates understanding of:
- Operating system threading concepts
- x86 architecture and calling conventions
- Synchronization primitive design
- Concurrent programming patterns

All components have been tested and verified working on xv6.

---

## Appendix A: API Reference

### Thread Management
```c
void  thread_init(void);
int   thread_create(void *(*start_routine)(void*), void *arg);
void* thread_join(int tid);
void  thread_exit(void *retval);
int   thread_self(void);
void  thread_yield(void);
```

### Mutex
```c
void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
```

### Semaphore
```c
void sem_init(sem_t *s, int value);
void sem_wait(sem_t *s);
void sem_post(sem_t *s);
```

### Condition Variable
```c
void cond_init(cond_t *c);
void cond_wait(cond_t *c, mutex_t *m);
void cond_signal(cond_t *c);
void cond_broadcast(cond_t *c);
```

### Channel
```c
channel_t* channel_create(int capacity);
int  channel_send(channel_t *ch, void *data);
int  channel_recv(channel_t *ch, void **data);
void channel_close(channel_t *ch);
void channel_destroy(channel_t *ch);
```

### Thread-Safe I/O
```c
void tio_init(tio_file_t *file, int fd);
int  tio_open(const char *path, int mode);
int  tio_close(tio_file_t *file);
int  tio_read(tio_file_t *file, void *buf, int n);
int  tio_write(tio_file_t *file, void *buf, int n);
int  tio_pipe(tio_file_t *read_end, tio_file_t *write_end);
```
