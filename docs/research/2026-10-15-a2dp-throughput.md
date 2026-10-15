# A2DP scheduling margin — 2026-10-15

General problem: continuous real-time media production/encoding can starve a
lower-priority idle task and exhaust a finite buffer even when individual
callbacks remain bounded. Measure both throughput and scheduler health; source
frame completion alone is insufficient because inserted silence can alter timing.

Primary guidance: Espressif's [ESP-IDF v5.5.4 speed guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/performance/speed.html)
recommends timing measurements, task-priority awareness and explicit blocking so
lower-priority tasks can run. Bluetooth BTC/BTU/controller have high priorities;
Bluedroid tasks share a configurable core. The pinned SDK's ESP32 CPU Kconfig
supports 240 MHz. This is distinct from changing flash/PSRAM clocks.

The second catalogue capture uses 160 MHz and retains bounded producer bursts
(two blocks maximum, then at least one RTOS tick). Jeff repeatedly reports IDLE0
task-watchdog starvation; decoded CPU0 stacks include SBC_Encoder,
SbcAnalysisFilter8 and FastIDCT8 in BTC_TASK, while CPU1 is idle. Favourite movie
completed all 2157034 source frames but Jeff inserted 41 underruns; Anthony zero.
Jeff's measured read/process averages are 5.53/10.03 ms per 20 ms source block;
Anthony's are 5.00/6.27 ms. These are wall times, including scheduling delays,
not exclusive CPU time. Delay(1) is tick-phase-dependent, so adding a nominal
sleep to these values does not prove an exact steady-state throughput deficit.

Decision: test 240 MHz first, preserving queues, task affinity, yielding,
flash/PSRAM clocks, watchdogs and radio configuration. Moving only the producer
to CPU1 would not directly fix measured CPU0 SBC saturation. The image also
includes a small greeting/manual-retry edge fix, unrelated to the media path;
record this qualification limitation rather than claim a perfectly isolated image.

Require complete frames, zero underruns and zero task-watchdog events on both
boards, including repeated favourite-movie playback. If encoding starvation
clears but underruns remain, measure/pin the producer as a separate experiment.
A larger queue can hide transient delay but cannot fix inadequate sustained
throughput. Do not remove blocking or disable the watchdog to manufacture a pass.

Hardware comparison pending. Old captures retain their initial frame-only PASS
labels; repeated task-watchdog messages independently fail scheduling acceptance.
The updated harness captures watchdog counts explicitly for future runs.

## Bounded SBC encoder optimization experiment

The 240 MHz catalogue capture eliminated the observed early underruns, but Jeff
still reported five IDLE0 watchdog timeouts during `Skit - ju jubes.wav`, despite
zero source underruns. Case-insensitive decoding of all captured PC addresses
against that capture's retained ELF identifies SBC analysis filtering, FastIDCT8,
packing, and stereo bit allocation, all called through `SBC_Encoder` from the
BTC A2DP timer handler. CPU1 was idle at these watchdog samples. This is evidence
of encoder scheduling pressure, not proof of an encoder infinite loop.

Decision: apply `-O2` only to the pinned SDK's eight SBC encoder translation
units through the project-owned `firmware/cmake/sbc_encoder_optimization.cmake`.
The generated commands previously used `-Os`. Espressif's speed guide above
recommends performance optimization and measuring selected hotspots. Source
options must appear after existing `-Os`; all non-encoder sources remain
size-optimized. The include fails configuration if the expected source count
changes. No SDK source is edited. Keep 240 MHz, task placement, producer FIFO,
burst limits, yields, and watchdog settings unchanged for this experiment.

The SDK timer path (`btc_a2dp_source.c`, `btc_get_num_aa_frame`) already accounts
for elapsed time and caps frames per invocation. Changing its backlog behavior
would change transport semantics, so defer that workaround. Moving the producer
alone cannot address the sampled SBC hotspot; moving all Bluedroid tasks could
move starvation to another core rather than reduce required encoder work.

Also flatten all four negotiated SBC codec bytes into the existing event queue
and log the frequency/channel/block/subband/allocation masks plus min/max
bitpool on the radio task. This distinguishes speaker negotiation without
logging inside a Bluetooth callback or retaining callback-owned pointers.

Remaining uncertainty: whether targeted optimization supplies enough scheduling
margin, whether speaker codec negotiation explains the primary/secondary
difference, and whether extra encoder code exceeds the tight IRAM budget.
Before flashing, compare image/IRAM size. Then replay ju jubes and favourite
movie repeatedly, requiring complete frames, zero underruns, and zero watchdog
events on both boards before repeating the full catalogue. Configuration and
command inspection alone do not establish hardware acceptance.

Local verification: CMake configuration succeeded without compiling firmware.
Inspection of generated `firmware/build/compile_commands.json` found exactly
eight encoder commands with optimization sequence `-Os -O2`, and all 308 other
Bluetooth commands still end in `-Os`. A Ninja dry run for `sbc_encoder.c.obj`
scheduled that object for rebuilding without executing the compiler. Resource
size and hardware results remain pending the root-owned build and serial tests.
