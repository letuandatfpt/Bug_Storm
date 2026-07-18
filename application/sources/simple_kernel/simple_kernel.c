#include "simple_kernel.h"

#include <string.h>

typedef struct {
    uint8_t active;
    sk_task_id_t dst;
    sk_signal_t sig;
    uint32_t remaining_ms;
    uint32_t period_ms;
} sk_timer_t;

static const sk_task_t* sk_tasks;
static uint8_t sk_task_count;
static sk_task_id_t sk_current = SK_TASK_NONE;

static sk_msg_t sk_queue[SK_QUEUE_SIZE];
static uint8_t sk_queue_head;
static uint8_t sk_queue_tail;
static uint8_t sk_queue_count;

static sk_timer_t sk_timers[SK_MAX_TIMERS];
static sk_lock_fn_t sk_lock_hook;
static sk_lock_fn_t sk_unlock_hook;

static void sk_lock(void) {
    if (sk_lock_hook) {
        sk_lock_hook();
    }
}

static void sk_unlock(void) {
    if (sk_unlock_hook) {
        sk_unlock_hook();
    }
}

static const sk_task_t* sk_find_task(sk_task_id_t id) {
    uint8_t i;

    for (i = 0; i < sk_task_count; i++) {
        if (sk_tasks[i].id == id) {
            return &sk_tasks[i];
        }
    }

    return 0;
}

static int sk_queue_index_for_highest_priority(void) {
    uint8_t i;
    uint8_t pos = sk_queue_head;
    int best_index = -1;
    uint8_t best_priority = 0;

    for (i = 0; i < sk_queue_count; i++) {
        const sk_task_t* task = sk_find_task(sk_queue[pos].dst);

        if (task && (best_index < 0 || task->priority > best_priority)) {
            best_index = pos;
            best_priority = task->priority;
        }

        pos = (uint8_t)((pos + 1) % SK_QUEUE_SIZE);
    }

    return best_index;
}

static sk_msg_t sk_remove_queue_at(uint8_t index) {
    sk_msg_t msg = sk_queue[index];

    while (index != sk_queue_head) {
        uint8_t prev = (uint8_t)((index + SK_QUEUE_SIZE - 1) % SK_QUEUE_SIZE);
        sk_queue[index] = sk_queue[prev];
        index = prev;
    }

    sk_queue_head = (uint8_t)((sk_queue_head + 1) % SK_QUEUE_SIZE);
    sk_queue_count--;

    return msg;
}

void sk_set_lock_hooks(sk_lock_fn_t lock, sk_lock_fn_t unlock) {
    sk_lock_hook = lock;
    sk_unlock_hook = unlock;
}

uint8_t sk_init(const sk_task_t* tasks, uint8_t task_count) {
    if (!tasks || task_count == 0 || task_count > SK_MAX_TASKS) {
        return SK_ERR;
    }

    sk_lock();

    sk_tasks = tasks;
    sk_task_count = task_count;
    sk_current = SK_TASK_NONE;
    sk_queue_head = 0;
    sk_queue_tail = 0;
    sk_queue_count = 0;
    memset(sk_timers, 0, sizeof(sk_timers));

    sk_unlock();

    return SK_OK;
}

uint8_t sk_post(sk_task_id_t dst, sk_signal_t sig, uint16_t param) {
    return sk_post_from(SK_TASK_NONE, dst, sig, param);
}

uint8_t sk_post_from(sk_task_id_t src, sk_task_id_t dst, sk_signal_t sig, uint16_t param) {
    if (!sk_find_task(dst)) {
        return SK_ERR;
    }

    sk_lock();

    if (sk_queue_count >= SK_QUEUE_SIZE) {
        sk_unlock();
        return SK_ERR;
    }

    sk_queue[sk_queue_tail].src = src;
    sk_queue[sk_queue_tail].dst = dst;
    sk_queue[sk_queue_tail].sig = sig;
    sk_queue[sk_queue_tail].param = param;
    sk_queue_tail = (uint8_t)((sk_queue_tail + 1) % SK_QUEUE_SIZE);
    sk_queue_count++;

    sk_unlock();

    return SK_OK;
}

uint8_t sk_run_once(void) {
    int queue_index;
    sk_msg_t msg;
    const sk_task_t* task;

    sk_lock();
    queue_index = sk_queue_index_for_highest_priority();
    if (queue_index < 0) {
        sk_unlock();
        return SK_ERR;
    }

    msg = sk_remove_queue_at((uint8_t)queue_index);
    sk_unlock();

    task = sk_find_task(msg.dst);
    if (!task || !task->handler) {
        return SK_ERR;
    }

    sk_current = task->id;
    task->handler(&msg);
    sk_current = SK_TASK_NONE;

    return SK_OK;
}

void sk_run(sk_poll_fn_t idle_poll) {
    for (;;) {
        if (sk_run_once() != SK_OK && idle_poll) {
            idle_poll();
        }
    }
}

void sk_tick(uint32_t elapsed_ms) {
    uint8_t i;

    for (i = 0; i < SK_MAX_TIMERS; i++) {
        sk_task_id_t dst = SK_TASK_NONE;
        sk_signal_t sig = 0;

        sk_lock();

        if (!sk_timers[i].active) {
            sk_unlock();
            continue;
        }

        if (elapsed_ms < sk_timers[i].remaining_ms) {
            sk_timers[i].remaining_ms -= elapsed_ms;
            sk_unlock();
            continue;
        }

        dst = sk_timers[i].dst;
        sig = sk_timers[i].sig;

        if (sk_timers[i].period_ms > 0) {
            sk_timers[i].remaining_ms = sk_timers[i].period_ms;
        }
        else {
            sk_timers[i].active = 0;
        }

        sk_unlock();

        sk_post(dst, sig, 0);
    }
}

uint8_t sk_timer_set(sk_task_id_t dst, sk_signal_t sig, uint32_t timeout_ms, sk_timer_type_t type) {
    uint8_t i;

    if (!sk_find_task(dst) || timeout_ms == 0) {
        return SK_ERR;
    }

    sk_timer_stop(dst, sig);

    sk_lock();

    for (i = 0; i < SK_MAX_TIMERS; i++) {
        if (!sk_timers[i].active) {
            sk_timers[i].active = 1;
            sk_timers[i].dst = dst;
            sk_timers[i].sig = sig;
            sk_timers[i].remaining_ms = timeout_ms;
            sk_timers[i].period_ms = (type == SK_TIMER_PERIODIC) ? timeout_ms : 0;
            sk_unlock();
            return SK_OK;
        }
    }

    sk_unlock();

    return SK_ERR;
}

uint8_t sk_timer_stop(sk_task_id_t dst, sk_signal_t sig) {
    uint8_t i;
    uint8_t removed = SK_ERR;

    sk_lock();

    for (i = 0; i < SK_MAX_TIMERS; i++) {
        if (sk_timers[i].active && sk_timers[i].dst == dst && sk_timers[i].sig == sig) {
            sk_timers[i].active = 0;
            removed = SK_OK;
        }
    }

    sk_unlock();

    return removed;
}

sk_task_id_t sk_current_task(void) {
    return sk_current;
}
