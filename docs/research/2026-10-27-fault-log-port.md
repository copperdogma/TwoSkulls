# Retained fault logging port — 2026-10-27

General problem: preserve useful error context when the SD storage being logged
may itself fail, without adding writes to real-time audio paths.

## Reuse and decision

Reuse the implemented/tested Death Story 018 logger rather than invent another
logging pipeline. Owning source reviewed at
`/Users/cam/Documents/Projects/death-fortune-teller/firmware/main/death_fault_*`
and its story/research records. Those records distinguish synthetic host tests
from actual Death card-error retention and later scheduled export.

The original bounded research cites ESP-IDF 5.5.4's concurrent custom log hook
contract: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/log.html
and the installed `components/nvs_flash/include/nvs.h`. Adopt the same bounded
RAM hook, deferred sole-owner persistence, fixed report slots and NVS fallback.
No fresh external behavior is asserted by this port; these references are the
original implementation rationale. Panic core dumps need separate partition
support and remain outside this logger.

Adapt tag filtering and warning detection to TwoSkulls, keep a separate
`skulls_fault` namespace, and run persistence on the existing control owner
only after `audio::quiescent()` and idle show/startup/tone. Both native roles
use this same code. Boot storage retries predate this change: the logger only
retains to flash there and does not add remounts. Source tests are ported and
locally pass with ASan/UBSan, including both-role filtering and failure recovery.

## Remaining uncertainty

Host fake-SDK tests demonstrate logic, not actual SD/NVS timing or free heap
under paired radio/audio load. Native build and device evidence are recorded
in Story 003. Until uploaded and bench-qualified, the Death hardware evidence
must not be presented as TwoSkulls device proof. The compact fallback preserves
only the report tail; abrupt power loss before persistence can still lose RAM.
