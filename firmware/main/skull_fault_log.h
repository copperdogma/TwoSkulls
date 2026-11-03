#pragma once
#include <cstdint>
#include "skull_lifecycle.h"
namespace skull::faultlog {
// After nvs_flash_init, before storage mount. Bounded PSRAM allocation; failure
// leaves normal serial logging operational. No partition/NVS erasure.
void init();
// Sole control owner. idle must exclude audio/startup/show/tone work.
// No remount/unmount; retry an unavailable mounted card once per 60 seconds.
void poll(bool idle,bool card_mounted);
struct Snapshot { bool pending; uint64_t id; };
Snapshot snapshot();
Lifecycle::Metrics metrics();
void status();
void dump();
// Control owner only, idle and SD mounted: read fixed saved slots without writes.
void dump_saved();
// Idle CLI fixture, exercises production capture/persistence without corruption.
void test_fault();
}
