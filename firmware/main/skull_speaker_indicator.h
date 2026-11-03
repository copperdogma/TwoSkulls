#pragma once
#include <cstdint>

namespace skull::actuators {
// Monotonic microseconds since boot, not since the first (possibly late) poll.
// Owned solely by the actuator/control task; no callback touches this state.
struct SpeakerIndicator {
    bool connected_once = false;
    bool active = false;
    int64_t started_us = 0;

    void update(int64_t now_us, bool connected) {
        connected_once = connected_once || connected;
        const bool next = !connected && (connected_once || now_us >= 30000000);
        if (next && !active) started_us = now_us;
        active = next;
    }
    bool bright(int64_t now_us) const {
        return ((now_us - started_us) / 1000000) % 2 == 0;
    }
};
}
