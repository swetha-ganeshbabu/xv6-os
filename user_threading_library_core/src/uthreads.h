/*
 * uthreads.h - User-Level Threading Library Interface for xv6
 *
 * This header defines the public interface for the user-level threading library.
 * It includes thread management, synchronization primitives (mutexes, semaphores,
 * condition variables), and channels for inter-thread communication.
 */

#ifndef UTHREADS_H
#define UTHREADS_H

/*
 * =============================================================================
 * Constants and Configuration
 * =============================================================================
 */

#define MAX_THREADS     16      // Maximum number of threads per process
#define STACK_SIZE      4096    // 4KB stack per thread (keeping small for xv6)
#define MAX_WAIT_QUEUE  16      // Maximum threads in a wait queue

/*
 * =============================================================================
 * Thread States
 * =============================================================================
 * These states define the lifecycle of a thread in the system.
 */

#define T_UNUSED    0   // Thread slot is available
#define T_RUNNABLE  1   // Thread is ready to run, waiting for scheduler
#define T_RUNNING   2   // Thread is currently executing on the CPU
#define T_SLEEPING  3   // Thread is blocked (waiting on mutex, join, etc.)
#define T_ZOMBIE    4   // Thread has finished but not yet joined

/*
 * =============================================================================
 * Thread Structure
 * =============================================================================
 */

struct thread {
    int tid;                        // Thread ID
    int state;                      // Current state (T_UNUSED, T_RUNNABLE, etc.)
    char stack[STACK_SIZE];         // Thread's private stack
    void *sp;                       // Saved stack pointer

    // Thread function and argument
    void *(*start_routine)(void*);  // Function to execute
    void *arg;                      // Argument to pass to start_routine

    // Return value and join synchronization
    void *retval;                   // Return value from thread
    int join_tid;                   // TID of thread waiting to join this one (-1 if none)
};

/*
 * =============================================================================
 * Wait Queue Structure (for synchronization primitives)
 * =============================================================================
 */

struct wait_queue {
    int tids[MAX_WAIT_QUEUE];       // Array of waiting thread TIDs
    int count;                      // Number of threads in queue
    int head;                       // Head index (for FIFO)
    int tail;                       // Tail index (for FIFO)
};

/*
 * =============================================================================
 * Mutex Structure
 * =============================================================================
 */

typedef struct {
    int locked;                     // 0 = unlocked, 1 = locked
    int owner_tid;                  // TID of the thread holding the lock (-1 if none)
    struct wait_queue waiters;      // Queue of threads waiting for this mutex
} mutex_t;

/*
 * =============================================================================
 * Semaphore Structure
 * =============================================================================
 */

typedef struct {
    int count;                      // Current semaphore count
    struct wait_queue waiters;      // Queue of threads waiting on semaphore
} sem_t;

/*
 * =============================================================================
 * Condition Variable Structure
 * =============================================================================
 */

typedef struct {
    struct wait_queue waiters;      // Queue of threads waiting on condition
} cond_t;

/*
 * =============================================================================
 * Channel Structure (Bounded Buffer for message passing)
 * =============================================================================
 */

typedef struct {
    void **buffer;                  // Circular buffer of data pointers
    int capacity;                   // Maximum number of items
    int count;                      // Current number of items in buffer
    int head;                       // Read position
    int tail;                       // Write position
    int closed;                     // 1 if channel is closed
    mutex_t lock;                   // Mutex protecting the channel
    cond_t not_empty;               // Signaled when buffer becomes non-empty
    cond_t not_full;                // Signaled when buffer becomes non-full
} channel_t;

/*
 * =============================================================================
 * Thread Management API (Part 1)
 * =============================================================================
 */

/*
 * thread_init - Initialize the threading system
 *
 * Must be called before any other threading functions.
 * Sets up the main thread as thread 0 in T_RUNNING state.
 */
void thread_init(void);

/*
 * thread_create - Create a new thread
 *
 * @start_routine: Function the new thread will execute
 * @arg: Argument to pass to start_routine
 *
 * Returns: TID of the new thread, or -1 on failure
 *
 * The new thread begins execution at start_routine(arg).
 * When start_routine returns, thread_exit is called automatically.
 */
int thread_create(void *(*start_routine)(void*), void *arg);

/*
 * thread_join - Wait for a thread to terminate
 *
 * @tid: TID of the thread to wait for
 *
 * Returns: The return value from the terminated thread's thread_exit
 *
 * If the target thread has not finished, the calling thread blocks.
 * Once the target is in T_ZOMBIE state, its resources are cleaned up.
 */
void *thread_join(int tid);

/*
 * thread_exit - Terminate the current thread
 *
 * @retval: Value to return to joining thread
 *
 * This function does not return. It sets the thread to T_ZOMBIE,
 * wakes any thread waiting to join, and calls the scheduler.
 */
void thread_exit(void *retval);

/*
 * thread_self - Get the TID of the current thread
 *
 * Returns: TID of the calling thread
 */
int thread_self(void);

/*
 * thread_yield - Voluntarily give up the CPU
 *
 * Marks the current thread as T_RUNNABLE and calls the scheduler
 * to select another thread to run.
 */
void thread_yield(void);

/*
 * =============================================================================
 * Scheduler (Part 1)
 * =============================================================================
 */

/*
 * thread_schedule - Select and switch to the next runnable thread
 *
 * Implements round-robin scheduling. Finds the next T_RUNNABLE thread,
 * updates states, and performs a context switch.
 */
void thread_schedule(void);

/*
 * =============================================================================
 * Context Switch (Part 1 - implemented in assembly)
 * =============================================================================
 */

/*
 * thread_switch - Low-level context switch between threads
 *
 * @old: Pointer to the currently running thread
 * @next: Pointer to the thread to switch to
 *
 * Saves old thread's registers, loads next thread's registers.
 * Implemented in uthreads_swtch.S
 */
void thread_switch(struct thread *old, struct thread *next);

/*
 * =============================================================================
 * Mutex API (Part 2)
 * =============================================================================
 */

/*
 * mutex_init - Initialize a mutex
 *
 * @m: Pointer to the mutex to initialize
 *
 * Sets the mutex to unlocked state with no owner.
 */
void mutex_init(mutex_t *m);

/*
 * mutex_lock - Acquire a mutex
 *
 * @m: Pointer to the mutex to lock
 *
 * If the mutex is unlocked, locks it and returns immediately.
 * If locked by another thread, blocks until the mutex is available.
 */
void mutex_lock(mutex_t *m);

/*
 * mutex_unlock - Release a mutex
 *
 * @m: Pointer to the mutex to unlock
 *
 * Releases the mutex. If threads are waiting, wakes one of them.
 * Only the owning thread should call this function.
 */
void mutex_unlock(mutex_t *m);

/*
 * =============================================================================
 * Semaphore API (Part 2 - Extra Credit)
 * =============================================================================
 */

/*
 * sem_init - Initialize a semaphore
 *
 * @s: Pointer to the semaphore to initialize
 * @value: Initial count value
 */
void sem_init(sem_t *s, int value);

/*
 * sem_wait - Decrement semaphore (P operation)
 *
 * @s: Pointer to the semaphore
 *
 * Decrements the count. If count becomes negative, blocks.
 */
void sem_wait(sem_t *s);

/*
 * sem_post - Increment semaphore (V operation)
 *
 * @s: Pointer to the semaphore
 *
 * Increments the count. If threads are waiting, wakes one.
 */
void sem_post(sem_t *s);

/*
 * =============================================================================
 * Condition Variable API (Part 2 - Extra Credit)
 * =============================================================================
 */

/*
 * cond_init - Initialize a condition variable
 *
 * @c: Pointer to the condition variable to initialize
 */
void cond_init(cond_t *c);

/*
 * cond_wait - Wait on a condition variable
 *
 * @c: Pointer to the condition variable
 * @m: Pointer to the mutex (must be locked by caller)
 *
 * Atomically releases the mutex and waits on the condition.
 * Re-acquires the mutex before returning.
 */
void cond_wait(cond_t *c, mutex_t *m);

/*
 * cond_signal - Wake one waiting thread
 *
 * @c: Pointer to the condition variable
 *
 * Wakes one thread waiting on this condition variable.
 */
void cond_signal(cond_t *c);

/*
 * cond_broadcast - Wake all waiting threads
 *
 * @c: Pointer to the condition variable
 *
 * Wakes all threads waiting on this condition variable.
 */
void cond_broadcast(cond_t *c);

/*
 * =============================================================================
 * Channel API (Part 2 - Extra Extra Credit)
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
 * channel_send - Send data through the channel
 *
 * @ch: Pointer to the channel
 * @data: Data pointer to send
 *
 * Returns: 0 on success, -1 if channel is closed
 *
 * Blocks if the channel is full.
 */
int channel_send(channel_t *ch, void *data);

/*
 * channel_recv - Receive data from the channel
 *
 * @ch: Pointer to the channel
 * @data: Pointer to store received data
 *
 * Returns: 0 on success, -1 if channel is closed and empty
 *
 * Blocks if the channel is empty.
 */
int channel_recv(channel_t *ch, void **data);

/*
 * channel_close - Close a channel
 *
 * @ch: Pointer to the channel
 *
 * Marks the channel as closed and wakes all waiting threads.
 */
void channel_close(channel_t *ch);

/*
 * channel_destroy - Free channel resources
 *
 * @ch: Pointer to the channel
 *
 * Frees all memory associated with the channel.
 */
void channel_destroy(channel_t *ch);

#endif /* UTHREADS_H */
