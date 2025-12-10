/*
 * t_shared_counter.c - Shared Counter Test with Mutex
 *
 * This test demonstrates:
 * 1. Race condition without mutex (counter loses increments)
 * 2. Correct operation with mutex (counter is accurate)
 *
 * Requirements from project:
 * - Create a shared integer counter initialized to 0
 * - Create 3 or more threads, each incrementing the counter 1000 times
 * - Protect all counter access with mutex
 * - After all threads complete, verify: counter == num_threads * 1000
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_THREADS 3
#define INCREMENTS_PER_THREAD 1000

// Shared counter
int counter_no_mutex = 0;
int counter_with_mutex = 0;

// Mutex for protecting counter
mutex_t counter_lock;

/*
 * Thread function WITHOUT mutex protection (demonstrates race condition)
 */
void *increment_no_mutex(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < INCREMENTS_PER_THREAD; i++) {
        // Read-modify-write without protection
        int temp = counter_no_mutex;
        // Yield to simulate interleaving that exposes race condition
        if (i % 100 == 0) {
            thread_yield();
        }
        counter_no_mutex = temp + 1;
    }

    printf(1, "Thread %d (no mutex): finished incrementing\n", id);
    return 0;
}

/*
 * Thread function WITH mutex protection (correct behavior)
 */
void *increment_with_mutex(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < INCREMENTS_PER_THREAD; i++) {
        mutex_lock(&counter_lock);

        // Read-modify-write with protection
        int temp = counter_with_mutex;
        // Yield inside critical section to test mutex correctness
        if (i % 100 == 0) {
            thread_yield();
        }
        counter_with_mutex = temp + 1;

        mutex_unlock(&counter_lock);
    }

    printf(1, "Thread %d (with mutex): finished incrementing\n", id);
    return 0;
}

int main(void) {
    int tids[NUM_THREADS];
    int i;
    int expected = NUM_THREADS * INCREMENTS_PER_THREAD;

    printf(1, "=== Shared Counter Test ===\n\n");
    printf(1, "Configuration:\n");
    printf(1, "  Number of threads: %d\n", NUM_THREADS);
    printf(1, "  Increments per thread: %d\n", INCREMENTS_PER_THREAD);
    printf(1, "  Expected final counter: %d\n\n", expected);

    // Initialize threading system
    thread_init();

    // Initialize mutex
    mutex_init(&counter_lock);

    // =====================================================
    // Test 1: Without mutex (demonstrates race condition)
    // =====================================================
    printf(1, "--- Test 1: WITHOUT Mutex (Race Condition Demo) ---\n");
    counter_no_mutex = 0;

    // Create threads
    for (i = 0; i < NUM_THREADS; i++) {
        tids[i] = thread_create(increment_no_mutex, (void*)(uint)(i + 1));
        if (tids[i] < 0) {
            printf(1, "ERROR: Failed to create thread %d\n", i);
            exit();
        }
    }

    // Wait for all threads to complete
    for (i = 0; i < NUM_THREADS; i++) {
        thread_join(tids[i]);
    }

    printf(1, "\nResults WITHOUT mutex:\n");
    printf(1, "  Final counter value: %d\n", counter_no_mutex);
    printf(1, "  Expected value: %d\n", expected);
    printf(1, "  Lost increments: %d\n", expected - counter_no_mutex);

    if (counter_no_mutex == expected) {
        printf(1, "  Status: PASS (got lucky, no race detected)\n");
    } else {
        printf(1, "  Status: RACE CONDITION DETECTED (as expected)\n");
    }

    // =====================================================
    // Test 2: With mutex (correct behavior)
    // =====================================================
    printf(1, "\n--- Test 2: WITH Mutex (Protected) ---\n");
    counter_with_mutex = 0;

    // Create threads
    for (i = 0; i < NUM_THREADS; i++) {
        tids[i] = thread_create(increment_with_mutex, (void*)(uint)(i + 1));
        if (tids[i] < 0) {
            printf(1, "ERROR: Failed to create thread %d\n", i);
            exit();
        }
    }

    // Wait for all threads to complete
    for (i = 0; i < NUM_THREADS; i++) {
        thread_join(tids[i]);
    }

    printf(1, "\nResults WITH mutex:\n");
    printf(1, "  Final counter value: %d\n", counter_with_mutex);
    printf(1, "  Expected value: %d\n", expected);

    if (counter_with_mutex == expected) {
        printf(1, "  Status: PASS - Mutex correctly prevented race condition!\n");
    } else {
        printf(1, "  Status: FAIL - Counter mismatch (mutex bug?)\n");
    }

    // =====================================================
    // Summary
    // =====================================================
    printf(1, "\n=== Test Summary ===\n");
    printf(1, "Without mutex: %d/%d increments recorded (%d lost)\n",
           counter_no_mutex, expected, expected - counter_no_mutex);
    printf(1, "With mutex:    %d/%d increments recorded\n",
           counter_with_mutex, expected);

    if (counter_with_mutex == expected) {
        printf(1, "\nMutex implementation: WORKING CORRECTLY\n");
    } else {
        printf(1, "\nMutex implementation: NEEDS DEBUGGING\n");
    }

    printf(1, "\n=== Shared Counter Test Complete ===\n");

    exit();
}
