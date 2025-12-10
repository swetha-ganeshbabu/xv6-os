/*
 * t_basic_test.c - Basic Threading Library Test
 *
 * Tests the fundamental threading operations:
 * - Thread initialization
 * - Thread creation
 * - Thread yield
 * - Thread join
 * - Thread exit with return value
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_THREADS 3
#define ITERATIONS 5

/*
 * Simple thread function that prints its ID and yields several times
 */
void *thread_func(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < ITERATIONS; i++) {
        printf(1, "Thread %d: iteration %d\n", id, i);
        thread_yield();
    }

    printf(1, "Thread %d: exiting with return value %d\n", id, id * 10);
    return (void*)(uint)(id * 10);
}

/*
 * Thread function that tests thread_self()
 */
void *self_test_func(void *arg) {
    int expected_tid = (int)(uint)arg;
    int actual_tid = thread_self();

    printf(1, "Self test: expected tid around %d, got %d\n", expected_tid, actual_tid);

    return (void*)(uint)actual_tid;
}

int main(void) {
    int tids[NUM_THREADS];
    void *retvals[NUM_THREADS];
    int i;

    printf(1, "=== Basic Threading Library Test ===\n\n");

    // Initialize threading system
    printf(1, "Step 1: Initializing threading system...\n");
    thread_init();
    printf(1, "Main thread TID: %d\n\n", thread_self());

    // Test 1: Create multiple threads
    printf(1, "Step 2: Creating %d threads...\n", NUM_THREADS);
    for (i = 0; i < NUM_THREADS; i++) {
        tids[i] = thread_create(thread_func, (void*)(uint)(i + 1));
        if (tids[i] < 0) {
            printf(1, "ERROR: Failed to create thread %d\n", i);
            exit();
        }
        printf(1, "Created thread with TID %d\n", tids[i]);
    }
    printf(1, "\n");

    // Let threads run by yielding from main
    printf(1, "Step 3: Main thread yielding to let other threads run...\n\n");
    for (i = 0; i < ITERATIONS * NUM_THREADS; i++) {
        thread_yield();
    }

    // Test 2: Join threads and collect return values
    printf(1, "\nStep 4: Joining threads and collecting return values...\n");
    for (i = 0; i < NUM_THREADS; i++) {
        retvals[i] = thread_join(tids[i]);
        printf(1, "Thread %d returned: %d (expected: %d)\n",
               tids[i], (int)(uint)retvals[i], (i + 1) * 10);
    }

    // Verify return values
    printf(1, "\nStep 5: Verifying return values...\n");
    int all_passed = 1;
    for (i = 0; i < NUM_THREADS; i++) {
        int expected = (i + 1) * 10;
        int actual = (int)(uint)retvals[i];
        if (actual != expected) {
            printf(1, "FAIL: Thread %d returned %d, expected %d\n",
                   tids[i], actual, expected);
            all_passed = 0;
        }
    }

    if (all_passed) {
        printf(1, "All return values correct!\n");
    }

    // Test 3: Test thread_self()
    printf(1, "\nStep 6: Testing thread_self()...\n");
    int self_tid = thread_create(self_test_func, (void*)(uint)4);
    if (self_tid < 0) {
        printf(1, "ERROR: Failed to create self-test thread\n");
        exit();
    }
    thread_yield();
    void *self_ret = thread_join(self_tid);
    printf(1, "Self-test thread returned its TID: %d\n", (int)(uint)self_ret);

    printf(1, "\n=== Basic Threading Test Complete ===\n");

    exit();
}
