#ifndef VORTEX_ENGINE_H
#define VORTEX_ENGINE_H

#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <ucontext.h>
#include <sys/types.h>

#define VX_DEFAULT_STACK_SIZE (64 * 1024)
#define VX_MAX_WORKERS 64
#define VX_QUEUE_CAPACITY 1024

typedef enum {
    VX_STATE_READY,
    VX_STATE_RUNNING,
    VX_STATE_BLOCKED,
    VX_STATE_TERMINATED,
    VX_STATE_SUSPENDED
} vx_state_t;

struct vx_runtime;
struct vx_coroutine;
struct vx_worker;

typedef void (*vx_coroutine_fn)(void *arg);

typedef struct vx_coroutine {
    uint64_t id;
    vx_state_t state;
    ucontext_t context;
    void *stack_mem;
    size_t stack_size;
    vx_coroutine_fn fn;
    void *arg;
    struct vx_worker *worker;
    int exit_code;
    struct vx_coroutine *next;
} vx_coroutine_t;

typedef struct {
    vx_coroutine_t *buffer[VX_QUEUE_CAPACITY];
    atomic_size_t head;
    atomic_size_t tail;
} vx_queue_t;

typedef struct vx_worker {
    size_t id;
    struct vx_runtime *runtime;
    pthread_t thread_id;
    vx_queue_t local_queue;
    vx_coroutine_t *current_coroutine;
    ucontext_t worker_context;
    bool running;
    unsigned int seed;
} vx_worker_t;

typedef struct {
    atomic_bool locked;
    vx_coroutine_t *owner;
    vx_queue_t wait_queue;
} vx_mutex_t;

typedef struct {
    atomic_int value;
    vx_queue_t wait_queue;
} vx_semaphore_t;

typedef struct {
    void **buffer;
    size_t capacity;
    size_t size;
    size_t head;
    size_t tail;
    vx_mutex_t mutex;
    vx_semaphore_t not_full;
    vx_semaphore_t not_empty;
} vx_channel_t;

typedef struct vx_runtime {
    size_t num_workers;
    vx_worker_t workers[VX_MAX_WORKERS];
    atomic_bool running;
    atomic_uint_fast64_t next_coroutine_id;
    vx_queue_t global_submission_queue;
} vx_runtime_t;

vx_runtime_t *vx_runtime_create(size_t num_workers);
void vx_runtime_destroy(vx_runtime_t *runtime);
void vx_runtime_start(vx_runtime_t *runtime);
void vx_runtime_stop(vx_runtime_t *runtime);

vx_coroutine_t *vx_coroutine_create(vx_runtime_t *runtime, vx_coroutine_fn fn, void *arg, size_t stack_size);
void vx_coroutine_destroy(vx_coroutine_t *co);
void vx_coroutine_spawn(vx_runtime_t *runtime, vx_coroutine_fn fn, void *arg);
void vx_yield(void);

void vx_mutex_init(vx_mutex_t *mutex);
void vx_mutex_lock(vx_mutex_t *mutex);
void vx_mutex_unlock(vx_mutex_t *mutex);

void vx_semaphore_init(vx_semaphore_t *sem, int initial_value);
void vx_semaphore_wait(vx_semaphore_t *sem);
void vx_semaphore_signal(vx_semaphore_t *sem);

vx_channel_t *vx_channel_create(size_t capacity);
void vx_channel_destroy(vx_channel_t *chan);
bool vx_channel_send(vx_channel_t *chan, void *item);
bool vx_channel_receive(vx_channel_t *chan, void **item);

bool vx_queue_push(vx_queue_t *q, vx_coroutine_t *co);
vx_coroutine_t *vx_queue_pop(vx_queue_t *q);
vx_coroutine_t *vx_queue_steal(vx_queue_t *q);

#endif
