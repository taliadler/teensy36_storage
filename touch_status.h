#ifndef touch_status_h_
#define touch_status_h_

#include <stdint.h>

#define MAX_TOUCHES 10

struct touch_status {
    int is_active[MAX_TOUCHES];
    uint16_t x[MAX_TOUCHES];
    uint16_t y[MAX_TOUCHES];
    uint8_t size[MAX_TOUCHES];
};

extern struct touch_status current_touch_status;

extern uint16_t touch_x_max;
extern uint16_t touch_y_max;

#endif // touch_status_h_
