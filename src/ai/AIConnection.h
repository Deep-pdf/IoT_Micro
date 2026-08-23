#ifndef AI_CONNECTION_H
#define AI_CONNECTION_H

#include <Arduino.h>

class AIConnection {
public:
    static bool ensureWiFiConnected();
    static String postAsk(const String& question);
};

#endif // AI_CONNECTION_H
