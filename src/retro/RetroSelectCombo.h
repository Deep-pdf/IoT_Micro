#ifndef RETRO_SELECT_COMBO_H
#define RETRO_SELECT_COMBO_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Reliable simultaneous two-button combination detector for Retro Arcade.
 * 
 * Maps Button A (GPIO 13) + Joystick Click (GPIO 32) -> SELECT.
 * 
 * Features:
 * - Detects simultaneous A + B button press within a configurable window (default 60 ms).
 * - Suppresses individual A and B actions while the combo is being formed and while active.
 * - Asserts SELECT continuously while both buttons remain held.
 * - Releases SELECT immediately when either button is released.
 * - If one button is released while the other remains held, transitions cleanly to the
 *   remaining button's normal action without unintended duplicate presses.
 * - Preserves fast, responsive independent single-button presses (including short taps).
 * - Zero blocking delays (pure non-blocking state machine using millis()).
 */
class RetroSelectCombo {
public:
    // Configurable combination-detection window in milliseconds.
    // Prevents accidental single A or B button presses while the player is forming the combo.
    static constexpr uint32_t COMBO_WINDOW_MS = 60;

    // Contact bounce release filter in milliseconds (active LOW buttons)
    static constexpr uint32_t DEBOUNCE_RELEASE_MS = 20;

    // Minimum duration in milliseconds to hold an emitted tap event for single-button quick presses
    static constexpr uint32_t TAP_HOLD_MS = 35;

    RetroSelectCombo() {
        reset();
    }

    void reset() {
        m_state = STATE_IDLE;
        m_pressTimeA = 0;
        m_pressTimeB = 0;
        m_releaseTime = 0;
        m_tapUntilA = 0;
        m_tapUntilB = 0;
        m_btnA.reset();
        m_btnB.reset();
    }

    /**
     * @brief Polls physical button levels and resolves independent vs combo actions.
     * 
     * @param rawA True if Button A (GPIO 13) is physically pressed (LOW).
     * @param rawB True if Button B (GPIO 32) is physically pressed (LOW).
     * @param now Current timestamp in milliseconds (millis()).
     * @param[out] outA Resolved Button A action.
     * @param[out] outB Resolved Button B action.
     * @param[out] outSelect Resolved SELECT combo action.
     */
    void update(bool rawA, bool rawB, uint32_t now, bool &outA, bool &outB, bool &outSelect) {
        // 1. Debounce physical inputs (0 ms press latency, 20 ms release filter)
        m_btnA.update(rawA, now);
        m_btnB.update(rawB, now);

        bool a = m_btnA.stable;
        bool b = m_btnB.stable;

        outA = false;
        outB = false;
        outSelect = false;

        switch (m_state) {
            case STATE_IDLE: {
                if (a && b) {
                    // Both pressed simultaneously in the same poll frame
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else if (a) {
                    m_state = STATE_PENDING_A;
                    m_pressTimeA = now;
                } else if (b) {
                    m_state = STATE_PENDING_B;
                    m_pressTimeB = now;
                }
                break;
            }

            case STATE_PENDING_A: {
                if (b) {
                    // Button B joined within the detection window -> SELECT combo formed!
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else if (!a) {
                    // Button A released before combo window expired without B -> quick tap of A!
                    m_state = STATE_IDLE;
                    m_tapUntilA = now + TAP_HOLD_MS;
                    outA = true;
                } else if ((now - m_pressTimeA) >= COMBO_WINDOW_MS) {
                    // Window expired without B being pressed -> confirmed Button A hold
                    m_state = STATE_A_ONLY;
                    outA = true;
                }
                // While still within window, suppress A and B
                break;
            }

            case STATE_PENDING_B: {
                if (a) {
                    // Button A joined within the detection window -> SELECT combo formed!
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else if (!b) {
                    // Button B released before combo window expired without A -> quick tap of B!
                    m_state = STATE_IDLE;
                    m_tapUntilB = now + TAP_HOLD_MS;
                    outB = true;
                } else if ((now - m_pressTimeB) >= COMBO_WINDOW_MS) {
                    // Window expired without A being pressed -> confirmed Button B hold
                    m_state = STATE_B_ONLY;
                    outB = true;
                }
                // While still within window, suppress A and B
                break;
            }

            case STATE_A_ONLY: {
                if (!a) {
                    // Button A released
                    m_state = STATE_IDLE;
                } else if (b) {
                    // Button B pressed while holding A -> enter SELECT combo!
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else {
                    outA = true;
                }
                break;
            }

            case STATE_B_ONLY: {
                if (!b) {
                    // Button B released
                    m_state = STATE_IDLE;
                } else if (a) {
                    // Button A pressed while holding B -> enter SELECT combo!
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else {
                    outB = true;
                }
                break;
            }

            case STATE_COMBO: {
                if (!a && !b) {
                    // Both released simultaneously
                    m_state = STATE_IDLE;
                } else if (!b && a) {
                    // Button B released, Button A still held
                    // SELECT releases immediately!
                    m_state = STATE_COMBO_RELEASE_A;
                    m_releaseTime = now;
                } else if (!a && b) {
                    // Button A released, Button B still held
                    // SELECT releases immediately!
                    m_state = STATE_COMBO_RELEASE_B;
                    m_releaseTime = now;
                } else {
                    // Both remain held
                    outSelect = true;
                }
                break;
            }

            case STATE_COMBO_RELEASE_A: {
                if (!a) {
                    // Button A also released within combo release window -> clean full release
                    m_state = STATE_IDLE;
                } else if (b) {
                    // Button B pressed again -> re-enter combo
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else if ((now - m_releaseTime) >= COMBO_WINDOW_MS) {
                    // Button A remains held past the combo release window -> transition cleanly to Button A action!
                    m_state = STATE_A_ONLY;
                    outA = true;
                }
                break;
            }

            case STATE_COMBO_RELEASE_B: {
                if (!b) {
                    // Button B also released within combo release window -> clean full release
                    m_state = STATE_IDLE;
                } else if (a) {
                    // Button A pressed again -> re-enter combo
                    m_state = STATE_COMBO;
                    outSelect = true;
                } else if ((now - m_releaseTime) >= COMBO_WINDOW_MS) {
                    // Button B remains held past the combo release window -> transition cleanly to Button B action!
                    m_state = STATE_B_ONLY;
                    outB = true;
                }
                break;
            }
        }

        // Handle active tap extensions (if not in combo)
        if (m_tapUntilA > 0) {
            if (now < m_tapUntilA) {
                outA = true;
            } else {
                m_tapUntilA = 0;
            }
        }
        if (m_tapUntilB > 0) {
            if (now < m_tapUntilB) {
                outB = true;
            } else {
                m_tapUntilB = 0;
            }
        }

        // Hard suppression: while SELECT combo is active, never emit A or B
        if (outSelect) {
            outA = false;
            outB = false;
            m_tapUntilA = 0;
            m_tapUntilB = 0;
        }
    }

private:
    enum State : uint8_t {
        STATE_IDLE = 0,
        STATE_PENDING_A,
        STATE_PENDING_B,
        STATE_A_ONLY,
        STATE_B_ONLY,
        STATE_COMBO,
        STATE_COMBO_RELEASE_A,
        STATE_COMBO_RELEASE_B
    };

    struct DebouncedContact {
        bool     stable = false;
        bool     rawState = false;
        uint32_t lastEdgeMs = 0;

        void reset() {
            stable = false;
            rawState = false;
            lastEdgeMs = 0;
        }

        void update(bool isPressed, uint32_t now) {
            if (isPressed != rawState) {
                rawState = isPressed;
                lastEdgeMs = now;
            }
            if (isPressed && !stable) {
                // Immediate 0 ms response on press
                stable = true;
            } else if (!isPressed && stable) {
                // Require stable release to filter mechanical contact chatter
                if ((now - lastEdgeMs) >= DEBOUNCE_RELEASE_MS) {
                    stable = false;
                }
            }
        }
    };

    State            m_state = STATE_IDLE;
    uint32_t         m_pressTimeA = 0;
    uint32_t         m_pressTimeB = 0;
    uint32_t         m_releaseTime = 0;
    uint32_t         m_tapUntilA = 0;
    uint32_t         m_tapUntilB = 0;
    DebouncedContact m_btnA;
    DebouncedContact m_btnB;
};

#endif // RETRO_SELECT_COMBO_H

