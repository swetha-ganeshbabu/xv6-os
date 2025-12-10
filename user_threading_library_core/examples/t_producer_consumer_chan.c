/*
 * t_producer_consumer_chan.c - Producer-Consumer Problem using Channels
 *
 * Same Producer-Consumer problem solved using channels instead of semaphores.
 * Channels internally handle all the locking and waiting.
 *
 * Requirements from project:
 * - 3 producer threads, each generating 10 "items" (integers)
 * - 2 consumer threads that process items
 * - Use a single channel_t for all communication
 * - Producers stop after sending their 10 items
 * - Consumers stop once all items (30 total) have been processed and channel is closed
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define CHANNEL_CAPACITY 5
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

// Shared channel for communication
channel_t *item_channel;

// Coordination for knowing when producers are done
int producers_done = 0;
mutex_t done_lock;

// Tracking consumed items
int items_consumed = 0;
mutex_t consumed_lock;

/*
 * Producer thread function
 */
void *producer(void *arg) {
    int id = (int)(uint)arg;
    int i;

    for (i = 0; i < ITEMS_PER_PRODUCER; i++) {
        // Create an item (unique identifier based on producer ID and iteration)
        // We need to allocate memory for the item since channels pass pointers
        int *item = (int*)malloc(sizeof(int));
        if (item == 0) {
            printf(1, "Producer %d: malloc failed!\n", id);
            break;
        }
        *item = id * 100 + i;

        // Send item through channel
        int result = channel_send(item_channel, (void*)item);
        if (result < 0) {
            printf(1, "Producer %d: channel closed, stopping\n", id);
            free(item);
            break;
        }

        printf(1, "Producer %d: produced item %d\n", id, *item);

        // Yield to allow other threads to run
        thread_yield();
    }

    // Mark this producer as done
    mutex_lock(&done_lock);
    producers_done++;
    printf(1, "Producer %d: finished (%d/%d producers done)\n",
           id, producers_done, NUM_PRODUCERS);

    // If all producers are done, close the channel
    if (producers_done == NUM_PRODUCERS) {
        printf(1, "\nAll producers done! Closing channel...\n\n");
        channel_close(item_channel);
    }
    mutex_unlock(&done_lock);

    return 0;
}

/*
 * Consumer thread function
 */
void *consumer(void *arg) {
    int id = (int)(uint)arg;
    int local_consumed = 0;

    while (1) {
        void *data;
        int result = channel_recv(item_channel, &data);

        if (result < 0) {
            // Channel is closed and empty
            printf(1, "Consumer %d: channel closed, stopping\n", id);
            break;
        }

        int *item = (int*)data;
        int item_value = *item;

        // Track total consumed
        mutex_lock(&consumed_lock);
        items_consumed++;
        int total = items_consumed;
        mutex_unlock(&consumed_lock);

        printf(1, "Consumer %d: consumed item %d (total: %d/%d)\n",
               id, item_value, total, TOTAL_ITEMS);

        // Free the allocated item
        free(item);

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

    printf(1, "=== Producer-Consumer Problem (Channels) ===\n\n");
    printf(1, "Configuration:\n");
    printf(1, "  Producers: %d (each produces %d items)\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf(1, "  Consumers: %d\n", NUM_CONSUMERS);
    printf(1, "  Channel capacity: %d\n", CHANNEL_CAPACITY);
    printf(1, "  Total items to process: %d\n\n", TOTAL_ITEMS);

    // Initialize threading system
    thread_init();

    // Create channel
    item_channel = channel_create(CHANNEL_CAPACITY);
    if (item_channel == 0) {
        printf(1, "ERROR: Failed to create channel\n");
        exit();
    }

    // Initialize coordination mutexes
    mutex_init(&done_lock);
    mutex_init(&consumed_lock);

    printf(1, "Starting production and consumption...\n\n");

    // Create consumer threads first (so they're ready to receive)
    for (i = 0; i < NUM_CONSUMERS; i++) {
        consumer_tids[i] = thread_create(consumer, (void*)(uint)(i + 1));
        if (consumer_tids[i] < 0) {
            printf(1, "ERROR: Failed to create consumer %d\n", i);
            exit();
        }
    }

    // Create producer threads
    for (i = 0; i < NUM_PRODUCERS; i++) {
        producer_tids[i] = thread_create(producer, (void*)(uint)(i + 1));
        if (producer_tids[i] < 0) {
            printf(1, "ERROR: Failed to create producer %d\n", i);
            exit();
        }
    }

    // Wait for all producer threads to complete
    printf(1, "\nWaiting for producers to finish...\n");
    for (i = 0; i < NUM_PRODUCERS; i++) {
        thread_join(producer_tids[i]);
    }

    // Wait for all consumer threads to complete
    printf(1, "\nWaiting for consumers to finish...\n");
    int total_by_consumers = 0;
    for (i = 0; i < NUM_CONSUMERS; i++) {
        void *ret = thread_join(consumer_tids[i]);
        int consumed = (int)(uint)ret;
        printf(1, "Consumer %d processed %d items\n", i + 1, consumed);
        total_by_consumers += consumed;
    }

    // Clean up channel
    channel_destroy(item_channel);

    // Verify results
    printf(1, "\n=== Results ===\n");
    printf(1, "Total items produced: %d\n", TOTAL_ITEMS);
    printf(1, "Total items consumed: %d\n", total_by_consumers);

    if (total_by_consumers == TOTAL_ITEMS) {
        printf(1, "Status: PASS - All items processed correctly!\n");
    } else {
        printf(1, "Status: FAIL - Item count mismatch!\n");
    }

    printf(1, "\n=== Producer-Consumer (Channels) Test Complete ===\n");

    exit();
}
