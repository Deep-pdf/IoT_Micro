#pragma once

// ── Deauth parameters ──────────────────────────────────────────
#define DEAUTH_BURST_COUNT        100   // frames per burst per target
#define DEAUTH_FRAME_DELAY_MS     1     // 1 ms inter-frame delay (~1000 fps max)
#define DEAUTH_BURST_PAUSE_MS     50    // was 500 — tighter loop = harder reconnect
#define DEAUTH_CHANNEL_HOLD_MS    50    // was 200
#define DEAUTH_EVENT_INTERVAL     50    // UI update every 50 frames (saves CPU for injection)
#define DEAUTH_SNIFF_MS           3000  // ms to sniff for client MACs before attacking
#define DEAUTH_MAX_CLIENTS        16    // max client MACs remembered per attack

// ── Evil Twin parameters ───────────────────────────────────────
#define EVILTWIN_RUN_SECONDS      300   // 5-minute auto-stop
#define EVILTWIN_PORTAL_IP        "192.168.4.1"
