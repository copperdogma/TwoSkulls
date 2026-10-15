#pragma once
#include <cstdint>
namespace skull::catalog { struct Catalog; }
namespace skull::show {
void init(catalog::Catalog *assets, bool primary);
// Safe from Matter's system task: bounded single pending trigger, no backlog.
bool trigger();
// Diagnostic/control task only. Local playback deliberately bypasses the peer
// handshake, so it is available for single-board acceptance before Jeff returns.
bool local(const char *path);
void tick(bool initialization_finished, bool startup_active);
bool busy();
const char *state_name();
}
