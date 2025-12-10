/*
 * t_reader_writer.c - Reader-Writer Lock with Writer Priority
 *
 * Implements reader-writer synchronization with writer priority.
 * Once a writer is waiting, no new readers can start reading.
 *
 * Requirements from project:
 * - Multiple readers can access shared data simultaneously
 * - Writers get exclusive access (no readers or other writers)
 * - Writer Priority: Once a writer is waiting, no new readers should start
 * - At least 3 reader threads and 2 writer threads
 * - Each reader performs at least 5 read operations
 * - Each writer performs at least 3 write operations
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_READERS 3
#define NUM_WRITERS 2
#define READS_PER_READER 5
#define WRITES_PER_WRITER 3

/*
 * Reader-Writer Lock State Structure
 * This implements writer-priority policy
 */
typedef struct {
    int readers_reading;    // Number of readers currently reading
    int writers_waiting;    // Number of writers waiting
    int writer_writing;     // 1 if a writer is currently writing

    mutex_t lock;           // Protects the state variables
    cond_t readers_ok;      // Signaled when readers can proceed
    cond_t writers_ok;      // Signaled when a writer can proceed
} rwlock_t;

// Global reader-writer lock
rwlock_t rw;

// Shared data being protected
int shared_value = 0;

/*
 * Initialize the reader-writer lock
 */
void rwlock_init(rwlock_t *rw) {
    rw->readers_reading = 0;
    rw->writers_waiting = 0;
    rw->writer_writing = 0;

    mutex_init(&rw->lock);
    cond_init(&rw->readers_ok);
    cond_init(&rw->writers_ok);
}

/*
 * reader_lock - Acquire read access
 *
 * Writer Priority: If any writers are waiting, readers must wait.
 * Multiple readers can read simultaneously.
 */
void reader_lock(rwlock_t *rw) {
    mutex_lock(&rw->lock);

    // Wait if a writer is writing OR if writers are waiting (writer priority)
    while (rw->writer_writing || rw->writers_waiting > 0) {
        cond_wait(&rw->readers_ok, &rw->lock);
    }

    // We can now read
    rw->readers_reading++;

    mutex_unlock(&rw->lock);
}

/*
 * reader_unlock - Release read access
 *
 * If this is the last reader and writers are waiting, wake one writer.
 */
void reader_unlock(rwlock_t *rw) {
    mutex_lock(&rw->lock);

    rw->readers_reading--;

    // If no more readers and writers are waiting, wake a writer
    if (rw->readers_reading == 0 && rw->writers_waiting > 0) {
        cond_signal(&rw->writers_ok);
    }

    mutex_unlock(&rw->lock);
}

/*
 * writer_lock - Acquire write access
 *
 * Must wait until no readers are reading and no other writer is writing.
 */
void writer_lock(rwlock_t *rw) {
    mutex_lock(&rw->lock);

    // Announce that we're waiting (this blocks new readers - writer priority)
    rw->writers_waiting++;

    // Wait until no readers and no other writer
    while (rw->readers_reading > 0 || rw->writer_writing) {
        cond_wait(&rw->writers_ok, &rw->lock);
    }

    // We're no longer waiting, we're writing
    rw->writers_waiting--;
    rw->writer_writing = 1;

    mutex_unlock(&rw->lock);
}

/*
 * writer_unlock - Release write access
 *
 * If writers are waiting, wake one writer (writer priority).
 * Otherwise, wake all waiting readers.
 */
void writer_unlock(rwlock_t *rw) {
    mutex_lock(&rw->lock);

    rw->writer_writing = 0;

    // Prefer writers over readers (writer priority)
    if (rw->writers_waiting > 0) {
        cond_signal(&rw->writers_ok);
    } else {
        // No writers waiting, wake all readers
        cond_broadcast(&rw->readers_ok);
    }

    mutex_unlock(&rw->lock);
}

/*
 * Reader thread function
 */
void *reader(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < READS_PER_READER; i++) {
        reader_lock(&rw);

        // Read the shared value
        int value = shared_value;
        printf(1, "Reader %d: reading value = %d (readers active: %d)\n",
               id, value, rw.readers_reading);

        // Simulate some work while holding the lock
        thread_yield();

        reader_unlock(&rw);

        // Yield between operations
        thread_yield();
    }

    printf(1, "Reader %d: finished %d reads\n", id, READS_PER_READER);
    return 0;
}

/*
 * Writer thread function
 */
void *writer(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < WRITES_PER_WRITER; i++) {
        writer_lock(&rw);

        // Write a new value
        shared_value = id * 10 + i;
        printf(1, "Writer %d: wrote new value = %d\n", id, shared_value);

        // Simulate some work while holding the lock
        thread_yield();

        writer_unlock(&rw);

        // Yield between operations
        thread_yield();
        thread_yield();  // Extra yield to let readers catch up
    }

    printf(1, "Writer %d: finished %d writes\n", id, WRITES_PER_WRITER);
    return 0;
}

int main(void) {
    int reader_tids[NUM_READERS];
    int writer_tids[NUM_WRITERS];
    int i;

    printf(1, "=== Reader-Writer Lock with Writer Priority ===\n\n");
    printf(1, "Configuration:\n");
    printf(1, "  Readers: %d (each performs %d reads)\n",
           NUM_READERS, READS_PER_READER);
    printf(1, "  Writers: %d (each performs %d writes)\n",
           NUM_WRITERS, WRITES_PER_WRITER);
    printf(1, "  Writer Priority: YES (waiting writers block new readers)\n\n");

    // Initialize threading system
    thread_init();

    // Initialize reader-writer lock
    rwlock_init(&rw);

    printf(1, "Starting readers and writers...\n\n");

    // Create reader threads
    for (i = 0; i < NUM_READERS; i++) {
        reader_tids[i] = thread_create(reader, (void*)(uint)(i + 1));
        if (reader_tids[i] < 0) {
            printf(1, "ERROR: Failed to create reader %d\n", i);
            exit();
        }
    }

    // Create writer threads
    for (i = 0; i < NUM_WRITERS; i++) {
        writer_tids[i] = thread_create(writer, (void*)(uint)(i + 1));
        if (writer_tids[i] < 0) {
            printf(1, "ERROR: Failed to create writer %d\n", i);
            exit();
        }
    }

    // Wait for all threads to complete
    printf(1, "\nWaiting for all threads to complete...\n\n");

    for (i = 0; i < NUM_READERS; i++) {
        thread_join(reader_tids[i]);
    }

    for (i = 0; i < NUM_WRITERS; i++) {
        thread_join(writer_tids[i]);
    }

    // Final results
    printf(1, "\n=== Results ===\n");
    printf(1, "Final shared value: %d\n", shared_value);
    printf(1, "Total read operations: %d\n", NUM_READERS * READS_PER_READER);
    printf(1, "Total write operations: %d\n", NUM_WRITERS * WRITES_PER_WRITER);

    printf(1, "\nWriter Priority Policy:\n");
    printf(1, "  - Once a writer starts waiting, new readers are blocked\n");
    printf(1, "  - Writers are preferred over readers when lock is released\n");
    printf(1, "  - This prevents writer starvation\n");

    printf(1, "\n=== Reader-Writer Test Complete ===\n");

    exit();
}
