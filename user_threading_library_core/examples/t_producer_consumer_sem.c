/*
 * t_producer_consumer_sem.c - Producer-Consumer Problem using Semaphores
 *
 * Classic synchronization problem solved using semaphores.
 *
 * Requirements from project:
 * - 3 producer threads, each generating 10 "items" (integers)
 * - 2 consumer threads that process items
 * - Bounded buffer of size 5
 * - Use semaphores to track empty slots and available items
 * - Use a mutex to protect buffer access
 * - Producers stop after producing their 10 items
 * - Consumers stop once all 30 items have been processed
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define BUFFER_SIZE 5
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

// Bounded buffer
int buffer[BUFFER_SIZE];
int buffer_head = 0;  // Read position
int buffer_tail = 0;  // Write position

// Synchronization primitives
sem_t empty_slots;    // Counts empty slots in buffer
sem_t filled_slots;   // Counts filled slots in buffer
mutex_t buffer_lock;  // Protects buffer access

// Coordination
int items_produced = 0;   // Total items produced
int items_consumed = 0;   // Total items consumed
mutex_t coord_lock;       // Protects coordination variables

/*
 * Producer thread function
 */
void *producer(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < ITEMS_PER_PRODUCER; i++) {
        // Create an item (unique identifier based on producer ID and iteration)
        int item = id * 100 + i;

        // Wait for an empty slot
        sem_wait(&empty_slots);

        // Lock the buffer
        mutex_lock(&buffer_lock);

        // Add item to buffer
        buffer[buffer_tail] = item;
        buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;

        printf(1, "Producer %d: produced item %d\n", id, item);

        // Unlock the buffer
        mutex_unlock(&buffer_lock);

        // Signal that a slot is now filled
        sem_post(&filled_slots);

        // Yield to allow other threads to run
        thread_yield();
    }

    printf(1, "Producer %d: finished producing %d items\n", id, ITEMS_PER_PRODUCER);
    return 0;
}

/*
 * Consumer thread function
 */
void *consumer(void *arg) {
    int id = (int)(uint)arg;
    int local_consumed = 0;

    while (1) {
        // Check if all items have been consumed
        mutex_lock(&coord_lock);
        if (items_consumed >= TOTAL_ITEMS) {
            mutex_unlock(&coord_lock);
            break;
        }
        items_consumed++;
        int my_item_num = items_consumed;
        mutex_unlock(&coord_lock);

        // Wait for a filled slot
        sem_wait(&filled_slots);

        // Lock the buffer
        mutex_lock(&buffer_lock);

        // Remove item from buffer
        int item = buffer[buffer_head];
        buffer_head = (buffer_head + 1) % BUFFER_SIZE;

        printf(1, "Consumer %d: consumed item %d (total: %d/%d)\n",
               id, item, my_item_num, TOTAL_ITEMS);

        // Unlock the buffer
        mutex_unlock(&buffer_lock);

        // Signal that a slot is now empty
        sem_post(&empty_slots);

        local_consumed++;

        // Yield to allow other threads to run
        thread_yield();
    }

    printf(1, "Consumer %d: finished consuming %d items\n", id, local_consumed);
    return (void*)(uint)local_consumed;
}

int main(void) {
    int producer_tids[NUM_PRODUCERS];
    int consumer_tids[NUM_CONSUMERS];
    int i;

    printf(1, "=== Producer-Consumer Problem (Semaphores) ===\n\n");
    printf(1, "Configuration:\n");
    printf(1, "  Producers: %d (each produces %d items)\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf(1, "  Consumers: %d\n", NUM_CONSUMERS);
    printf(1, "  Buffer size: %d\n", BUFFER_SIZE);
    printf(1, "  Total items to process: %d\n\n", TOTAL_ITEMS);

    // Initialize threading system
    thread_init();

    // Initialize semaphores
    // empty_slots starts at BUFFER_SIZE (all slots empty)
    // filled_slots starts at 0 (no items in buffer)
    sem_init(&empty_slots, BUFFER_SIZE);
    sem_init(&filled_slots, 0);

    // Initialize mutexes
    mutex_init(&buffer_lock);
    mutex_init(&coord_lock);

    printf(1, "Starting production and consumption...\n\n");

    // Create producer threads
    for (i = 0; i < NUM_PRODUCERS; i++) {
        producer_tids[i] = thread_create(producer, (void*)(uint)(i + 1));
        if (producer_tids[i] < 0) {
            printf(1, "ERROR: Failed to create producer %d\n", i);
            exit();
        }
    }

    // Create consumer threads
    for (i = 0; i < NUM_CONSUMERS; i++) {
        consumer_tids[i] = thread_create(consumer, (void*)(uint)(i + 1));
        if (consumer_tids[i] < 0) {
            printf(1, "ERROR: Failed to create consumer %d\n", i);
            exit();
        }
    }

    // Wait for all producer threads to complete
    printf(1, "\nWaiting for producers to finish...\n");
    for (i = 0; i < NUM_PRODUCERS; i++) {
        thread_join(producer_tids[i]);
    }
    printf(1, "All producers finished!\n");

    // Wait for all consumer threads to complete
    printf(1, "\nWaiting for consumers to finish...\n");
    int total_by_consumers = 0;
    for (i = 0; i < NUM_CONSUMERS; i++) {
        void *ret = thread_join(consumer_tids[i]);
        int consumed = (int)(uint)ret;
        printf(1, "Consumer %d processed %d items\n", i + 1, consumed);
        total_by_consumers += consumed;
    }

    // Verify results
    printf(1, "\n=== Results ===\n");
    printf(1, "Total items produced: %d\n", TOTAL_ITEMS);
    printf(1, "Total items consumed: %d\n", total_by_consumers);

    if (total_by_consumers == TOTAL_ITEMS) {
        printf(1, "Status: PASS - All items processed correctly!\n");
    } else {
        printf(1, "Status: FAIL - Item count mismatch!\n");
    }

    printf(1, "\n=== Producer-Consumer Test Complete ===\n");

    exit();
}
