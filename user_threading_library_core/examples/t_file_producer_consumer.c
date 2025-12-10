/*
 * t_file_producer_consumer.c - Producer-Consumer with Pipes (Part 4 Extra Credit)
 *
 * Demonstrates Thread-Safe File I/O using pipes between separate processes.
 *
 * Architecture:
 *   Parent Process (Producer):
 *     - Multiple producer threads write items to pipe
 *     - Each thread produces items and writes to shared pipe
 *
 *   Child Process (Consumer):
 *     - Multiple consumer threads read from pipe
 *     - Each thread reads items and processes them
 *
 * This achieves TRUE async I/O because:
 *   - When producer blocks on pipe write (pipe full), consumer can still run
 *   - When consumer blocks on pipe read (pipe empty), producer can still run
 *   - The kernel schedules processes independently
 *
 * Key insight: In N:1 user-level threading, blocking syscalls block ALL threads
 * in a process. Using separate processes with pipes allows the kernel to schedule
 * the other process when one blocks.
 */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "uthreads.h"
#include "uthreads_io.h"

#define NUM_PRODUCER_THREADS 2
#define NUM_CONSUMER_THREADS 2
#define ITEMS_PER_THREAD 5
#define TOTAL_ITEMS (NUM_PRODUCER_THREADS * ITEMS_PER_THREAD)

/*
 * Item structure sent through pipe
 */
struct item {
    int producer_id;
    int sequence;
    int value;
};

/*
 * Global state for producer process
 */
int write_fd;                    // Write end of pipe
mutex_t write_lock;              // Protects pipe writes
int items_produced = 0;          // Counter for coordination
mutex_t prod_count_lock;

/*
 * Global state for consumer process
 */
int read_fd;                     // Read end of pipe
mutex_t read_lock;               // Protects pipe reads
int items_consumed = 0;          // Counter for coordination
mutex_t cons_count_lock;

/*
 * Producer thread function
 */
void *producer_thread(void *arg) {
    int id = (int)(uint)arg;
    int i;
    struct item it;

    printf(1, "[Producer %d] Starting\n", id);

    for (i = 0; i < ITEMS_PER_THREAD; i++) {
        // Create item
        it.producer_id = id;
        it.sequence = i;
        it.value = id * 100 + i;

        // Write to pipe (thread-safe)
        mutex_lock(&write_lock);

        // Yield before I/O to be cooperative
        thread_yield();

        int n = write(write_fd, &it, sizeof(it));

        thread_yield();

        mutex_unlock(&write_lock);

        if (n != sizeof(it)) {
            printf(1, "[Producer %d] Write error!\n", id);
            break;
        }

        printf(1, "[Producer %d] Sent item: seq=%d, val=%d\n",
               id, it.sequence, it.value);

        // Update counter
        mutex_lock(&prod_count_lock);
        items_produced++;
        mutex_unlock(&prod_count_lock);

        // Yield to let other threads run
        thread_yield();
    }

    printf(1, "[Producer %d] Finished\n", id);
    return (void*)(uint)ITEMS_PER_THREAD;
}

/*
 * Consumer thread function
 */
void *consumer_thread(void *arg) {
    int id = (int)(uint)arg;
    int local_consumed = 0;
    struct item it;

    printf(1, "[Consumer %d] Starting\n", id);

    while (1) {
        // Check if we've consumed all items
        mutex_lock(&cons_count_lock);
        if (items_consumed >= TOTAL_ITEMS) {
            mutex_unlock(&cons_count_lock);
            break;
        }
        items_consumed++;
        int my_count = items_consumed;
        mutex_unlock(&cons_count_lock);

        // Read from pipe (thread-safe)
        mutex_lock(&read_lock);

        // Yield before I/O
        thread_yield();

        int n = read(read_fd, &it, sizeof(it));

        thread_yield();

        mutex_unlock(&read_lock);

        if (n != sizeof(it)) {
            printf(1, "[Consumer %d] Read error or EOF\n", id);
            break;
        }

        printf(1, "[Consumer %d] Received item %d/%d: from P%d, seq=%d, val=%d\n",
               id, my_count, TOTAL_ITEMS, it.producer_id, it.sequence, it.value);

        local_consumed++;

        // Yield to let other threads run
        thread_yield();
    }

    printf(1, "[Consumer %d] Finished, consumed %d items\n", id, local_consumed);
    return (void*)(uint)local_consumed;
}

/*
 * Run producer process
 */
void run_producer(int wfd) {
    int tids[NUM_PRODUCER_THREADS];
    int i;

    printf(1, "\n=== PRODUCER PROCESS (pid %d) ===\n", getpid());

    // Initialize threading for this process
    thread_init();

    // Set up globals
    write_fd = wfd;
    mutex_init(&write_lock);
    mutex_init(&prod_count_lock);
    items_produced = 0;

    // Create producer threads
    printf(1, "Creating %d producer threads...\n", NUM_PRODUCER_THREADS);
    for (i = 0; i < NUM_PRODUCER_THREADS; i++) {
        tids[i] = thread_create(producer_thread, (void*)(uint)(i + 1));
        if (tids[i] < 0) {
            printf(1, "Failed to create producer thread %d\n", i);
            exit();
        }
    }

    // Wait for all producers to finish
    int total = 0;
    for (i = 0; i < NUM_PRODUCER_THREADS; i++) {
        void *ret = thread_join(tids[i]);
        total += (int)(uint)ret;
    }

    printf(1, "\n=== Producer process done, produced %d items ===\n", total);

    // Close write end of pipe
    close(write_fd);

    exit();
}

/*
 * Run consumer process
 */
void run_consumer(int rfd) {
    int tids[NUM_CONSUMER_THREADS];
    int i;

    printf(1, "\n=== CONSUMER PROCESS (pid %d) ===\n", getpid());

    // Initialize threading for this process
    thread_init();

    // Set up globals
    read_fd = rfd;
    mutex_init(&read_lock);
    mutex_init(&cons_count_lock);
    items_consumed = 0;

    // Create consumer threads
    printf(1, "Creating %d consumer threads...\n", NUM_CONSUMER_THREADS);
    for (i = 0; i < NUM_CONSUMER_THREADS; i++) {
        tids[i] = thread_create(consumer_thread, (void*)(uint)(i + 1));
        if (tids[i] < 0) {
            printf(1, "Failed to create consumer thread %d\n", i);
            exit();
        }
    }

    // Wait for all consumers to finish
    int total = 0;
    for (i = 0; i < NUM_CONSUMER_THREADS; i++) {
        void *ret = thread_join(tids[i]);
        total += (int)(uint)ret;
    }

    printf(1, "\n=== Consumer process done, consumed %d items ===\n", total);

    // Close read end of pipe
    close(read_fd);

    exit();
}

int main(void) {
    int fds[2];
    int pid;

    printf(1, "===========================================\n");
    printf(1, "  Thread-Safe File I/O Demo (Part 4 EC)\n");
    printf(1, "  Producer-Consumer with Pipes\n");
    printf(1, "===========================================\n\n");

    printf(1, "Configuration:\n");
    printf(1, "  Producer threads: %d\n", NUM_PRODUCER_THREADS);
    printf(1, "  Consumer threads: %d\n", NUM_CONSUMER_THREADS);
    printf(1, "  Items per producer: %d\n", ITEMS_PER_THREAD);
    printf(1, "  Total items: %d\n\n", TOTAL_ITEMS);

    // Create pipe
    if (pipe(fds) < 0) {
        printf(1, "Error: Failed to create pipe\n");
        exit();
    }

    printf(1, "Pipe created: read_fd=%d, write_fd=%d\n", fds[0], fds[1]);

    // Fork into producer and consumer processes
    pid = fork();

    if (pid < 0) {
        printf(1, "Error: Fork failed\n");
        close(fds[0]);
        close(fds[1]);
        exit();
    }

    if (pid == 0) {
        // Child process = Consumer
        close(fds[1]);  // Close write end
        run_consumer(fds[0]);
        // run_consumer calls exit()
    } else {
        // Parent process = Producer
        close(fds[0]);  // Close read end
        run_producer(fds[1]);
        // run_producer calls exit(), but we also wait for child
    }

    // Should not reach here
    exit();
}
