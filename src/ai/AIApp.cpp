#include "AIApp.h"
#include "AIConnection.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include <vector>
#include <string>

// Helper function to draw the AI screen with "DEEPAI" header and a given body text
static void drawAIScreen(Adafruit_ST7735 &tft, const String &bodyText) {
    tft.fillScreen(ST77XX_BLACK);
    
    // Draw DEEPAI Header
    tft.setFont(&Dream_Orphans_Bd6pt7b);
    tft.setTextSize(2);
    uint16_t orange = tft.color565(255, 122, 0);
    tft.setTextColor(orange);
    
    // Center "DEEPAI"
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds("DEEPAI", 0, 24, &x1, &y1, &w, &h);
    int16_t x = (128 - w) / 2 - x1;
    tft.setCursor(x, 24);
    tft.print("DEEPAI");
    
    // Draw horizontal separator line
    tft.drawFastHLine(10, 32, 108, orange);
    
    // Draw body text (e.g. "Thinking..." or Gemini response) with word wrapping
    tft.setFont(&Dream_Orphans_Bd6pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    
    int16_t margin = 8;
    int16_t maxW = 128 - 2 * margin; // 112 px
    int16_t startY = 50;
    int16_t lineHeight = 14;
    
    // Split the body text by newline characters
    std::vector<std::string> lines;
    int lastIdx = 0;
    int len = bodyText.length();
    while (lastIdx < len) {
        int nextNewline = bodyText.indexOf('\n', lastIdx);
        int endIdx = (nextNewline == -1) ? len : nextNewline;
        String lineSegment = bodyText.substring(lastIdx, endIdx);
        
        // Wrap this segment to maxW width
        const char *ptr = lineSegment.c_str();
        std::string currentLine = "";
        
        while (*ptr) {
            // Skip leading spaces in a word
            while (*ptr == ' ') ptr++;
            if (*ptr == '\0') break;
            
            const char *wordStart = ptr;
            while (*ptr && *ptr != ' ') ptr++;
            std::string singleWord(wordStart, ptr - wordStart);
            
            std::string testLine = currentLine.empty() ? singleWord : currentLine + " " + singleWord;
            tft.getTextBounds(testLine.c_str(), 0, 0, &x1, &y1, &w, &h);
            
            if (w <= maxW) {
                currentLine = testLine;
            } else {
                if (!currentLine.empty()) {
                    lines.push_back(currentLine);
                }
                currentLine = singleWord;
            }
        }
        if (!currentLine.empty()) {
            lines.push_back(currentLine);
        } else if (lineSegment.isEmpty()) {
            lines.push_back(""); // maintain blank lines
        }
        
        lastIdx = endIdx + 1;
    }
    
    // Draw wrapped lines
    for (size_t i = 0; i < lines.size(); i++) {
        int16_t y = startY + i * lineHeight;
        if (y + lineHeight > 160) break; // Keep text within screen limits
        tft.setCursor(margin, y);
        tft.print(lines[i].c_str());
    }
}

void AIApp::init(Adafruit_ST7735 &tft) {
    // 1. Clear screen and show "DEEPAI" and "Thinking..."
    drawAIScreen(tft, "Thinking...");
    
    // 2. Send the question to the AI Connection bridge
    String response = AIConnection::postAsk("Say hello to my ESP32 in one short sentence.");
    
    // 3. Render the response text
    drawAIScreen(tft, response);
}

void AIApp::update(Adafruit_ST7735 &tft) {
    // In this phase, we don't have interactive elements on this screen.
    // The main loop handles the BACK button and exits.
}
