#include "engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

static __thread vx_worker_t *current_worker_ptr = NULL;

vx_worker_t *vx_current_worker(void) {
    return current_worker_ptr;
}

bool vx_queue_push(vx_queue_t *q, vx_coroutine_t *co) {
    size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    if ((tail + 1) % VX_QUEUE_CAPACITY == head) {
        return false;
    }
    q->buffer[tail] = co;
    atomic_store_explicit(&q->tail, (tail + 1) % VX_QUEUE_CAPACITY, memory_order_release);
    return true;
}

vx_coroutine_t *vx_queue_pop(vx_queue_t *q) {
    size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (head == tail) {
        return NULL;
    }
    vx_coroutine_t *co = q->buffer[head];
    if (!atomic_compare_exchange_strong_explicit(&q->head, &head, (head + 1) % VX_QUEUE_CAPACITY,
                                               memory_order_release, memory_order_relaxed)) {
        return NULL;
    }
    return co;
}

vx_coroutine_t *vx_queue_steal(vx_queue_t *q) {
    return vx_queue_pop(q);
}

static void vx_coroutine_trampoline(void) {
    vx_worker_t *w = vx_current_worker();
    if (!w || !w->current_coroutine) return;
    vx_coroutine_t *co = w->current_coroutine;
    co->fn(co->arg);
    co->state = VX_STATE_TERMINATED;
    setcontext(&w->worker_context);
}

vx_coroutine_t *vx_coroutine_create(vx_runtime_t *runtime, vx_coroutine_fn fn, void *arg, size_t stack_size) {
    vx_coroutine_t *co = malloc(sizeof(vx_coroutine_t));
    if (!co) return NULL;
    co->id = atomic_fetch_add(&runtime->next_coroutine_id, 1);
    co->state = VX_STATE_READY;
    co->fn = fn;
    co->arg = arg;
    co->worker = NULL;
    co->exit_code = 0;
    co->next = NULL;
    co->stack_size = stack_size > 0 ? stack_size : VX_DEFAULT_STACK_SIZE;
    
    long page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_size = co->stack_size + page_size;
    void *mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        free(co);
        return NULL;
    }
    if (mprotect(mem, page_size, PROT_NONE) != 0) {
        munmap(mem, alloc_size);
        free(co);
        return NULL;
    }
    co->stack_mem = mem;
    
    getcontext(&co->context);
    co->context.uc_stack.ss_sp = (char *)mem + page_size;
    co->context.uc_stack.ss_size = co->stack_size;
    co->context.uc_link = NULL;
    makecontext(&co->context, vx_coroutine_trampoline, 0);
    return co;
}

void vx_coroutine_destroy(vx_coroutine_t *co) {
    if (!co) return;
    long page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_size = co->stack_size + page_size;
    munmap(co->stack_mem, alloc_size);
    free(co);
}

void vx_coroutine_spawn(vx_runtime_t *runtime, vx_coroutine_fn fn, void *arg) {
    vx_coroutine_t *co = vx_coroutine_create(runtime, fn, arg, VX_DEFAULT_STACK_SIZE);
    if (!co) return;
    vx_worker_t *w = vx_current_worker();
    if (w) {
        if (!vx_queue_push(&w->local_queue, co)) {
            vx_queue_push(&runtime->global_submission_queue, co);
        }
    } else {
        vx_queue_push(&runtime->global_submission_queue, co);
    }
}

void vx_yield(void) {
    vx_worker_t *w = vx_current_worker();
    if (!w || !w->current_coroutine) return;
    vx_coroutine_t *co = w->current_coroutine;
    co->state = VX_STATE_READY;
    if (!vx_queue_push(&w->local_queue, co)) {
        vx_queue_push(&w->runtime->global_submission_queue, co);
    }
    w->current_coroutine = NULL;
    swapcontext(&co->context, &w->worker_context);
}

static void *vx_worker_routine(void *arg) {
    vx_worker_t *w = (vx_worker_t *)arg;
    current_worker_ptr = w;
    
    while (atomic_load(&w->runtime->running)) {
        vx_coroutine_t *co = vx_queue_pop(&w->local_queue);
        if (!co) {
            co = vx_queue_pop(&w->runtime->global_submission_queue);
        }
        if (!co) {
            for (size_t i = 0; i < w->runtime->num_workers; i++) {
                if (i == w->id) continue;
                co = vx_queue_steal(&w->runtime->workers[i].local_queue);
                if (co) break;
            }
        }
        if (co) {
            w->current_coroutine = co;
            co->state = VX_STATE_RUNNING;
            co->worker = w;
            if (swapcontext(&w->worker_context, &co->context) == -1) {
                perror("swapcontext failed");
            }
            if (co->state == VX_STATE_TERMINATED) {
                vx_coroutine_destroy(co);
                w->current_coroutine = NULL;
            }
        } else {
            usleep(1000);
        }
    }
    return NULL;
}

vx_runtime_t *vx_runtime_create(size_t num_workers) {
    vx_runtime_t *runtime = malloc(sizeof(vx_runtime_t));
    if (!runtime) return NULL;
    runtime->num_workers = num_workers > VX_MAX_WORKERS ? VX_MAX_WORKERS : num_workers;
    atomic_init(&runtime->running, false);
    atomic_init(&runtime->next_coroutine_id, 1);
    atomic_init(&runtime->global_submission_queue.head, 0);
    atomic_init(&runtime->global_submission_queue.tail, 0);
    
    for (size_t i = 0; i < runtime->num_workers; i++) {
        runtime->workers[i].id = i;
        runtime->workers[i].runtime = runtime;
        runtime->workers[i].current_coroutine = NULL;
        runtime->workers[i].running = false;
        runtime->workers[i].seed = (unsigned int)(i + 1);
        atomic_init(&runtime->workers[i].local_queue.head, 0);
        atomic_init(&runtime->workers[i].local_queue.tail, 0);
    }
    return runtime;
}

void vx_runtime_destroy(vx_runtime_t *runtime) {
    if (!runtime) return;
    vx_runtime_stop(runtime);
    free(runtime);
}

void vx_runtime_start(vx_runtime_t *runtime) {
    atomic_store(&runtime->running, true);
    for (size_t i = 0; i < runtime->num_workers; i++) {
        runtime->workers[i].running = true;
        pthread_create(&runtime->workers[i].thread_id, NULL, vx_worker_routine, &runtime->workers[i]);
    }
}

void vx_runtime_stop(vx_runtime_t *runtime) {
    if (!atomic_load(&runtime->running)) return;
    atomic_store(&runtime->running, false);
    for (size_t i = 0; i < runtime->num_workers; i++) {
        if (runtime->workers[i].running) {
            pthread_join(runtime->workers[i].thread_id, NULL);
            runtime->workers[i].running = false;
        }
    }
}

void vx_mutex_init(vx_mutex_t *mutex) {
    atomic_init(&mutex->locked, false);
    mutex->owner = NULL;
    atomic_init(&mutex->wait_queue.head, 0);
    atomic_init(&mutex->wait_queue.tail, 0);
}

void vx_mutex_lock(vx_mutex_t *mutex) {
    vx_worker_t *w = vx_current_worker();
    bool expected = false;
    while (!atomic_compare_exchange_weak(&mutex->locked, &expected, true)) {
        expected = false;
        if (w && w->current_coroutine) {
            w->current_coroutine->state = VX_STATE_BLOCKED;
            vx_queue_push(&mutex->wait_queue, w->current_coroutine);
            vx_coroutine_t *co = w->current_coroutine;
            w->current_coroutine = NULL;
            swapcontext(&co->context, &w->worker_context);
        } else {
            usleep(1000);
        }
    }
    mutex->owner = w ? w->current_coroutine : NULL;
}

void vx_mutex_unlock(vx_mutex_t *mutex) {
    mutex->owner = NULL;
    atomic_store(&mutex->locked, false);
    vx_coroutine_t *co = vx_queue_pop(&mutex->wait_queue);
    if (co) {
        co->state = VX_STATE_READY;
        vx_worker_t *w = vx_current_worker();
        if (w) {
            vx_queue_push(&w->local_queue, co);
        } else {
            vx_queue_push(&co->worker->runtime->global_submission_queue, co);
        }
    }
}

void vx_semaphore_init(vx_semaphore_t *sem, int initial_value) {
    atomic_init(&sem->value, initial_value);
    atomic_init(&sem->wait_queue.head, 0);
    atomic_init(&sem->wait_queue.tail, 0);
}

void vx_semaphore_wait(vx_semaphore_t *sem) {
    vx_worker_t *w = vx_current_worker();
    while (true) {
        int val = atomic_load(&sem->value);
        if (val > 0 && atomic_compare_exchange_weak(&sem->value, &val, val - 1)) {
            break;
        }
        if (w && w->current_coroutine) {
            w->current_coroutine->state = VX_STATE_BLOCKED;
            vx_queue_push(&sem->wait_queue, w->current_coroutine);
            vx_coroutine_t *co = w->current_coroutine;
            w->current_coroutine = NULL;
            swapcontext(&co->context, &w->worker_context);
        } else {
            usleep(1000);
        }
    }
}

void vx_semaphore_signal(vx_semaphore_t *sem) {
    atomic_fetch_add(&sem->value, 1);
    vx_coroutine_t *co = vx_queue_pop(&sem->wait_queue);
    if (co) {
        co->state = VX_STATE_READY;
        vx_worker_t *w = vx_current_worker();
        if (w) {
            vx_queue_push(&w->local_queue, co);
        } else {
            vx_queue_push(&co->worker->runtime->global_submission_queue, co);
        }
    }
}

vx_channel_t *vx_channel_create(size_t capacity) {
    vx_channel_t *chan = malloc(sizeof(vx_channel_t));
    if (!chan) return NULL;
    chan->capacity = capacity;
    chan->size = 0;
    chan->head = 0;
    chan->tail = 0;
    chan->buffer = malloc(sizeof(void *) * (capacity > 0 ? capacity : 1));
    vx_mutex_init(&chan->mutex);
    vx_semaphore_init(&chan->not_full, capacity > 0 ? (int)capacity : 0);
    vx_semaphore_init(&chan->not_empty, 0);
    return chan;
}

void vx_channel_destroy(vx_channel_t *chan) {
    if (!chan) return;
    free(chan->buffer);
    free(chan);
}

bool vx_channel_send(vx_channel_t *chan, void *item) {
    vx_semaphore_wait(&chan->not_full);
    vx_mutex_lock(&chan->mutex);
    chan->buffer[chan->tail] = item;
    chan->tail = (chan->tail + 1) % (chan->capacity > 0 ? chan->capacity : 1);
    chan->size++;
    vx_mutex_unlock(&chan->mutex);
    vx_semaphore_signal(&chan->not_empty);
    return true;
}

bool vx_channel_receive(vx_channel_t *chan, void **item) {
    vx_semaphore_wait(&chan->not_empty);
    vx_mutex_lock(&chan->mutex);
    *item = chan->buffer[chan->head];
    chan->head = (chan->head + 1) % (chan->capacity > 0 ? chan->capacity : 1);
    chan->size--;
    vx_mutex_unlock(&chan->mutex);
    vx_semaphore_signal(&chan->not_full);
    return true;
}
