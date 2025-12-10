/*
 * t_producer_consumer_sem.c - Producer-Consumer with Semaphores (Part 3.1.1)
 *
 * Classic producer-consumer problem solved using semaphores.
 *
 * Requirements:
 * - 3 producer threads, each generating 10 items
 * - 2 consumer threads that process items
 * - Bounded buffer of size 5
 * - Use semaphores to track empty slots and available items
 * - Use a mutex to protect buffer access
 */

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define BUFFER_SIZE 5
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

/* Bounded buffer */
int buffer[BUFFER_SIZE];
int buffer_in = 0;   /* Index for next insert */
int buffer_out = 0;  /* Index for next remove */

/* Synchronization primitives */
sem_t empty_slots;    /* Counts empty slots in buffer */
sem_t full_slots;     /* Counts items in buffer */
mutex_t buffer_mutex; /* Protects buffer access */

/* Tracking */
int items_produced = 0;
int items_consumed = 0;
mutex_t produced_mutex;
mutex_t consumed_mutex;

/* Done flag and its mutex */
int production_done = 0;
mutex_t done_mutex;

/*
 * Producer thread function
 */
void *producer(void *arg)
{
    int id = (int)arg;
    int i;
    int item;

    printf(1, "Producer %d: started\n", id);

    for (i = 0; i < ITEMS_PER_PRODUCER; i++) {
        /* Create item: unique identifier = producer_id * 100 + item_number */
        item = id * 100 + i;

        /* Wait for an empty slot */
        sem_wait(&empty_slots);

        /* Acquire buffer lock */
        mutex_lock(&buffer_mutex);

        /* Add item to buffer */
        buffer[buffer_in] = item;
        buffer_in = (buffer_in + 1) % BUFFER_SIZE;

        printf(1, "Producer %d: produced item %d\n", id, item);

        mutex_unlock(&buffer_mutex);

        /* Signal that there's a new item */
        sem_post(&full_slots);

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
    int item;
    int consumed_count = 0;

    printf(1, "Consumer %d: started\n", id);

    while (1) {
        /* Check if we're done */
        mutex_lock(&consumed_mutex);
        if (items_consumed >= TOTAL_ITEMS) {
            mutex_unlock(&consumed_mutex);
            break;
        }
        mutex_unlock(&consumed_mutex);

        /* Wait for an item */
        sem_wait(&full_slots);

        /* Double-check after waking up */
        mutex_lock(&consumed_mutex);
        if (items_consumed >= TOTAL_ITEMS) {
            mutex_unlock(&consumed_mutex);
            /* Put back the slot we took */
            sem_post(&full_slots);
            break;
        }

        /* Acquire buffer lock */
        mutex_lock(&buffer_mutex);

        /* Remove item from buffer */
        item = buffer[buffer_out];
        buffer_out = (buffer_out + 1) % BUFFER_SIZE;

        mutex_unlock(&buffer_mutex);

        /* Update consumed count */
        items_consumed++;
        mutex_unlock(&consumed_mutex);

        /* Signal that there's an empty slot */
        sem_post(&empty_slots);

        printf(1, "Consumer %d: consumed item %d\n", id, item);
        consumed_count++;

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
    int total_consumed = 0;

    printf(1, "\n");
    printf(1, "########################################################\n");
    printf(1, "#   PRODUCER-CONSUMER with Semaphores (Part 3.1.1)     #\n");
    printf(1, "########################################################\n");
    printf(1, "\n");

    printf(1, "Configuration:\n");
    printf(1, "  - Producers: %d (each producing %d items)\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf(1, "  - Consumers: %d\n", NUM_CONSUMERS);
    printf(1, "  - Buffer size: %d\n", BUFFER_SIZE);
    printf(1, "  - Total items: %d\n", TOTAL_ITEMS);
    printf(1, "\n");

    /* Initialize threading system */
    thread_init();

    /* Initialize semaphores */
    sem_init(&empty_slots, BUFFER_SIZE);  /* All slots initially empty */
    sem_init(&full_slots, 0);             /* No items initially */

    /* Initialize mutexes */
    mutex_init(&buffer_mutex);
    mutex_init(&produced_mutex);
    mutex_init(&consumed_mutex);
    mutex_init(&done_mutex);

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

    /* Signal end of production */
    mutex_lock(&done_mutex);
    production_done = 1;
    mutex_unlock(&done_mutex);

    /* Wait for consumers to finish */
    printf(1, "\n--- Waiting for consumers ---\n");
    for (i = 0; i < NUM_CONSUMERS; i++) {
        ret = thread_join(consumer_tids[i]);
        printf(1, "Consumer %d joined (consumed %d items)\n", i + 1, (int)ret);
        total_consumed += (int)ret;
    }

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
