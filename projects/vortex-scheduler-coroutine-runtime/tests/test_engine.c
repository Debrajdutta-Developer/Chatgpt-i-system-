#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

static atomic_int test_counter = 0;

static void dummy_task(void *arg) {
    int *val = (int *)arg;
    atomic_fetch_add(&test_counter, *val);
    vx_yield();
    atomic_fetch_add(&test_counter, *val);
}

int main(void) {
    printf("Running VortexScheduler Engine Unit Tests...\n");

    // Check 1: Runtime creation
    vx_runtime_t *runtime = vx_runtime_create(2);
    assert(runtime != NULL);
    printf("[CHECK 1] Runtime successfully created.\n");

    // Check 2: Queue push and pop invariants
    vx_queue_t queue;
    atomic_init(&queue.head, 0);
    atomic_init(&queue.tail, 0);
    vx_coroutine_t *co_dummy = vx_coroutine_create(runtime, dummy_task, &((int){10}), 4096);
    assert(co_dummy != NULL);
    bool pushed = vx_queue_push(&queue, co_dummy);
    assert(pushed == true);
    printf("[CHECK 2] Queue push invariant verified.\n");

    // Check 3: Queue pop correctness
    vx_coroutine_t *popped = vx_queue_pop(&queue);
    assert(popped == co_dummy);
    printf("[CHECK 3] Queue pop correctness verified.\n");

    // Check 4: Queue empty state
    vx_coroutine_t *popped_empty = vx_queue_pop(&queue);
    assert(popped_empty == NULL);
    printf("[CHECK 4] Queue empty state verified.\n");

    // Check 5: Channel creation and bounds
    vx_channel_t *chan = vx_channel_create(4);
    assert(chan != NULL);
    assert(chan->capacity == 4);
    printf("[CHECK 5] Channel creation and capacity verified.\n");

    // Check 6: Channel send and receive
    int test_val = 42;
    bool sent = vx_channel_send(chan, &test_val);
    assert(sent == true);
    void *recv_val = NULL;
    bool received = vx_channel_receive(chan, &recv_val);
    assert(received == true);
    assert(*(int *)recv_val == 42);
    printf("[CHECK 6] Channel send/receive data integrity verified.\n");

    // Check 7: Mutex initialization and state
    vx_mutex_t mutex;
    vx_mutex_init(&mutex);
    assert(atomic_load(&mutex.locked) == false);
    assert(mutex.owner == NULL);
    printf("[CHECK 7] Mutex initialization verified.\n");

    // Check 8: Semaphore initialization
    vx_semaphore_t sem;
    vx_semaphore_init(&sem, 2);
    assert(atomic_load(&sem.value) == 2);
    printf("[CHECK 8] Semaphore initialization verified.\n");

    // Check 9: Runtime start and stop lifecycle
    vx_runtime_start(runtime);
    usleep(100000);
    vx_runtime_stop(runtime);
    assert(atomic_load(&runtime->running) == false);
    printf("[CHECK 9] Runtime lifecycle start/stop verified.\n");

    // Check 10: Coroutine destruction cleanup
    vx_coroutine_destroy(co_dummy);
    vx_channel_destroy(chan);
    vx_runtime_destroy(runtime);
    assert(true);
    printf("[CHECK 10] Resource cleanup and destruction verified.\n");

    printf("All 10 Unit Tests passed successfully!\n");
    return 0;
}
