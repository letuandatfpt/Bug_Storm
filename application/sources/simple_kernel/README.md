# Simple Kernel

Kernel này là bản tối giản để học và thử nghiệm, chưa thay thế AK kernel mặc định.

Nó có:

- cooperative scheduler
- priority theo task
- message queue tĩnh
- software timer đơn giản
- hook khóa ngắt tùy chọn

Ví dụ:

```c
#include "simple_kernel.h"

enum {
    TASK_LED,
    SIG_LED_TOGGLE = 1,
};

static void task_led(const sk_msg_t* msg) {
    if (msg->sig == SIG_LED_TOGGLE) {
        /* toggle led here */
    }
}

static const sk_task_t tasks[] = {
    { TASK_LED, 1, task_led },
};

void app(void) {
    sk_init(tasks, 1);
    sk_timer_set(TASK_LED, SIG_LED_TOGGLE, 500, SK_TIMER_PERIODIC);
    sk_run(0);
}

void systick_1ms_irq(void) {
    sk_tick(1);
}
```

Nếu dùng trong firmware STM32 thực tế, nên gọi `sk_set_lock_hooks()` với hàm disable/enable interrupt của platform để bảo vệ queue khi post message từ interrupt.
