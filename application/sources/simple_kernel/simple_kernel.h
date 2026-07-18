#ifndef SIMPLE_KERNEL_H
#define SIMPLE_KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef SK_MAX_TASKS
#define SK_MAX_TASKS        8
#endif

#ifndef SK_QUEUE_SIZE
#define SK_QUEUE_SIZE       16
#endif

#ifndef SK_MAX_TIMERS
#define SK_MAX_TIMERS       8
#endif

#define SK_OK               1
#define SK_ERR              0
#define SK_TASK_NONE        0xFF

typedef uint8_t sk_task_id_t;
typedef uint8_t sk_signal_t;

typedef struct {
    sk_task_id_t src;
    sk_task_id_t dst;
    sk_signal_t sig;
    uint16_t param;
} sk_msg_t;

typedef void (*sk_task_fn_t)(const sk_msg_t* msg);
typedef void (*sk_poll_fn_t)(void);
typedef void (*sk_lock_fn_t)(void);

typedef struct {
    sk_task_id_t id;
    uint8_t priority;
    sk_task_fn_t handler;
} sk_task_t;

typedef enum {
    SK_TIMER_ONE_SHOT = 0,
    SK_TIMER_PERIODIC
} sk_timer_type_t;

void sk_set_lock_hooks(sk_lock_fn_t lock, sk_lock_fn_t unlock);
uint8_t sk_init(const sk_task_t* tasks, uint8_t task_count);
uint8_t sk_post(sk_task_id_t dst, sk_signal_t sig, uint16_t param);
uint8_t sk_post_from(sk_task_id_t src, sk_task_id_t dst, sk_signal_t sig, uint16_t param);
uint8_t sk_run_once(void);
void sk_run(sk_poll_fn_t idle_poll);
void sk_tick(uint32_t elapsed_ms);
uint8_t sk_timer_set(sk_task_id_t dst, sk_signal_t sig, uint32_t timeout_ms, sk_timer_type_t type);
uint8_t sk_timer_stop(sk_task_id_t dst, sk_signal_t sig);
sk_task_id_t sk_current_task(void);

#ifdef __cplusplus
}
#endif

#endif
