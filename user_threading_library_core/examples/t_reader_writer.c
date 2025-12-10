/*
 * t_reader_writer.c - Reader-Writer Lock with Writer Priority (Part 3.2)
 *
 * Implements and demonstrates reader-writer synchronization with writer priority.
 *
 * Requirements:
 * - Multiple readers can access shared data simultaneously
 * - Writers get exclusive access (no readers or other writers)
 * - Writer Priority: Once a writer is waiting, no new readers start
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

/* Shared data protected by the rwlock */
int shared_value = 0;

/* Reader-writer lock with writer priority */
rwlock_t rwlock;

/* Statistics tracking */
int total_reads = 0;
int total_writes = 0;
mutex_t stats_mutex;

/*
 * Reader thread function
 */
void *reader_thread(void *arg)
{
    int id = (int)arg;
    int i;
    int value;
    int read_count = 0;

    printf(1, "Reader %d: started\n", id);

    for (i = 0; i < READS_PER_READER; i++) {
        /* Acquire read lock */
        reader_lock(&rwlock);

        /* Read shared data */
        value = shared_value;
        read_count++;

        printf(1, "Reader %d: read value = %d (read #%d)\n", id, value, i + 1);

        /* Release read lock */
        reader_unlock(&rwlock);

        /* Update stats */
        mutex_lock(&stats_mutex);
        total_reads++;
        mutex_unlock(&stats_mutex);

        /* Yield to allow interleaving */
        thread_yield();

        /* Small delay simulation */
        int j;
        for (j = 0; j < 100; j++) {
            /* busy wait */
        }
    }

    printf(1, "Reader %d: finished (%d reads)\n", id, read_count);
    return (void*)read_count;
}

/*
 * Writer thread function
 */
void *writer_thread(void *arg)
{
    int id = (int)arg;
    int i;
    int new_value;
    int write_count = 0;

    printf(1, "Writer %d: started\n", id);

    for (i = 0; i < WRITES_PER_WRITER; i++) {
        /* Acquire write lock */
        writer_lock(&rwlock);

        /* Write to shared data */
        new_value = id * 100 + i + 1;
        shared_value = new_value;
        write_count++;

        printf(1, "Writer %d: wrote value = %d (write #%d)\n", id, new_value, i + 1);

        /* Release write lock */
        writer_unlock(&rwlock);

        /* Update stats */
        mutex_lock(&stats_mutex);
        total_writes++;
        mutex_unlock(&stats_mutex);

        /* Yield to allow interleaving */
        thread_yield();
    }

    printf(1, "Writer %d: finished (%d writes)\n", id, write_count);
    return (void*)write_count;
}

int main(int argc, char *argv[])
{
    int reader_tids[NUM_READERS];
    int writer_tids[NUM_WRITERS];
    void *ret;
    int i;
    int expected_reads = NUM_READERS * READS_PER_READER;
    int expected_writes = NUM_WRITERS * WRITES_PER_WRITER;

    printf(1, "\n");
    printf(1, "########################################################\n");
    printf(1, "#   READER-WRITER LOCK with Writer Priority (3.2)      #\n");
    printf(1, "########################################################\n");
    printf(1, "\n");

    printf(1, "Configuration:\n");
    printf(1, "  - Readers: %d (each performing %d reads)\n",
           NUM_READERS, READS_PER_READER);
    printf(1, "  - Writers: %d (each performing %d writes)\n",
           NUM_WRITERS, WRITES_PER_WRITER);
    printf(1, "  - Expected total reads: %d\n", expected_reads);
    printf(1, "  - Expected total writes: %d\n", expected_writes);
    printf(1, "\n");

    printf(1, "Writer Priority Policy:\n");
    printf(1, "  - Multiple readers can read simultaneously\n");
    printf(1, "  - Writers get exclusive access\n");
    printf(1, "  - When a writer is waiting, new readers must wait\n");
    printf(1, "\n");

    /* Initialize threading system */
    thread_init();

    /* Initialize rwlock */
    rwlock_init(&rwlock);

    /* Initialize stats mutex */
    mutex_init(&stats_mutex);

    printf(1, "--- Starting threads ---\n\n");

    /* Create reader threads first */
    for (i = 0; i < NUM_READERS; i++) {
        reader_tids[i] = thread_create(reader_thread, (void*)(i + 1));
    }

    /* Give readers a chance to start */
    thread_yield();

    /* Create writer threads */
    for (i = 0; i < NUM_WRITERS; i++) {
        writer_tids[i] = thread_create(writer_thread, (void*)(i + 1));
    }

    /* Wait for all readers to finish */
    printf(1, "\n--- Waiting for readers ---\n");
    for (i = 0; i < NUM_READERS; i++) {
        ret = thread_join(reader_tids[i]);
        printf(1, "Reader %d joined (%d reads)\n", i + 1, (int)ret);
    }

    /* Wait for all writers to finish */
    printf(1, "\n--- Waiting for writers ---\n");
    for (i = 0; i < NUM_WRITERS; i++) {
        ret = thread_join(writer_tids[i]);
        printf(1, "Writer %d joined (%d writes)\n", i + 1, (int)ret);
    }

    printf(1, "\n");
    printf(1, "########################################################\n");
    printf(1, "RESULTS:\n");
    printf(1, "  Total reads:  %d (expected: %d)\n", total_reads, expected_reads);
    printf(1, "  Total writes: %d (expected: %d)\n", total_writes, expected_writes);
    printf(1, "  Final shared value: %d\n", shared_value);
    printf(1, "\n");

    if (total_reads == expected_reads && total_writes == expected_writes) {
        printf(1, "  STATUS: PASS - All operations completed!\n");
    } else {
        printf(1, "  STATUS: FAIL - Operation count mismatch!\n");
    }

    printf(1, "\n");
    printf(1, "WRITER PRIORITY VERIFICATION:\n");
    printf(1, "  The output above should show that when writers are\n");
    printf(1, "  waiting, readers do not start new read operations.\n");
    printf(1, "  Writers should not be starved.\n");
    printf(1, "########################################################\n\n");

    exit();
}
