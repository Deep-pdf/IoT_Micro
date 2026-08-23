#include "AIConnection.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Timeout for HTTP requests (30 seconds)
const unsigned long HTTP_TIMEOUT_MS = 30000;

// Phone bridge URL
const char* const PHONE_BRIDGE_URL = "http://10.240.13.100:8080/ask";

bool AIConnection::ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    Serial.println("AI Connection: WiFi not connected, waiting for connection...");
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    return WiFi.status() == WL_CONNECTED;
}

// Simple manual JSON parser to extract the "answer" field
static String extractAnswer(const String& json) {
    int index = json.indexOf("\"answer\"");
    if (index == -1) return "";
    
    int colonIndex = json.indexOf(':', index);
    if (colonIndex == -1) return "";
    
    int startQuote = json.indexOf('"', colonIndex);
    if (startQuote == -1) return "";
    
    String result = "";
    for (int i = startQuote + 1; i < json.length(); i++) {
        char c = json[i];
        if (c == '\\') {
            if (i + 1 < json.length()) {
                char next = json[i + 1];
                if (next == 'n') result += '\n';
                else if (next == 't') result += '\t';
                else if (next == '\"') result += '\"';
                else if (next == '\\') result += '\\';
                else result += next;
                i++;
            }
        } else if (c == '\"') {
            break;
        } else {
            result += c;
        }
    }
    return result;
}

String AIConnection::postAsk(const String& question) {
    if (!ensureWiFiConnected()) {
        return "ERROR: WiFi not connected";
    }

    HTTPClient http;
    http.begin(PHONE_BRIDGE_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    // Escape quotes and backslashes in the question
    String escapedQuestion = "";
    for (size_t i = 0; i < question.length(); i++) {
        char c = question[i];
        if (c == '\"') {
            escapedQuestion += "\\\"";
        } else if (c == '\\') {
            escapedQuestion += "\\\\";
        } else {
            escapedQuestion += c;
        }
    }
    String payload = "{\"question\":\"" + escapedQuestion + "\"}";

    Serial.print("AI Connection: Sending POST payload: ");
    Serial.println(payload);

    int httpResponseCode = http.POST(payload);
    String response = "";

    if (httpResponseCode > 0) {
        response = http.getString();
        Serial.print("AI Connection: HTTP Response code: ");
        Serial.println(httpResponseCode);
        Serial.print("AI Connection: Response body: ");
        Serial.println(response);
    } else {
        Serial.print("AI Connection: Error on sending POST: ");
        Serial.println(http.errorToString(httpResponseCode).c_str());
        http.end();
        return "ERROR: Connection failed (" + String(httpResponseCode) + ")";
    }

    http.end();

    if (httpResponseCode != 200) {
        return "ERROR: HTTP Status " + String(httpResponseCode);
    }

    String answer = extractAnswer(response);
    if (answer.isEmpty()) {
        return "ERROR: Could not parse response";
    }

    return answer;
}
