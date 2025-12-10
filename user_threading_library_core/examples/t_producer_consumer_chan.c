/*
 * t_producer_consumer_chan.c - Producer-Consumer with Channels (Part 3.1.2)
 *
 * Same producer-consumer problem, but using channels for communication.
 * Channels internally handle all synchronization.
 *
 * Requirements:
 * - 3 producer threads, each generating 10 items
 * - 2 consumer threads that process items
 * - Use a single channel_t for all communication
 * - Producers stop after sending their 10 items
 * - Consumers stop once all 30 items processed and channel closed
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define CHANNEL_CAPACITY 5
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

/* Shared channel */
channel_t *item_channel;

/* Tracking */
int items_produced = 0;
int items_consumed = 0;
mutex_t produced_mutex;
mutex_t consumed_mutex;

/* Item structure */
struct item {
    int producer_id;
    int item_num;
};

/*
 * Producer thread function
 */
void *producer(void *arg)
{
    int id = (int)arg;
    int i;
    struct item *it;
    int ret;

    printf(1, "Producer %d: started\n", id);

    for (i = 0; i < ITEMS_PER_PRODUCER; i++) {
        /* Allocate and populate item */
        it = malloc(sizeof(struct item));
        if (it == 0) {
            printf(1, "Producer %d: failed to allocate item!\n", id);
            break;
        }
        it->producer_id = id;
        it->item_num = i;

        /* Send item through channel (blocks if full) */
        ret = channel_send(item_channel, (void*)it);

        if (ret < 0) {
            printf(1, "Producer %d: channel closed, stopping\n", id);
            free(it);
            break;
        }

        printf(1, "Producer %d: produced item %d\n", id, i);

        /* Update produced count */
        mutex_lock(&produced_mutex);
        items_produced++;
        mutex_unlock(&produced_mutex);

        /* Yield occasionally */
        thread_yield();
    }

    printf(1, "Producer %d: finished producing %d items\n", id, ITEMS_PER_PRODUCER);
    return (void*)ITEMS_PER_PRODUCER;
}

/*
 * Consumer thread function
 */
void *consumer(void *arg)
{
    int id = (int)arg;
    struct item *it;
    int consumed_count = 0;
    int ret;

    printf(1, "Consumer %d: started\n", id);

    while (1) {
        /* Receive item from channel (blocks if empty) */
        ret = channel_recv(item_channel, (void**)&it);

        if (ret < 0) {
            /* Channel closed and empty */
            printf(1, "Consumer %d: channel closed, stopping\n", id);
            break;
        }

        printf(1, "Consumer %d: consumed item from P%d (item %d)\n",
               id, it->producer_id, it->item_num);

        /* Free the item */
        free(it);
        consumed_count++;

        /* Update consumed count */
        mutex_lock(&consumed_mutex);
        items_consumed++;
        int current = items_consumed;
        mutex_unlock(&consumed_mutex);

        /* Check if all items consumed */
        if (current >= TOTAL_ITEMS) {
            /* Close channel to signal other consumers */
            channel_close(item_channel);
            break;
        }

        /* Yield occasionally */
        thread_yield();
    }

    printf(1, "Consumer %d: finished consuming %d items\n", id, consumed_count);
    return (void*)consumed_count;
}

int main(int argc, char *argv[])
{
    int producer_tids[NUM_PRODUCERS];
    int consumer_tids[NUM_CONSUMERS];
    void *ret;
    int i;

    printf(1, "\n");
    printf(1, "########################################################\n");
    printf(1, "#   PRODUCER-CONSUMER with Channels (Part 3.1.2)       #\n");
    printf(1, "########################################################\n");
    printf(1, "\n");

    printf(1, "Configuration:\n");
    printf(1, "  - Producers: %d (each producing %d items)\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf(1, "  - Consumers: %d\n", NUM_CONSUMERS);
    printf(1, "  - Channel capacity: %d\n", CHANNEL_CAPACITY);
    printf(1, "  - Total items: %d\n", TOTAL_ITEMS);
    printf(1, "\n");

    /* Initialize threading system */
    thread_init();

    /* Create channel */
    item_channel = channel_create(CHANNEL_CAPACITY);
    if (item_channel == 0) {
        printf(1, "ERROR: Failed to create channel!\n");
        exit();
    }

    /* Initialize tracking mutexes */
    mutex_init(&produced_mutex);
    mutex_init(&consumed_mutex);

    printf(1, "--- Starting threads ---\n\n");

    /* Create producer threads */
    for (i = 0; i < NUM_PRODUCERS; i++) {
        producer_tids[i] = thread_create(producer, (void*)(i + 1));
    }

    /* Create consumer threads */
    for (i = 0; i < NUM_CONSUMERS; i++) {
        consumer_tids[i] = thread_create(consumer, (void*)(i + 1));
    }

    /* Wait for producers to finish */
    printf(1, "\n--- Waiting for producers ---\n");
    for (i = 0; i < NUM_PRODUCERS; i++) {
        ret = thread_join(producer_tids[i]);
        printf(1, "Producer %d joined (produced %d items)\n", i + 1, (int)ret);
    }

    /* Wait for consumers to finish */
    printf(1, "\n--- Waiting for consumers ---\n");
    for (i = 0; i < NUM_CONSUMERS; i++) {
        ret = thread_join(consumer_tids[i]);
        printf(1, "Consumer %d joined (consumed %d items)\n", i + 1, (int)ret);
    }

    /* Clean up channel */
    channel_destroy(item_channel);

    printf(1, "\n");
    printf(1, "########################################################\n");
    printf(1, "RESULTS:\n");
    printf(1, "  Items produced: %d\n", items_produced);
    printf(1, "  Items consumed: %d\n", items_consumed);
    printf(1, "\n");

    if (items_produced == TOTAL_ITEMS && items_consumed == TOTAL_ITEMS) {
        printf(1, "  STATUS: PASS - All items processed correctly!\n");
    } else {
        printf(1, "  STATUS: FAIL - Mismatch in item counts!\n");
    }
    printf(1, "########################################################\n\n");

    exit();
}
