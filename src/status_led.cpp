#include "status_led.h"
#include <Arduino.h>
#include <math.h>

static TaskHandle_t      s_ledTaskHandle = NULL;
static volatile bool     s_enabled       = (STATUS_LED_ENABLED != 0);
static volatile bool     s_breathe       = (STATUS_LED_BREATHE_MODE != 0);

// FreeRTOS task running smooth continuous breathing PWM on GPIO 26
static void statusLedTask(void *param) {
    const float twoPi  = 6.28318530718f;
    const float minVal = 0.36787944f; // exp(-1)
    const float span   = 2.35040239f; // exp(1) - exp(-1)
    
    // Start at bottom of breath (-PI/2)
    float phase = -1.5707963f;

    while (true) {
        if (!s_enabled) {
            ledcWrite(STATUS_LED_PWM_CHANNEL, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!s_breathe) {
            ledcWrite(STATUS_LED_PWM_CHANNEL, STATUS_LED_MAX_BRIGHTNESS);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Calculate dynamic phase increment from configured period
        float steps = (float)STATUS_LED_BREATHE_PERIOD_MS / (float)STATUS_LED_UPDATE_STEP_MS;
        if (steps < 10.0f) steps = 10.0f; // Safety clamp
        float phaseStep = twoPi / steps;

        // Cinematic organic breathing curve:
        // When sin(phase) == -1: exp(-1) = 0.367879 -> 0.0 (smooth valley)
        // When sin(phase) == +1: exp(1)  = 2.718281 -> 1.0 (smooth peak)
        float normalized = (expf(sinf(phase)) - minVal) / span;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        uint32_t duty = (uint32_t)(STATUS_LED_MIN_BRIGHTNESS + 
                                   normalized * (STATUS_LED_MAX_BRIGHTNESS - STATUS_LED_MIN_BRIGHTNESS) + 0.5f);
        if (duty > 255) duty = 255;

        ledcWrite(STATUS_LED_PWM_CHANNEL, duty);

        phase += phaseStep;
        if (phase >= twoPi) {
            phase -= twoPi;
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_LED_UPDATE_STEP_MS));
    }
}

void initStatusLed() {
    if (!s_enabled) {
        // Option 1 disabled (false / 0): Keep light turned off on boot
        pinMode(STATUS_LED_PIN, OUTPUT);
        digitalWrite(STATUS_LED_PIN, LOW);
        return;
    }

    // Configure ESP32 hardware PWM on GPIO 26
    ledcSetup(STATUS_LED_PWM_CHANNEL, STATUS_LED_PWM_FREQ, STATUS_LED_PWM_RES);
    ledcAttachPin(STATUS_LED_PIN, STATUS_LED_PWM_CHANNEL);

    if (!s_breathe) {
        // Option 2 set to solid (0 / false): Constant steady green light
        ledcWrite(STATUS_LED_PWM_CHANNEL, STATUS_LED_MAX_BRIGHTNESS);
    } else {
        // Option 2 set to breathing (1 / true): Launch background fade task
        if (s_ledTaskHandle == NULL) {
            xTaskCreate(statusLedTask, "status_led", 2048, NULL, 1, &s_ledTaskHandle);
        }
    }
}

void setStatusLedEnabled(bool enabled) {
    s_enabled = enabled;
    if (!s_enabled) {
        ledcWrite(STATUS_LED_PWM_CHANNEL, 0);
    } else if (s_breathe && s_ledTaskHandle == NULL) {
        xTaskCreate(statusLedTask, "status_led", 2048, NULL, 1, &s_ledTaskHandle);
    }
}

void setStatusLedBreathe(bool breathe) {
    s_breathe = breathe;
    if (s_breathe && s_ledTaskHandle == NULL && s_enabled) {
        xTaskCreate(statusLedTask, "status_led", 2048, NULL, 1, &s_ledTaskHandle);
    }
}

void setStatusLedDuty(uint8_t duty) {
    s_breathe = false;
    ledcWrite(STATUS_LED_PWM_CHANNEL, duty);
}
