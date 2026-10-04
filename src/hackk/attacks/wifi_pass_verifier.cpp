#include <stdbool.h>
#include <string.h>
#include "esp_wifi.h"

extern "C" bool wifi_verify_password(const char *ssid, const char *password)
{
    (void)ssid;
    (void)password;
    return false;   // feature removed
}
