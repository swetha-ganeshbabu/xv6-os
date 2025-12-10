/*
 * uthreads.h - User-Level Threading Library for xv6
 *
 * This header defines the interface for a cooperative user-level threading
 * library. All threading operations happen entirely in user space - the xv6
 * kernel is unaware that threads exist.
 *
 * Threading Model: N:1 (many user threads map to one kernel process)
 * Scheduling: Cooperative, round-robin
 */

#ifndef _UTHREADS_H_
#define _UTHREADS_H_

/*
 * =============================================================================
 * CONFIGURATION CONSTANTS
 * =============================================================================
 */

/* Maximum number of threads per process */
#define MAX_THREADS 16

/* Stack size for each thread (8KB) */
#define STACK_SIZE 8192

/* Maximum number of threads that can wait on a single primitive */
#define MAX_WAITERS 16

/* Channel buffer capacity default */
#define DEFAULT_CHANNEL_CAPACITY 8

/*
 * =============================================================================
 * THREAD STATES
 * =============================================================================
 * These states manage the thread lifecycle within our scheduler.
 */

/* Thread state enumeration */
enum thread_state {
    T_UNUSED = 0,   /* Slot is available in thread table */
    T_RUNNABLE,     /* Thread is ready to run, waiting for scheduler */
    T_RUNNING,      /* Thread is currently executing on CPU */
    T_SLEEPING,     /* Thread is blocked (waiting on mutex, join, etc.) */
    T_ZOMBIE        /* Thread has finished but not yet joined */
};

/*
 * =============================================================================
 * THREAD STRUCTURE
 * =============================================================================
 */

/* Thread Control Block (TCB) */
struct thread {
    int tid;                        /* Thread ID */
    enum thread_state state;        /* Current thread state */

    char *stack;                    /* Base of allocated stack */
    char *sp;                       /* Current stack pointer (saved during switch) */

    void *(*start_routine)(void*);  /* Function to execute */
    void *arg;                      /* Argument to start_routine */
    void *retval;                   /* Return value from thread */

    int join_tid;                   /* TID of thread waiting to join this one */
    int waiting_on;                 /* TID this thread is waiting to join (-1 if none) */
};

/*
 * =============================================================================
 * MUTEX STRUCTURE
 * =============================================================================
 */

typedef struct {
    int locked;                     /* 0 = unlocked, 1 = locked */
    int owner_tid;                  /* TID of thread holding the lock (-1 if none) */

    /* Wait queue for blocked threads */
    int wait_queue[MAX_WAITERS];    /* TIDs of waiting threads */
    int wait_count;                 /* Number of threads in wait queue */
} mutex_t;

/*
 * =============================================================================
 * SEMAPHORE STRUCTURE
 * =============================================================================
 */

typedef struct {
    int count;                      /* Current semaphore count */

    /* Wait queue for blocked threads */
    int wait_queue[MAX_WAITERS];    /* TIDs of waiting threads */
    int wait_count;                 /* Number of threads in wait queue */
} sem_t;

/*
 * =============================================================================
 * CONDITION VARIABLE STRUCTURE
 * =============================================================================
 */

typedef struct {
    /* Wait queue for threads waiting on condition */
    int wait_queue[MAX_WAITERS];    /* TIDs of waiting threads */
    int wait_count;                 /* Number of threads in wait queue */
} cond_t;

/*
 * =============================================================================
 * CHANNEL STRUCTURE (Bounded Buffer for Message Passing)
 * =============================================================================
 */

typedef struct {
    void **buffer;                  /* Circular buffer of void pointers */
    int capacity;                   /* Maximum buffer size */
    int count;                      /* Current number of items in buffer */
    int read_pos;                   /* Index for next read */
    int write_pos;                  /* Index for next write */
    int closed;                     /* 1 if channel is closed */

    mutex_t lock;                   /* Protects channel state */
    cond_t not_empty;               /* Signal when data available */
    cond_t not_full;                /* Signal when space available */
} channel_t;

/*
 * =============================================================================
 * READER-WRITER LOCK STRUCTURE (Writer Priority)
 * =============================================================================
 */

typedef struct {
    int readers;                    /* Number of active readers */
    int writers;                    /* Number of active writers (0 or 1) */
    int waiting_writers;            /* Number of writers waiting */

    mutex_t lock;                   /* Protects rwlock state */
    cond_t read_ok;                 /* Condition for readers to proceed */
    cond_t write_ok;                /* Condition for writers to proceed */
} rwlock_t;

/*
 * =============================================================================
 * CORE THREADING API (Part 1)
 * =============================================================================
 */

/*
 * thread_init - Initialize the threading system
 *
 * Must be called before any other threading functions.
 * Sets up the thread table and registers the main program as thread 0.
 */
void thread_init(void);

/*
 * thread_create - Create a new thread
 *
 * @start_routine: Function the new thread will execute
 * @arg: Argument to pass to start_routine
 *
 * Returns: TID of new thread on success, -1 on failure
 *
 * The new thread starts in T_RUNNABLE state and will begin executing
 * when selected by the scheduler.
 */
int thread_create(void* (*start_routine)(void*), void *arg);

/*
 * thread_join - Wait for a thread to terminate
 *
 * @tid: Thread ID to wait for
 *
 * Returns: Return value of the terminated thread
 *
 * Blocks the calling thread until the target thread exits.
 * Cleans up the target thread's resources.
 */
void *thread_join(int tid);

/*
 * thread_exit - Terminate the current thread
 *
 * @retval: Return value to pass to joining thread
 *
 * This function does not return. The thread transitions to T_ZOMBIE
 * state and the scheduler runs another thread.
 */
void thread_exit(void *retval);

/*
 * thread_self - Get current thread's TID
 *
 * Returns: TID of the currently running thread
 */
int thread_self(void);

/*
 * thread_yield - Voluntarily give up the CPU
 *
 * The current thread becomes T_RUNNABLE and the scheduler
 * selects another thread to run.
 */
void thread_yield(void);

/*
 * thread_schedule - Internal scheduler function
 *
 * Selects the next runnable thread using round-robin scheduling
 * and performs a context switch to it.
 */
void thread_schedule(void);

/*
 * thread_switch - Context switch between threads (assembly)
 *
 * @old: Thread to save context from
 * @next: Thread to restore context to
 *
 * Saves callee-saved registers and stack pointer of old thread,
 * then restores those of the next thread.
 */
void thread_switch(struct thread *old, struct thread *next);

/*
 * =============================================================================
 * MUTEX API (Part 2.1)
 * =============================================================================
 */

/*
 * mutex_init - Initialize a mutex
 *
 * @m: Pointer to mutex to initialize
 */
void mutex_init(mutex_t *m);

/*
 * mutex_lock - Acquire a mutex
 *
 * @m: Pointer to mutex to acquire
 *
 * Blocks if the mutex is held by another thread.
 */
void mutex_lock(mutex_t *m);

/*
 * mutex_unlock - Release a mutex
 *
 * @m: Pointer to mutex to release
 *
 * Wakes one waiting thread if any are blocked on this mutex.
 */
void mutex_unlock(mutex_t *m);

/*
 * =============================================================================
 * SEMAPHORE API (Part 2.3 - Extra Credit)
 * =============================================================================
 */

/*
 * sem_init - Initialize a semaphore
 *
 * @s: Pointer to semaphore to initialize
 * @value: Initial count value
 */
void sem_init(sem_t *s, int value);

/*
 * sem_wait - Decrement semaphore (P operation)
 *
 * @s: Pointer to semaphore
 *
 * Blocks if count would become negative.
 */
void sem_wait(sem_t *s);

/*
 * sem_post - Increment semaphore (V operation)
 *
 * @s: Pointer to semaphore
 *
 * Wakes one waiting thread if any are blocked.
 */
void sem_post(sem_t *s);

/*
 * =============================================================================
 * CONDITION VARIABLE API (Part 2.4 - Extra Credit)
 * =============================================================================
 */

/*
 * cond_init - Initialize a condition variable
 *
 * @c: Pointer to condition variable to initialize
 */
void cond_init(cond_t *c);

/*
 * cond_wait - Wait on a condition variable
 *
 * @c: Pointer to condition variable
 * @m: Pointer to mutex (must be held by caller)
 *
 * Atomically releases the mutex and blocks. Re-acquires mutex before returning.
 */
void cond_wait(cond_t *c, mutex_t *m);

/*
 * cond_signal - Wake one waiting thread
 *
 * @c: Pointer to condition variable
 */
void cond_signal(cond_t *c);

/*
 * cond_broadcast - Wake all waiting threads
 *
 * @c: Pointer to condition variable
 */
void cond_broadcast(cond_t *c);

/*
 * =============================================================================
 * CHANNEL API (Part 2.5 - Extra Credit)
 * =============================================================================
 */

/*
 * channel_create - Create a new channel
 *
 * @capacity: Maximum number of items the channel can hold
 *
 * Returns: Pointer to new channel, or 0 on failure
 */
channel_t* channel_create(int capacity);

/*
 * channel_send - Send data through channel
 *
 * @ch: Pointer to channel
 * @data: Data to send
 *
 * Returns: 0 on success, -1 if channel is closed
 *
 * Blocks if channel is full.
 */
int channel_send(channel_t *ch, void *data);

/*
 * channel_recv - Receive data from channel
 *
 * @ch: Pointer to channel
 * @data: Pointer to store received data
 *
 * Returns: 0 on success, -1 if channel is closed and empty
 *
 * Blocks if channel is empty.
 */
int channel_recv(channel_t *ch, void **data);

/*
 * channel_close - Close a channel
 *
 * @ch: Pointer to channel
 *
 * Wakes all blocked senders and receivers.
 */
void channel_close(channel_t *ch);

/*
 * channel_destroy - Free channel resources
 *
 * @ch: Pointer to channel to destroy
 */
void channel_destroy(channel_t *ch);

/*
 * =============================================================================
 * READER-WRITER LOCK API (Part 3.2 - Extra Credit)
 * =============================================================================
 */

/*
 * rwlock_init - Initialize a reader-writer lock
 *
 * @rw: Pointer to rwlock to initialize
 */
void rwlock_init(rwlock_t *rw);

/*
 * reader_lock - Acquire read lock
 *
 * @rw: Pointer to rwlock
 *
 * Blocks if a writer is active or waiting (writer priority).
 */
void reader_lock(rwlock_t *rw);

/*
 * reader_unlock - Release read lock
 *
 * @rw: Pointer to rwlock
 */
void reader_unlock(rwlock_t *rw);

/*
 * writer_lock - Acquire write lock
 *
 * @rw: Pointer to rwlock
 *
 * Blocks if any readers or writers are active.
 */
void writer_lock(rwlock_t *rw);

/*
 * writer_unlock - Release write lock
 *
 * @rw: Pointer to rwlock
 */
void writer_unlock(rwlock_t *rw);

#endif /* _UTHREADS_H_ */
