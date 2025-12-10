/*
 * uthreads.c - User-Level Threading Library Implementation for xv6
 *
 * This file implements a cooperative user-level threading library.
 * All thread management happens in user space - the kernel is unaware
 * of the existence of these threads.
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

/*
 * =============================================================================
 * GLOBAL STATE
 * =============================================================================
 */

/* Thread table - all thread control blocks */
struct thread threads[MAX_THREADS];

/* Pointer to the currently running thread */
struct thread *current_thread = 0;

/* Next thread ID to allocate */
static int next_tid = 1;

/* Index of current thread in round-robin scheduling */
static int current_index = 0;

/* Flag to track if threading system is initialized */
static int threading_initialized = 0;

/* External assembly function declarations */
extern void thread_entry(void);
extern void thread_switch(struct thread *old, struct thread *next);

/*
 * =============================================================================
 * HELPER FUNCTIONS
 * =============================================================================
 */

/*
 * Allocate a thread slot from the thread table.
 * Returns pointer to thread or 0 if no slots available.
 */
static struct thread *alloc_thread(void)
{
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_UNUSED) {
            return &threads[i];
        }
    }
    return 0;  /* No free slots */
}

/*
 * Find a thread by TID.
 * Returns pointer to thread or 0 if not found.
 */
static struct thread *find_thread(int tid)
{
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == tid && threads[i].state != T_UNUSED) {
            return &threads[i];
        }
    }
    return 0;
}

/*
 * =============================================================================
 * CORE THREADING API IMPLEMENTATION (Part 1)
 * =============================================================================
 */

/*
 * thread_init - Initialize the threading system
 *
 * Sets up the thread table and registers the main program as thread 0.
 * Must be called before any other threading functions.
 */
void thread_init(void)
{
    int i;

    if (threading_initialized) {
        return;  /* Already initialized */
    }

    /* Initialize all thread slots to unused */
    for (i = 0; i < MAX_THREADS; i++) {
        threads[i].tid = 0;
        threads[i].state = T_UNUSED;
        threads[i].stack = 0;
        threads[i].sp = 0;
        threads[i].start_routine = 0;
        threads[i].arg = 0;
        threads[i].retval = 0;
        threads[i].join_tid = -1;
        threads[i].waiting_on = -1;
    }

    /*
     * Set up the main thread (thread 0).
     * The main thread is already running, so it doesn't need a separate stack.
     * We mark it as T_RUNNING.
     */
    threads[0].tid = next_tid++;
    threads[0].state = T_RUNNING;
    threads[0].stack = 0;  /* Main uses the process stack */
    threads[0].sp = 0;     /* Will be saved on first switch */
    threads[0].join_tid = -1;
    threads[0].waiting_on = -1;

    current_thread = &threads[0];
    current_index = 0;

    threading_initialized = 1;
}

/*
 * thread_create - Create a new thread
 *
 * Allocates a new thread, sets up its stack, and marks it as runnable.
 * Returns the new thread's TID on success, -1 on failure.
 */
int thread_create(void* (*start_routine)(void*), void *arg)
{
    struct thread *t;
    char *sp;

    if (!threading_initialized) {
        return -1;
    }

    /* Allocate a thread slot */
    t = alloc_thread();
    if (t == 0) {
        return -1;  /* No free slots */
    }

    /* Allocate stack for the new thread */
    t->stack = malloc(STACK_SIZE);
    if (t->stack == 0) {
        return -1;  /* Out of memory */
    }

    /* Initialize thread fields */
    t->tid = next_tid++;
    t->start_routine = start_routine;
    t->arg = arg;
    t->retval = 0;
    t->join_tid = -1;
    t->waiting_on = -1;

    /*
     * Set up the initial stack for the new thread.
     * The stack grows downward on x86, so we start at the top.
     *
     * Stack layout (growing downward, higher addresses first):
     *
     * [Top of stack]
     * +------------------+ <- stack + STACK_SIZE
     * | thread_exit addr | <- "return address" for start_routine
     * +------------------+
     * | arg              | <- argument for start_routine
     * +------------------+
     * | start_routine    | <- to be popped by thread_entry
     * +------------------+
     * | (fake ret addr)  | <- return address for thread_entry (never used)
     * +------------------+
     * | fake EBP         | <- saved EBP (will be popped first)
     * +------------------+
     * | fake EBX         | <- saved EBX
     * +------------------+
     * | fake ESI         | <- saved ESI
     * +------------------+
     * | fake EDI         | <- saved EDI <- sp points here
     * +------------------+
     * [Bottom of stack space]
     */

    sp = t->stack + STACK_SIZE;

    /* Push thread_exit as return address for when start_routine returns */
    sp -= sizeof(void*);
    *(void**)sp = (void*)thread_exit;

    /* Push the argument to start_routine */
    sp -= sizeof(void*);
    *(void**)sp = arg;

    /* Push the start_routine address */
    sp -= sizeof(void*);
    *(void**)sp = (void*)start_routine;

    /* Push fake return address for thread_entry (never used) */
    sp -= sizeof(void*);
    *(void**)sp = 0;

    /* Push fake callee-saved registers (will be popped by thread_switch) */
    sp -= sizeof(void*);
    *(void**)sp = 0;  /* EBP */

    sp -= sizeof(void*);
    *(void**)sp = 0;  /* EBX */

    sp -= sizeof(void*);
    *(void**)sp = 0;  /* ESI */

    sp -= sizeof(void*);
    *(void**)sp = 0;  /* EDI */

    /* Save the stack pointer */
    t->sp = sp;

    /* Set the thread state to runnable, but modify return address */
    /* We need thread_entry to be called first, so push it as return addr */
    /* Actually, we need to set up so that when thread_switch returns,
       it goes to thread_entry */

    /* Re-setup: the return address for thread_switch should be thread_entry */
    sp = t->stack + STACK_SIZE;

    /* Push thread_exit as return address for when start_routine returns */
    sp -= sizeof(void*);
    *(void**)sp = (void*)thread_exit;

    /* Push the argument to start_routine */
    sp -= sizeof(void*);
    *(void**)sp = arg;

    /* Push the start_routine address */
    sp -= sizeof(void*);
    *(void**)sp = (void*)start_routine;

    /* Push thread_entry as the return address for thread_switch */
    sp -= sizeof(void*);
    *(void**)sp = (void*)thread_entry;

    /* Push fake callee-saved registers */
    sp -= sizeof(void*);
    *(void**)sp = 0;  /* EBP */

    sp -= sizeof(void*);
    *(void**)sp = 0;  /* EBX */

    sp -= sizeof(void*);
    *(void**)sp = 0;  /* ESI */

    sp -= sizeof(void*);
    *(void**)sp = 0;  /* EDI */

    t->sp = sp;
    t->state = T_RUNNABLE;

    return t->tid;
}

/*
 * thread_exit - Terminate the current thread
 *
 * Sets the return value, transitions to T_ZOMBIE state,
 * wakes any thread waiting to join, and yields to scheduler.
 * This function does not return.
 */
void thread_exit(void *retval)
{
    struct thread *t;
    int i;

    if (!current_thread) {
        exit();  /* Main thread exiting, exit process */
    }

    /* Save return value */
    current_thread->retval = retval;

    /* Wake up any thread waiting to join this one */
    for (i = 0; i < MAX_THREADS; i++) {
        t = &threads[i];
        if (t->state == T_SLEEPING && t->waiting_on == current_thread->tid) {
            t->state = T_RUNNABLE;
            t->waiting_on = -1;
        }
    }

    /* Transition to zombie state */
    current_thread->state = T_ZOMBIE;

    /* Yield to another thread - this never returns */
    thread_schedule();

    /* Should never reach here */
    exit();
}

/*
 * thread_join - Wait for a thread to terminate
 *
 * Blocks until the target thread exits, then collects its return value
 * and cleans up its resources.
 */
void *thread_join(int tid)
{
    struct thread *target;
    void *retval;

    if (!threading_initialized) {
        return 0;
    }

    target = find_thread(tid);
    if (target == 0) {
        return 0;  /* Thread not found */
    }

    /* Can't join yourself */
    if (target == current_thread) {
        return 0;
    }

    /* Wait for the thread to become a zombie */
    while (target->state != T_ZOMBIE) {
        /* Mark ourselves as waiting */
        current_thread->state = T_SLEEPING;
        current_thread->waiting_on = tid;

        /* Yield to let other threads run */
        thread_schedule();
    }

    /* Collect return value */
    retval = target->retval;

    /* Clean up the thread */
    if (target->stack) {
        free(target->stack);
        target->stack = 0;
    }
    target->state = T_UNUSED;
    target->tid = 0;

    return retval;
}

/*
 * thread_self - Get current thread's TID
 */
int thread_self(void)
{
    if (current_thread) {
        return current_thread->tid;
    }
    return -1;
}

/*
 * thread_yield - Voluntarily give up the CPU
 *
 * Marks the current thread as runnable and calls the scheduler.
 */
void thread_yield(void)
{
    if (!threading_initialized || !current_thread) {
        return;
    }

    /* Only change state if we're currently running */
    if (current_thread->state == T_RUNNING) {
        current_thread->state = T_RUNNABLE;
    }

    thread_schedule();
}

/*
 * thread_schedule - Select and switch to the next runnable thread
 *
 * Implements round-robin scheduling: starts searching from the thread
 * after the current one and wraps around.
 */
void thread_schedule(void)
{
    struct thread *old, *next;
    int i, idx;

    if (!threading_initialized) {
        return;
    }

    old = current_thread;

    /*
     * Round-robin scheduling: start from the thread after current
     * and find the first runnable thread.
     */
    next = 0;
    for (i = 1; i <= MAX_THREADS; i++) {
        idx = (current_index + i) % MAX_THREADS;
        if (threads[idx].state == T_RUNNABLE) {
            next = &threads[idx];
            current_index = idx;
            break;
        }
    }

    /* If no runnable thread found, check if current can continue */
    if (next == 0) {
        if (old && old->state == T_RUNNING) {
            /* Current thread can continue */
            return;
        }
        /* No runnable threads at all */
        if (old && old->state == T_ZOMBIE) {
            /* Current thread is zombie and no others to run - exit process */
            exit();
        }
        return;
    }

    /* If the next thread is the same as current, no switch needed */
    if (next == old && old->state == T_RUNNING) {
        return;
    }

    /* Update states */
    if (old && old->state == T_RUNNING) {
        old->state = T_RUNNABLE;
    }
    next->state = T_RUNNING;
    current_thread = next;

    /* Perform the context switch */
    if (old) {
        thread_switch(old, next);
    }
}

/*
 * =============================================================================
 * MUTEX IMPLEMENTATION (Part 2.1)
 * =============================================================================
 */

/*
 * mutex_init - Initialize a mutex
 */
void mutex_init(mutex_t *m)
{
    int i;
    m->locked = 0;
    m->owner_tid = -1;
    m->wait_count = 0;
    for (i = 0; i < MAX_WAITERS; i++) {
        m->wait_queue[i] = -1;
    }
}

/*
 * mutex_lock - Acquire a mutex
 *
 * If the mutex is unlocked, acquires it immediately.
 * If locked, blocks until it becomes available.
 */
void mutex_lock(mutex_t *m)
{
    if (!threading_initialized || !current_thread) {
        return;
    }

    while (m->locked) {
        /* Mutex is locked, add ourselves to wait queue */
        if (m->wait_count < MAX_WAITERS) {
            m->wait_queue[m->wait_count++] = current_thread->tid;
        }

        /* Block and yield */
        current_thread->state = T_SLEEPING;
        thread_schedule();

        /* When we wake up, try again */
    }

    /* Acquire the lock */
    m->locked = 1;
    m->owner_tid = current_thread->tid;
}

/*
 * mutex_unlock - Release a mutex
 *
 * Releases the mutex and wakes one waiting thread if any.
 */
void mutex_unlock(mutex_t *m)
{
    struct thread *t;
    int i, tid;

    if (!threading_initialized || !current_thread) {
        return;
    }

    /* Verify we own the lock */
    if (!m->locked || m->owner_tid != current_thread->tid) {
        return;  /* We don't own this lock */
    }

    /* Release the lock */
    m->locked = 0;
    m->owner_tid = -1;

    /* Wake up one waiting thread (FIFO order) */
    if (m->wait_count > 0) {
        tid = m->wait_queue[0];

        /* Shift the queue */
        for (i = 0; i < m->wait_count - 1; i++) {
            m->wait_queue[i] = m->wait_queue[i + 1];
        }
        m->wait_count--;

        /* Wake the thread */
        t = find_thread(tid);
        if (t && t->state == T_SLEEPING) {
            t->state = T_RUNNABLE;
        }
    }
}

/*
 * =============================================================================
 * SEMAPHORE IMPLEMENTATION (Part 2.3 - Extra Credit)
 * =============================================================================
 */

/*
 * sem_init - Initialize a semaphore
 */
void sem_init(sem_t *s, int value)
{
    int i;
    s->count = value;
    s->wait_count = 0;
    for (i = 0; i < MAX_WAITERS; i++) {
        s->wait_queue[i] = -1;
    }
}

/*
 * sem_wait - Decrement semaphore (P operation)
 *
 * Blocks if count would become negative.
 */
void sem_wait(sem_t *s)
{
    if (!threading_initialized || !current_thread) {
        return;
    }

    /* Decrement count */
    s->count--;

    /* If count is negative, block */
    if (s->count < 0) {
        /* Add to wait queue */
        if (s->wait_count < MAX_WAITERS) {
            s->wait_queue[s->wait_count++] = current_thread->tid;
        }

        /* Block and yield */
        current_thread->state = T_SLEEPING;
        thread_schedule();
    }
}

/*
 * sem_post - Increment semaphore (V operation)
 *
 * Wakes one waiting thread if any.
 */
void sem_post(sem_t *s)
{
    struct thread *t;
    int i, tid;

    if (!threading_initialized) {
        return;
    }

    /* Increment count */
    s->count++;

    /* If threads are waiting, wake one */
    if (s->wait_count > 0) {
        tid = s->wait_queue[0];

        /* Shift the queue */
        for (i = 0; i < s->wait_count - 1; i++) {
            s->wait_queue[i] = s->wait_queue[i + 1];
        }
        s->wait_count--;

        /* Wake the thread */
        t = find_thread(tid);
        if (t && t->state == T_SLEEPING) {
            t->state = T_RUNNABLE;
        }
    }
}

/*
 * =============================================================================
 * CONDITION VARIABLE IMPLEMENTATION (Part 2.4 - Extra Credit)
 * =============================================================================
 */

/*
 * cond_init - Initialize a condition variable
 */
void cond_init(cond_t *c)
{
    int i;
    c->wait_count = 0;
    for (i = 0; i < MAX_WAITERS; i++) {
        c->wait_queue[i] = -1;
    }
}

/*
 * cond_wait - Wait on a condition variable
 *
 * Must be called with mutex held. Atomically releases mutex and blocks.
 * Re-acquires mutex before returning.
 */
void cond_wait(cond_t *c, mutex_t *m)
{
    if (!threading_initialized || !current_thread) {
        return;
    }

    /* Add to wait queue */
    if (c->wait_count < MAX_WAITERS) {
        c->wait_queue[c->wait_count++] = current_thread->tid;
    }

    /*
     * Atomically release mutex and sleep.
     * In our cooperative model, no other thread runs between
     * mutex_unlock and setting our state to SLEEPING because
     * there's no preemption.
     */

    /* Release the mutex */
    mutex_unlock(m);

    /* Block */
    current_thread->state = T_SLEEPING;
    thread_schedule();

    /* When we wake up, re-acquire the mutex */
    mutex_lock(m);
}

/*
 * cond_signal - Wake one waiting thread
 */
void cond_signal(cond_t *c)
{
    struct thread *t;
    int i, tid;

    if (c->wait_count > 0) {
        tid = c->wait_queue[0];

        /* Shift the queue */
        for (i = 0; i < c->wait_count - 1; i++) {
            c->wait_queue[i] = c->wait_queue[i + 1];
        }
        c->wait_count--;

        /* Wake the thread */
        t = find_thread(tid);
        if (t && t->state == T_SLEEPING) {
            t->state = T_RUNNABLE;
        }
    }
}

/*
 * cond_broadcast - Wake all waiting threads
 */
void cond_broadcast(cond_t *c)
{
    struct thread *t;
    int i, tid;

    /* Wake all threads in the queue */
    for (i = 0; i < c->wait_count; i++) {
        tid = c->wait_queue[i];
        t = find_thread(tid);
        if (t && t->state == T_SLEEPING) {
            t->state = T_RUNNABLE;
        }
    }

    /* Clear the queue */
    c->wait_count = 0;
}

/*
 * =============================================================================
 * CHANNEL IMPLEMENTATION (Part 2.5 - Extra Credit)
 * =============================================================================
 */

/*
 * channel_create - Create a new channel
 */
channel_t* channel_create(int capacity)
{
    channel_t *ch;

    ch = malloc(sizeof(channel_t));
    if (ch == 0) {
        return 0;
    }

    ch->buffer = malloc(sizeof(void*) * capacity);
    if (ch->buffer == 0) {
        free(ch);
        return 0;
    }

    ch->capacity = capacity;
    ch->count = 0;
    ch->read_pos = 0;
    ch->write_pos = 0;
    ch->closed = 0;

    mutex_init(&ch->lock);
    cond_init(&ch->not_empty);
    cond_init(&ch->not_full);

    return ch;
}

/*
 * channel_send - Send data through channel
 *
 * Blocks if channel is full. Returns -1 if channel is closed.
 */
int channel_send(channel_t *ch, void *data)
{
    mutex_lock(&ch->lock);

    /* Wait while channel is full and not closed */
    while (ch->count == ch->capacity && !ch->closed) {
        cond_wait(&ch->not_full, &ch->lock);
    }

    /* Check if channel was closed */
    if (ch->closed) {
        mutex_unlock(&ch->lock);
        return -1;
    }

    /* Add data to buffer */
    ch->buffer[ch->write_pos] = data;
    ch->write_pos = (ch->write_pos + 1) % ch->capacity;
    ch->count++;

    /* Signal that data is available */
    cond_signal(&ch->not_empty);

    mutex_unlock(&ch->lock);
    return 0;
}

/*
 * channel_recv - Receive data from channel
 *
 * Blocks if channel is empty. Returns -1 if channel is closed and empty.
 */
int channel_recv(channel_t *ch, void **data)
{
    mutex_lock(&ch->lock);

    /* Wait while channel is empty and not closed */
    while (ch->count == 0 && !ch->closed) {
        cond_wait(&ch->not_empty, &ch->lock);
    }

    /* Check if channel is closed and empty */
    if (ch->count == 0 && ch->closed) {
        mutex_unlock(&ch->lock);
        return -1;
    }

    /* Get data from buffer */
    *data = ch->buffer[ch->read_pos];
    ch->read_pos = (ch->read_pos + 1) % ch->capacity;
    ch->count--;

    /* Signal that space is available */
    cond_signal(&ch->not_full);

    mutex_unlock(&ch->lock);
    return 0;
}

/*
 * channel_close - Close a channel
 *
 * Wakes all blocked senders and receivers.
 */
void channel_close(channel_t *ch)
{
    mutex_lock(&ch->lock);

    ch->closed = 1;

    /* Wake all waiting threads */
    cond_broadcast(&ch->not_empty);
    cond_broadcast(&ch->not_full);

    mutex_unlock(&ch->lock);
}

/*
 * channel_destroy - Free channel resources
 */
void channel_destroy(channel_t *ch)
{
    if (ch) {
        if (ch->buffer) {
            free(ch->buffer);
        }
        free(ch);
    }
}

/*
 * =============================================================================
 * READER-WRITER LOCK IMPLEMENTATION (Part 3.2 - Extra Credit)
 * =============================================================================
 */

/*
 * rwlock_init - Initialize a reader-writer lock
 */
void rwlock_init(rwlock_t *rw)
{
    rw->readers = 0;
    rw->writers = 0;
    rw->waiting_writers = 0;

    mutex_init(&rw->lock);
    cond_init(&rw->read_ok);
    cond_init(&rw->write_ok);
}

/*
 * reader_lock - Acquire read lock
 *
 * Writer priority: blocks if a writer is active OR waiting.
 */
void reader_lock(rwlock_t *rw)
{
    mutex_lock(&rw->lock);

    /* Wait while a writer is active or writers are waiting */
    while (rw->writers > 0 || rw->waiting_writers > 0) {
        cond_wait(&rw->read_ok, &rw->lock);
    }

    /* Increment reader count */
    rw->readers++;

    mutex_unlock(&rw->lock);
}

/*
 * reader_unlock - Release read lock
 */
void reader_unlock(rwlock_t *rw)
{
    mutex_lock(&rw->lock);

    rw->readers--;

    /* If this was the last reader and writers are waiting, wake one */
    if (rw->readers == 0 && rw->waiting_writers > 0) {
        cond_signal(&rw->write_ok);
    }

    mutex_unlock(&rw->lock);
}

/*
 * writer_lock - Acquire write lock
 *
 * Blocks if any readers or writers are active.
 */
void writer_lock(rwlock_t *rw)
{
    mutex_lock(&rw->lock);

    /* Indicate we're waiting */
    rw->waiting_writers++;

    /* Wait while readers or writers are active */
    while (rw->readers > 0 || rw->writers > 0) {
        cond_wait(&rw->write_ok, &rw->lock);
    }

    /* We're no longer waiting, we're active */
    rw->waiting_writers--;
    rw->writers = 1;

    mutex_unlock(&rw->lock);
}

/*
 * writer_unlock - Release write lock
 */
void writer_unlock(rwlock_t *rw)
{
    mutex_lock(&rw->lock);

    rw->writers = 0;

    /* If writers are waiting, wake one writer */
    if (rw->waiting_writers > 0) {
        cond_signal(&rw->write_ok);
    } else {
        /* Otherwise, wake all waiting readers */
        cond_broadcast(&rw->read_ok);
    }

    mutex_unlock(&rw->lock);
}
