#pragma once

typedef enum {
    INPUT_NONE = 0,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_ENTER,
    INPUT_BACK
} ui_input_t;

void       ui_input_init();
ui_input_t ui_input_poll(int timeout_ms);   // blocking with timeout
