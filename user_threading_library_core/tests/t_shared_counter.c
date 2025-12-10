/*
 * t_shared_counter.c - Shared Counter Test (Part 2.2)
 *
 * Demonstrates:
 * 1. Race condition when multiple threads increment a shared counter
 *    WITHOUT mutex protection
 * 2. Correct behavior WITH mutex protection
 *
 * Requirements:
 * - 3 or more threads
 * - Each thread increments the counter 1000 times
 * - Verify: counter == num_threads * 1000
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_THREADS 3
#define INCREMENTS_PER_THREAD 1000

/* Shared counter */
int shared_counter = 0;

/* Mutex to protect the counter */
mutex_t counter_mutex;

/* Flag to control mutex usage */
int use_mutex = 0;

/*
 * Thread function: increment counter INCREMENTS_PER_THREAD times
 */
void *increment_thread(void *arg)
{
    int id = (int)arg;
    int i;
    int local_count = 0;

    printf(1, "Thread %d starting (tid=%d)\n", id, thread_self());

    for (i = 0; i < INCREMENTS_PER_THREAD; i++) {
        if (use_mutex) {
            mutex_lock(&counter_mutex);
        }

        /* Critical section: increment the shared counter */
        shared_counter++;
        local_count++;

        if (use_mutex) {
            mutex_unlock(&counter_mutex);
        }

        /* Yield occasionally to increase interleaving */
        if (i % 100 == 0) {
            thread_yield();
        }
    }

    printf(1, "Thread %d finished: performed %d increments\n", id, local_count);
    return (void*)local_count;
}

/*
 * Run the shared counter test
 */
void run_test(int with_mutex)
{
    int tids[NUM_THREADS];
    void *results[NUM_THREADS];
    int i;
    int expected = NUM_THREADS * INCREMENTS_PER_THREAD;

    /* Reset counter */
    shared_counter = 0;
    use_mutex = with_mutex;

    printf(1, "\n");
    printf(1, "====================================================\n");
    if (with_mutex) {
        printf(1, "  TEST: Shared Counter WITH Mutex Protection\n");
    } else {
        printf(1, "  TEST: Shared Counter WITHOUT Mutex Protection\n");
        printf(1, "  (Demonstrating Race Condition)\n");
    }
    printf(1, "====================================================\n");
    printf(1, "\n");

    printf(1, "Configuration:\n");
    printf(1, "  - Number of threads: %d\n", NUM_THREADS);
    printf(1, "  - Increments per thread: %d\n", INCREMENTS_PER_THREAD);
    printf(1, "  - Expected final count: %d\n", expected);
    printf(1, "\n");

    /* Create threads */
    printf(1, "Creating %d threads...\n", NUM_THREADS);
    for (i = 0; i < NUM_THREADS; i++) {
        tids[i] = thread_create(increment_thread, (void*)(i + 1));
        if (tids[i] < 0) {
            printf(1, "ERROR: Failed to create thread %d\n", i);
            return;
        }
    }

    /* Wait for all threads to complete */
    printf(1, "Waiting for threads to complete...\n\n");
    for (i = 0; i < NUM_THREADS; i++) {
        results[i] = thread_join(tids[i]);
    }

    /* Report results */
    printf(1, "\n");
    printf(1, "RESULTS:\n");
    printf(1, "  Final counter value: %d\n", shared_counter);
    printf(1, "  Expected value:      %d\n", expected);
    printf(1, "  Thread results: ");
    for (i = 0; i < NUM_THREADS; i++) {
        printf(1, "%d ", (int)results[i]);
    }
    printf(1, "\n");
    printf(1, "\n");

    if (shared_counter == expected) {
        printf(1, "  STATUS: PASS - Counter is correct!\n");
    } else {
        int lost = expected - shared_counter;
        printf(1, "  STATUS: FAIL - Counter is incorrect!\n");
        printf(1, "  Lost updates: %d (%.1d%% loss)\n",
               lost, (lost * 100) / expected);
        if (!with_mutex) {
            printf(1, "\n");
            printf(1, "  EXPLANATION: This demonstrates a race condition.\n");
            printf(1, "  Multiple threads read-modify-write the counter\n");
            printf(1, "  concurrently, causing some increments to be lost.\n");
        }
    }
    printf(1, "\n");
}

int main(int argc, char *argv[])
{
    printf(1, "\n");
    printf(1, "########################################################\n");
    printf(1, "#           SHARED COUNTER TEST (Part 2.2)             #\n");
    printf(1, "#                                                      #\n");
    printf(1, "# This test demonstrates the race condition that       #\n");
    printf(1, "# occurs when multiple threads access shared data      #\n");
    printf(1, "# without synchronization, and how mutexes fix it.     #\n");
    printf(1, "########################################################\n");

    /* Initialize threading system */
    thread_init();

    /* Initialize mutex */
    mutex_init(&counter_mutex);

    /* First run WITHOUT mutex - should show race condition */
    run_test(0);

    /* Second run WITH mutex - should be correct */
    run_test(1);

    printf(1, "########################################################\n");
    printf(1, "#                  TEST COMPLETE                       #\n");
    printf(1, "########################################################\n\n");

    exit();
}
