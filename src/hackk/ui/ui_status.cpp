#include "hackk/ui_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t s_q = NULL;

static void ensure_queue() {
    if (!s_q) s_q = xQueueCreate(16, sizeof(ui_event_t));
}

void ui_status_publish(const ui_event_t *ev) {
    if (!ev) return;
    ensure_queue();
    if (!s_q) return;
    // Never block the attack task — drop if full
    xQueueSend(s_q, ev, 0);
}

bool ui_status_pop(ui_event_t *out) {
    if (!out || !s_q) return false;
    return xQueueReceive(s_q, out, 0) == pdTRUE;
}
