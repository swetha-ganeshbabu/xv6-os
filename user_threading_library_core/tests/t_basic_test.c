/*
 * t_basic_test.c - Basic thread functionality test
 *
 * Tests:
 * - thread_init
 * - thread_create
 * - thread_yield
 * - thread_join
 * - thread_exit
 * - thread_self
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

/* Thread function that prints and yields */
void *thread_func(void *arg)
{
    int id = (int)arg;
    int i;

    for (i = 0; i < 3; i++) {
        printf(1, "Thread %d: iteration %d (tid=%d)\n", id, i, thread_self());
        thread_yield();
    }

    printf(1, "Thread %d: exiting with value %d\n", id, id * 10);
    return (void*)(id * 10);
}

/* Thread function that returns immediately */
void *quick_thread(void *arg)
{
    int id = (int)arg;
    printf(1, "Quick thread %d: running and exiting\n", id);
    return (void*)(id + 100);
}

int main(int argc, char *argv[])
{
    int tid1, tid2, tid3;
    void *ret1, *ret2, *ret3;

    printf(1, "\n=== Basic Thread Test ===\n\n");

    /* Initialize threading system */
    printf(1, "Initializing threading system...\n");
    thread_init();
    printf(1, "Main thread TID: %d\n\n", thread_self());

    /* Test 1: Create and join single thread */
    printf(1, "--- Test 1: Single thread create/join ---\n");
    tid1 = thread_create(quick_thread, (void*)1);
    printf(1, "Created thread with TID: %d\n", tid1);

    ret1 = thread_join(tid1);
    printf(1, "Thread %d returned: %d\n\n", tid1, (int)ret1);

    /* Test 2: Create multiple threads that interleave */
    printf(1, "--- Test 2: Multiple interleaving threads ---\n");
    tid1 = thread_create(thread_func, (void*)1);
    tid2 = thread_create(thread_func, (void*)2);
    tid3 = thread_create(thread_func, (void*)3);

    printf(1, "Created threads: %d, %d, %d\n", tid1, tid2, tid3);
    printf(1, "Main thread yielding to let threads run...\n\n");

    /* Yield a few times to let threads interleave */
    thread_yield();
    thread_yield();
    thread_yield();

    /* Join all threads */
    printf(1, "\nMain thread joining threads...\n");
    ret1 = thread_join(tid1);
    ret2 = thread_join(tid2);
    ret3 = thread_join(tid3);

    printf(1, "\nThread %d returned: %d\n", tid1, (int)ret1);
    printf(1, "Thread %d returned: %d\n", tid2, (int)ret2);
    printf(1, "Thread %d returned: %d\n", tid3, (int)ret3);

    /* Verify return values */
    if ((int)ret1 == 10 && (int)ret2 == 20 && (int)ret3 == 30) {
        printf(1, "\n=== All tests PASSED ===\n");
    } else {
        printf(1, "\n=== Tests FAILED ===\n");
    }

    exit();
}
