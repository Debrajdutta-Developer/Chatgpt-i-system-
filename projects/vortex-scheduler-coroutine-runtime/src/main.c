#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static atomic_int counter = 0;

static void sample_task(void *arg) {
    int id = *(int *)arg;
    printf("[VortexScheduler] Coroutine %d started\n", id);
    for (int i = 0; i < 3; i++) {
        atomic_fetch_add(&counter, 1);
        vx_yield();
    }
    printf("[VortexScheduler] Coroutine %d finished\n", id);
}

int main(void) {
    printf("Initializing VortexScheduler Runtime...");
    vx_runtime_t *runtime = vx_runtime_create(2);
    if (!runtime) {
        fprintf(stderr, "Failed to create runtime\n");
        return 1;
    }
    printf(" DONE\n");

    vx_runtime_start(runtime);

    int ids[5];
    for (int i = 0; i < 5; i++) {
        ids[i] = i + 1;
        vx_coroutine_spawn(runtime, sample_task, &ids[i]);
    }

    usleep(500000);

    vx_runtime_stop(runtime);
    vx_runtime_destroy(runtime);

    printf("VortexScheduler main executed successfully. Final counter value: %d\n", atomic_load(&counter));
    return 0;
}
