#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EVT_DEAUTH_STARTED,
    EVT_DEAUTH_STOPPED,
    EVT_DEAUTH_FRAME,
    EVT_EVILTWIN_STARTED,
    EVT_EVILTWIN_STOPPED,
    EVT_CREDS_CAPTURED,
    EVT_LOG,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    char     line[96];
    uint32_t counter;
} ui_event_t;

void ui_status_publish(const ui_event_t *ev);
bool ui_status_pop(ui_event_t *out);
