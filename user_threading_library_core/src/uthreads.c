/*
 * uthreads.c - User-Level Threading Library Implementation for xv6
 *
 * This file implements the complete user-level threading library including:
 * - Thread management (create, join, exit, yield)
 * - Round-robin scheduler
 * - Synchronization primitives (mutexes, semaphores, condition variables)
 * - Channels for inter-thread communication
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

/*
 * =============================================================================
 * Global State
 * =============================================================================
 */

struct thread threads[MAX_THREADS];     // Global thread table
struct thread *current_thread;          // Currently running thread
int next_tid = 1;                       // Next TID to allocate
int threading_initialized = 0;          // Flag to track initialization

/*
 * =============================================================================
 * Wait Queue Helper Functions
 * =============================================================================
 */

/*
 * wait_queue_init - Initialize a wait queue
 */
static void wait_queue_init(struct wait_queue *wq) {
    wq->count = 0;
    wq->head = 0;
    wq->tail = 0;
    for (int i = 0; i < MAX_WAIT_QUEUE; i++) {
        wq->tids[i] = -1;
    }
}

/*
 * wait_queue_push - Add a thread to the wait queue
 * Returns 0 on success, -1 if queue is full
 */
static int wait_queue_push(struct wait_queue *wq, int tid) {
    if (wq->count >= MAX_WAIT_QUEUE) {
        return -1;  // Queue is full
    }
    wq->tids[wq->tail] = tid;
    wq->tail = (wq->tail + 1) % MAX_WAIT_QUEUE;
    wq->count++;
    return 0;
}

/*
 * wait_queue_pop - Remove and return the first thread from the wait queue
 * Returns TID on success, -1 if queue is empty
 */
static int wait_queue_pop(struct wait_queue *wq) {
    if (wq->count == 0) {
        return -1;  // Queue is empty
    }
    int tid = wq->tids[wq->head];
    wq->tids[wq->head] = -1;
    wq->head = (wq->head + 1) % MAX_WAIT_QUEUE;
    wq->count--;
    return tid;
}

/*
 * =============================================================================
 * Thread Wrapper Function
 * =============================================================================
 * This function is the entry point for new threads. It calls the user's
 * start_routine and then calls thread_exit with the return value.
 */
static void thread_entry(void) {
    // Call the user's start routine with its argument
    void *retval = current_thread->start_routine(current_thread->arg);
    // When it returns, exit the thread
    thread_exit(retval);
}

/*
 * =============================================================================
 * Thread Management Implementation (Part 1)
 * =============================================================================
 */

/*
 * thread_init - Initialize the threading system
 */
void thread_init(void) {
    if (threading_initialized) {
        return;  // Already initialized
    }

    // Initialize all thread slots as unused
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].tid = -1;
        threads[i].state = T_UNUSED;
        threads[i].sp = 0;
        threads[i].start_routine = 0;
        threads[i].arg = 0;
        threads[i].retval = 0;
        threads[i].join_tid = -1;
    }

    // Set up the main thread (thread 0)
    threads[0].tid = 0;
    threads[0].state = T_RUNNING;
    threads[0].join_tid = -1;
    // Main thread uses the existing process stack, so we don't set sp or stack

    current_thread = &threads[0];
    next_tid = 1;
    threading_initialized = 1;
}

/*
 * thread_create - Create a new thread
 */
int thread_create(void *(*start_routine)(void*), void *arg) {
    if (!threading_initialized) {
        return -1;  // Must call thread_init first
    }

    // Find an unused thread slot
    struct thread *t = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_UNUSED) {
            t = &threads[i];
            break;
        }
    }

    if (t == 0) {
        return -1;  // No available thread slots
    }

    // Initialize the thread
    t->tid = next_tid++;
    t->state = T_RUNNABLE;
    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = 0;
    t->join_tid = -1;

    /*
     * Set up the thread's stack for first context switch.
     * The stack grows downward on x86.
     *
     * Stack layout (from high to low addresses):
     *   [top of stack]
     *   return address (thread_entry)  <- will be popped by 'ret' in thread_switch
     *   saved %ebp
     *   saved %ebx
     *   saved %esi
     *   saved %edi
     *   [sp points here]
     */
    char *sp = t->stack + STACK_SIZE;

    // Align stack to 16-byte boundary (for x86 ABI)
    sp = (char*)((uint)sp & ~0xF);

    // Push return address (thread_entry function)
    sp -= 4;
    *(uint*)sp = (uint)thread_entry;

    // Push dummy saved registers (will be restored by thread_switch)
    sp -= 4;  // %ebp
    *(uint*)sp = 0;
    sp -= 4;  // %ebx
    *(uint*)sp = 0;
    sp -= 4;  // %esi
    *(uint*)sp = 0;
    sp -= 4;  // %edi
    *(uint*)sp = 0;

    t->sp = sp;

    return t->tid;
}

/*
 * thread_join - Wait for a thread to terminate
 */
void *thread_join(int tid) {
    if (!threading_initialized) {
        return 0;
    }

    // Find the thread with the given TID
    struct thread *t = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == tid && threads[i].state != T_UNUSED) {
            t = &threads[i];
            break;
        }
    }

    if (t == 0) {
        return 0;  // Thread not found
    }

    // Cannot join self
    if (t == current_thread) {
        return 0;
    }

    // Wait for the thread to finish if it hasn't already
    while (t->state != T_ZOMBIE) {
        // Register ourselves as waiting to join
        t->join_tid = current_thread->tid;
        // Block
        current_thread->state = T_SLEEPING;
        thread_schedule();
    }

    // Thread is now a zombie - collect its return value
    void *retval = t->retval;

    // Clean up the thread slot
    t->tid = -1;
    t->state = T_UNUSED;
    t->sp = 0;
    t->start_routine = 0;
    t->arg = 0;
    t->retval = 0;
    t->join_tid = -1;

    return retval;
}

/*
 * thread_exit - Terminate the current thread
 */
void thread_exit(void *retval) {
    if (!threading_initialized) {
        exit();  // Fall back to process exit
    }

    // Save return value
    current_thread->retval = retval;

    // Set state to zombie
    current_thread->state = T_ZOMBIE;

    // Wake up any thread waiting to join us
    if (current_thread->join_tid >= 0) {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == current_thread->join_tid &&
                threads[i].state == T_SLEEPING) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }

    // Check if all threads are done
    int all_done = 1;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_RUNNABLE || threads[i].state == T_RUNNING ||
            threads[i].state == T_SLEEPING) {
            all_done = 0;
            break;
        }
    }

    if (all_done) {
        // All threads done, exit the process
        exit();
    }

    // Schedule another thread
    thread_schedule();

    // Should never reach here
    exit();
}

/*
 * thread_self - Get the TID of the current thread
 */
int thread_self(void) {
    if (!threading_initialized || current_thread == 0) {
        return -1;
    }
    return current_thread->tid;
}

/*
 * thread_yield - Voluntarily give up the CPU
 */
void thread_yield(void) {
    if (!threading_initialized) {
        return;
    }

    // Mark current thread as runnable (it's not blocked, just yielding)
    current_thread->state = T_RUNNABLE;

    // Call scheduler to pick next thread
    thread_schedule();
}

/*
 * =============================================================================
 * Scheduler Implementation (Part 1)
 * =============================================================================
 */

/*
 * thread_schedule - Round-robin scheduler
 */
void thread_schedule(void) {
    if (!threading_initialized) {
        return;
    }

    struct thread *old = current_thread;
    struct thread *next = 0;

    // Find the next runnable thread using round-robin
    // Start searching from the thread after current_thread
    int start_idx = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (&threads[i] == current_thread) {
            start_idx = i;
            break;
        }
    }

    // Search for next runnable thread
    for (int i = 1; i <= MAX_THREADS; i++) {
        int idx = (start_idx + i) % MAX_THREADS;
        if (threads[idx].state == T_RUNNABLE) {
            next = &threads[idx];
            break;
        }
    }

    // If no other runnable thread found, check if current can continue
    if (next == 0) {
        if (old->state == T_RUNNABLE || old->state == T_RUNNING) {
            // Current thread can continue
            old->state = T_RUNNING;
            return;
        }
        // No runnable threads at all - this shouldn't happen if thread_exit is correct
        // but check if main thread is still waiting
        if (threads[0].state == T_SLEEPING) {
            // Wake main thread as last resort
            threads[0].state = T_RUNNABLE;
            next = &threads[0];
        } else {
            // Truly no threads to run
            exit();
        }
    }

    // Update states
    if (old->state == T_RUNNING) {
        old->state = T_RUNNABLE;
    }
    next->state = T_RUNNING;
    current_thread = next;

    // Perform context switch
    thread_switch(old, next);
}

/*
 * =============================================================================
 * Mutex Implementation (Part 2)
 * =============================================================================
 */

/*
 * mutex_init - Initialize a mutex
 */
void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner_tid = -1;
    wait_queue_init(&m->waiters);
}

/*
 * mutex_lock - Acquire a mutex
 */
void mutex_lock(mutex_t *m) {
    if (!threading_initialized) {
        return;
    }

    // Spin until we can acquire the lock
    while (m->locked) {
        // Add ourselves to the wait queue
        wait_queue_push(&m->waiters, current_thread->tid);
        // Block
        current_thread->state = T_SLEEPING;
        thread_schedule();
    }

    // Acquire the lock
    m->locked = 1;
    m->owner_tid = current_thread->tid;
}

/*
 * mutex_unlock - Release a mutex
 */
void mutex_unlock(mutex_t *m) {
    if (!threading_initialized) {
        return;
    }

    // Verify we own the lock
    if (m->owner_tid != current_thread->tid) {
        return;  // Error: not the owner
    }

    // Release the lock
    m->locked = 0;
    m->owner_tid = -1;

    // Wake up one waiting thread if any
    int tid = wait_queue_pop(&m->waiters);
    if (tid >= 0) {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == tid && threads[i].state == T_SLEEPING) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }
}

/*
 * =============================================================================
 * Semaphore Implementation (Part 2 - Extra Credit)
 * =============================================================================
 */

/*
 * sem_init - Initialize a semaphore
 */
void sem_init(sem_t *s, int value) {
    s->count = value;
    wait_queue_init(&s->waiters);
}

/*
 * sem_wait - Decrement semaphore (P operation)
 */
void sem_wait(sem_t *s) {
    if (!threading_initialized) {
        return;
    }

    s->count--;

    if (s->count < 0) {
        // Must block
        wait_queue_push(&s->waiters, current_thread->tid);
        current_thread->state = T_SLEEPING;
        thread_schedule();
    }
}

/*
 * sem_post - Increment semaphore (V operation)
 */
void sem_post(sem_t *s) {
    if (!threading_initialized) {
        return;
    }

    s->count++;

    if (s->count <= 0) {
        // There were waiters, wake one up
        int tid = wait_queue_pop(&s->waiters);
        if (tid >= 0) {
            for (int i = 0; i < MAX_THREADS; i++) {
                if (threads[i].tid == tid && threads[i].state == T_SLEEPING) {
                    threads[i].state = T_RUNNABLE;
                    break;
                }
            }
        }
    }
}

/*
 * =============================================================================
 * Condition Variable Implementation (Part 2 - Extra Credit)
 * =============================================================================
 */

/*
 * cond_init - Initialize a condition variable
 */
void cond_init(cond_t *c) {
    wait_queue_init(&c->waiters);
}

/*
 * cond_wait - Wait on a condition variable
 *
 * Must be called with mutex locked. Atomically releases mutex and waits.
 * Re-acquires mutex before returning.
 */
void cond_wait(cond_t *c, mutex_t *m) {
    if (!threading_initialized) {
        return;
    }

    // Add ourselves to the condition's wait queue BEFORE releasing the mutex
    // This ensures atomicity in the cooperative model
    wait_queue_push(&c->waiters, current_thread->tid);

    // Release the mutex
    mutex_unlock(m);

    // Block - in cooperative model, nothing can run between mutex_unlock and here
    current_thread->state = T_SLEEPING;
    thread_schedule();

    // When we wake up, re-acquire the mutex
    mutex_lock(m);
}

/*
 * cond_signal - Wake one waiting thread
 */
void cond_signal(cond_t *c) {
    if (!threading_initialized) {
        return;
    }

    int tid = wait_queue_pop(&c->waiters);
    if (tid >= 0) {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == tid && threads[i].state == T_SLEEPING) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }
}

/*
 * cond_broadcast - Wake all waiting threads
 */
void cond_broadcast(cond_t *c) {
    if (!threading_initialized) {
        return;
    }

    int tid;
    while ((tid = wait_queue_pop(&c->waiters)) >= 0) {
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threads[i].tid == tid && threads[i].state == T_SLEEPING) {
                threads[i].state = T_RUNNABLE;
                break;
            }
        }
    }
}

/*
 * =============================================================================
 * Channel Implementation (Part 2 - Extra Extra Credit)
 * =============================================================================
 */

/*
 * channel_create - Create a new channel
 */
channel_t* channel_create(int capacity) {
    if (capacity <= 0) {
        return 0;
    }

    // Allocate channel structure
    channel_t *ch = (channel_t*)malloc(sizeof(channel_t));
    if (ch == 0) {
        return 0;
    }

    // Allocate buffer
    ch->buffer = (void**)malloc(sizeof(void*) * capacity);
    if (ch->buffer == 0) {
        free(ch);
        return 0;
    }

    // Initialize fields
    ch->capacity = capacity;
    ch->count = 0;
    ch->head = 0;
    ch->tail = 0;
    ch->closed = 0;

    // Initialize synchronization primitives
    mutex_init(&ch->lock);
    cond_init(&ch->not_empty);
    cond_init(&ch->not_full);

    return ch;
}

/*
 * channel_send - Send data through the channel
 */
int channel_send(channel_t *ch, void *data) {
    if (ch == 0) {
        return -1;
    }

    mutex_lock(&ch->lock);

    // Wait while buffer is full and channel is open
    while (ch->count == ch->capacity && !ch->closed) {
        cond_wait(&ch->not_full, &ch->lock);
    }

    // Check if channel was closed while waiting
    if (ch->closed) {
        mutex_unlock(&ch->lock);
        return -1;
    }

    // Add data to buffer
    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;

    // Signal that buffer is not empty
    cond_signal(&ch->not_empty);

    mutex_unlock(&ch->lock);
    return 0;
}

/*
 * channel_recv - Receive data from the channel
 */
int channel_recv(channel_t *ch, void **data) {
    if (ch == 0 || data == 0) {
        return -1;
    }

    mutex_lock(&ch->lock);

    // Wait while buffer is empty and channel is open
    while (ch->count == 0 && !ch->closed) {
        cond_wait(&ch->not_empty, &ch->lock);
    }

    // If channel is closed and empty, return error
    if (ch->count == 0 && ch->closed) {
        mutex_unlock(&ch->lock);
        return -1;
    }

    // Remove data from buffer
    *data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;

    // Signal that buffer is not full
    cond_signal(&ch->not_full);

    mutex_unlock(&ch->lock);
    return 0;
}

/*
 * channel_close - Close a channel
 */
void channel_close(channel_t *ch) {
    if (ch == 0) {
        return;
    }

    mutex_lock(&ch->lock);

    ch->closed = 1;

    // Wake up all waiting threads
    cond_broadcast(&ch->not_empty);
    cond_broadcast(&ch->not_full);

    mutex_unlock(&ch->lock);
}

/*
 * channel_destroy - Free channel resources
 */
void channel_destroy(channel_t *ch) {
    if (ch == 0) {
        return;
    }

    if (ch->buffer != 0) {
        free(ch->buffer);
    }
    free(ch);
}
