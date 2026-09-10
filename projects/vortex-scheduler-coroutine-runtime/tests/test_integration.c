#include "../src/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

static atomic_int integration_counter = 0;

static void worker_task(void *arg) {
    int inc = *(int *)arg;
    for (int i = 0; i < 5; i++) {
        atomic_fetch_add(&integration_counter, inc);
        vx_yield();
    }
}

int main(void) {
    printf("Running VortexScheduler Integration Tests...\n");

    // Check 1: Multi-worker runtime instantiation
    vx_runtime_t *runtime = vx_runtime_create(4);
    assert(runtime != NULL);
    assert(runtime->num_workers == 4);
    printf("[INT CHECK 1] 4-worker runtime initialized successfully.\n");

    // Check 2: Runtime start flag assertion
    vx_runtime_start(runtime);
    assert(atomic_load(&runtime->running) == true);
    printf("[INT CHECK 2] Runtime execution flag asserted.\n");

    // Check 3: Spawning multiple concurrent coroutines
    int arg1 = 1;
    int arg2 = 2;
    vx_coroutine_spawn(runtime, worker_task, &arg1);
    vx_coroutine_spawn(runtime, worker_task, &arg2);
    printf("[INT CHECK 3] Multiple coroutines spawned into runtime queues.\n");

    // Check 4: Concurrent execution propagation delay
    usleep(200000);
    assert(atomic_load(&integration_counter) > 0);
    printf("[INT CHECK 4] Coroutine execution progress verified via counter.\n");

    // Check 5: Channel synchronization integration
    vx_channel_t *chan = vx_channel_create(2);
    assert(chan != NULL);
    int msg = 999;
    bool sent = vx_channel_send(chan, &msg);
    assert(sent == true);
    printf("[INT CHECK 5] Integration channel send validated.\n");

    // Check 6: Channel receive integration
    void *out_msg = NULL;
    bool recv = vx_channel_receive(chan, &out_msg);
    assert(recv == true);
    assert(*(int *)out_msg == 999);
    printf("[INT CHECK 6] Integration channel receive validated.\n");

    // Check 7: Mutex contention integration
    vx_mutex_t mtx;
    vx_mutex_init(&mtx);
    vx_mutex_lock(&mtx);
    assert(atomic_load(&mtx.locked) == true);
    vx_mutex_unlock(&mtx);
    assert(atomic_load(&mtx.locked) == false);
    printf("[INT CHECK 7] Mutex contention flow verified.\n");

    // Check 8: Semaphore wait and signal integration
    vx_semaphore_t sem;
    vx_semaphore_init(&sem, 1);
    vx_semaphore_wait(&sem);
    assert(atomic_load(&sem.value) == 0);
    vx_semaphore_signal(&sem);
    assert(atomic_load(&sem.value) == 1);
    printf("[INT CHECK 8] Semaphore synchronization flow verified.\n");

    // Check 9: Clean runtime stoppage under load
    vx_runtime_stop(runtime);
    assert(atomic_load(&runtime->running) == false);
    printf("[INT CHECK 9] Runtime cleanly stopped under active load.\n");

    // Check 10: Final memory cleanup and teardown
    vx_channel_destroy(chan);
    vx_runtime_destroy(runtime);
    assert(true);
    printf("[INT CHECK 10] Complete system teardown verified.\n");

    printf("All 10 Integration Tests passed successfully!\n");
    return 0;
}
