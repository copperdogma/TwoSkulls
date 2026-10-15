# TwoSkulls
 Two talking animatronic skulls (Halloween 2024)

  Two Chatting Skulls with Skull Jaws Synced to Audio

  On startup:
  - Initializes serial communication via BLE
  - Initializes light controller for LED eyes
  - Mounts SD card, loads content, loads settings from config.txt file
  - Loads configuration from settings file
  - Determines skull role (primary or secondary) from config.txt file
  - Sets up AudioPlayer
  - Initializes Bluetooth A2DP (audio playback via bluetooth speakers)
  - Queues initialization audio (Primary or Secondary)
  - Primary attempts to find Secondary, playing "Marco" audio every 5 seconds until connected
  - Secondary responds with "Polo" after Primary finishes "Marco" and explicitly requests the reply
  - Primary initializes GPIO trigger pin for Matter controller
  - Primary starts monitoring GPIO trigger pin
  
  When Matter controller triggers (Primary Only):
  - Ignore if already playing sequence
  - Ignore if skit ended less than 10 seconds ago
  - Choose random skit to play, weighted to those least played. Never play same skit twice in a row.

  Skit Chosen
  - Primary sends which skit they're playing to Secondary
  - Secondary sends ACK if it's ready to play
  - If Primary receives ACK it starts playing immediately, otherwise it does nothing and waits for another Matter controller trigger
  - Both skulls load audio and associated txt file and play/execute animations
  - Each analyzes the audio they're playing in real-time, syncing their servo jaw motions to the audio


  LED Blinking Legend
  1 - Wifi connection attempt (PRIMARY)/receipt (SECONDARY)
  2 - this skull is SECONDARY
  4 - this skull is PRIMARY
  3 - SD card mount failed; retrying
  5 - failed to load config.txt; retrying


  Code originaly based off: Death_With_Skull_Synced_to_Audio

## Onboard SD card migration

The firmware now targets the onboard MicroSD slot on the FREENOVE ESP32-WROVER
board, using `SD_MMC` in 1-bit mode at 20 MHz, matching Death's setup.
**Move the jaw servo signal from GPIO15 to GPIO23 before running this firmware.**
GPIO15 is the SD command line. Remove the external SD module and all six of its
wires before reusing GPIO23. Keep the existing separate servo power supply.

See [the migration and bench checklist](docs/onboard-sd-migration.md) for the
pin map, card preparation, research, and the earlier Arduino baseline. The native
firmware has now passed paired audio and Apple Home bench checks on both boards;
Jeff's disconnected actuators and yard/soak checks remain pending.

## Development Workflow

The native prototype follows
[Story 001 — single-WROVER Anthony with Matter](docs/stories/story-001-single-wrover-matter-prototype.md),
with a [sibling-project and online research record](docs/research/2026-10-14-single-wrover-research.md).
The [native-IDF firmware](firmware/README.md) now implements both skull roles,
onboard SD playback, script role muting, startup diagnostics, jaw/eyes and idle
breathing. Anthony's Matter switch requests a paired skit; serial commands also
permit explicit local playback tests. Anthony's startup announcement and a full
local skit have played on the borrowed v3.0 board. Jeff's new board now runs the identical image selected by SD config. Both
completed a paired Starbucks skit with zero underruns, and Cam confirmed clean
turn-taking. Jeff actuator, measured acoustic timing, resource margin and yard
soak checks remain open. See the
[two-board bench evidence](docs/validation/2026-10-15-jeff-first-boot.md). See the
[application port record](docs/validation/2026-10-15-native-application-port.md).
See the [dated implementation evidence](docs/validation/2026-10-15-implementation-log.md).
The Arduino workflow below still describes the existing full show firmware.

This project uses a hybrid development approach:
- **Development**: Done in Cursor (or any preferred IDE) for better code editing, git integration, and development experience
- **Compilation/Flashing**: Done in Arduino IDE where ESP-IDF configuration is handled automatically

**Note**: You can also do full development, compilation, and flashing entirely in Arduino IDE if preferred. The hybrid approach is optional and chosen for better development experience.

SETUP:
Board: "ESP32 Wrover Module"
Port: "/dev/cu.usbserial-1430" (you'll need to figure out what port your ESP32 is on)
Core Debug Level: "None"
Erase All Flash Before Sketch Upload: "Disabled"
Flash Frequency: "80MHz"
Flash Mode: "QIO"
Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)" (originally "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)" but I ran out of space)
Upload Speed: "460800"

BOARD/LIBRARIES:
v2.0.17: esp32
v5.0.2 : ESP32 ESP32S2 AnalogWrite
v1.6.1 : arduinoFFT
v1.8.3 : ESP32-A2DP

SD CARD CONTENTS
/config.txt - config info, should be something like (role = primary or secondary):
      speaker_name=JBL Flip 5
      role=primary
/audio/Initialized - Primary.wav - required, speaks this first when it understands it's the primary skull and to show it's connected to bluetooth, reading from SD, and playing audio successfully
/audio/Initialized - Secondary.wav - required (for both Primary and Secondary), same purpose as Primary
/audio/Marco.wav - required, Primary skull will say this repeadedly when attempting to connect to Secondary skull
/audio/Polo.wav - required (for both Primary and Secondary), Secondary will say this when successfuly connected to Primary skull
/audio/Skit - example name.wav
/audio/Skit - example name.txt - for every skit you want the skulls to randomly say, you should have a wav and txt, with the txt identifying which skulls is speaking which parts. See here:

All audio files should be PCM (wav) formatted as 44.1kHz sampling rate, two-channel 16-bit sample data.

Skull Animation File Format (txt file):
NOTES:
A=Primary skull, B=Secondary skull
- default to close on init, and close at end of every sequence assigned to it
- When indicating jaw position, "duration" refers to how long it should take to open/close skull to that position. A duration of 0 is "as fast as you can".
- listen = dynamically analyze audio and sync jaw servo to that audio, i.e.: try to match the sound
speaker,timestamp,duration,(jaw position)

TXT FILE EXAMPLE (do not include the notes after the -):
A,30200,2000      - Primary (2s): I think you're just the worst person I've ever-
B,31500,1000      - Secondary (1s, cutting off Primary): I love you.
A,36000,300,.9    - Primary (300ms): opens jaw most of the way
A,37000,0,0       - Primary (0s): snaps jaw closed
B,47000,2000      - Secondary (2s): sorry, I was on the phone... you were saying?

Power
- Hardware is powered by 2 x USB-C cables.
  - One connects directly to the ESP32. This powers the ESP32 itself (logic, bluetooth, etc.), the LEDs, and the SD card reader.
  - The other powers the servo and the Matter controller, both of which require 5v.
  - The dual power is necessary because the servo draws too much current for the ESP32 to supply and was crashing the SD card reader.
- Battery Packs
  - PocketJuice Endurance: 8000mAh, output 5v/3.4A max:
    - runs breathing animation w/no speaker connection for 30 hours
  - GTOCE N6 Portable Charger: 40000mAh, output (depends on port): max USB-A port 2: 4.5V/5A

## Hardware Assembly Sequence

The following sequence should be followed when building each skull board, testing after each step:

1. **Power Converter Setup**
   - Solder power converter and test
   
2. **ESP32 WROVER Headers**
   - Solder female headers for ESP32 WROVER (main controller)
   - Solder 5v/GND from power converter to WROVER headers and test
   
3. **Main Controller**
   - Add WROVER and test to make sure it comes on
   
4. **LED Eyes**
   - Solder LED eye male header pins to GND/GPIOs and test
   
5. **Servo**
   - Solder servo male header pins to GND/5v/GPIO and test
   
6. **SD Card Module**
   - Insert the prepared card in the onboard MicroSD slot and test mounting/file reads
   
7. **Matter Controller**
   - Solder ESP32 mini (matter controller) to GND/5v/GPIO and test
   
8. **Final Components**
   - Solder capacitors and test

LEDs (x2, red, for eyes)
- connect to GPIO 32 and 33, plus single ground 
  - the two LED black wires are combined with a 100ohm resistor to stop them from drawing too many amps and burning out, which leads to a single black ground wire

SD card: onboard MicroSD slot (no external reader wiring)
- SD_MMC 1-bit mode, 20 MHz: CLK=GPIO14, CMD=GPIO15, D0=GPIO2
- Reserve GPIO14/15/2 for the SD bus; do not attach the servo to GPIO15.
- GPIO23 is now the jaw servo signal. GPIO5/18/19 are freed from the old reader.
- Keep GPIO4/12/13 free of new loads until the board's onboard slot wiring is checked.

Matter Controller (ESP32-C3)
 - GPIO 1: Connected to main ESP32 GPIO 1 for trigger signal
 - When activated, sends HIGH pulse to trigger random skit playback

Servo: Power HD HD-1160A
- 4.8v-6v, stall torque: 3.0kg.com, max current: 0.8A, 0.12s/60deg speed, Pulse width Modulation, 28 x 13.2 x 29.6 mm
- speed = 0.12s/60deg = 500 deg/s max speed
- VCC: 5v
- GND
- control (yellow): GPIO23 (moved from GPIO15 for onboard SD)


TROUBLESHOOTING:
- If it won't compile citing, library references, especially if you've just changed branches or did something large and disruptive,
  it may be a cachine issue with Arduino IDE. To clear the cache, compile for a different board (which will fail) and then switch
  back to the actual board and recompile.


FUTURE FEATURES
- phone control: make Primary into a BLE server as well, allowing phone to query skits, play specific ones, etc.
- speaker mode: let user use a microphone to play audio through the skulls, like a PA system. Ideally with one user controlling
  Primary and another controlling Secondary. This would require hardware changes (separate wifi module) as the ESP32 can't
  simultaneously drive bluetooth and wifi.
- remote monitoring
  - hook up the ESP32 cameras to a phone app so we can watch the skulls over wifi
  - take photos now and then when speaking to get spectator reactions
- AI integration
  - call out to OpenAI for dynamic skit generation and use ElevenLabs for AI voices
  - capture live camera footage to feed into AI for context when generating skits so it can comment on the people it can see


ISSUES

  TODO
  ** UPDATE LIBRARIES, but beware, ESP32-A2DP have breaking changes
  ** staging: mount on sticks, make name signs, put electronics in bag under their fun hats, figure out where/how to hide speakers
  ** create name skit txt files for remaining skits
  ** move the volume divisor onto the SD card?
    - maybe just get rid of it, seeing as we already have a volume level in the config.txt file...
  ** stress test: run them for an hour
    ** test battery pack life
  ** ISSUES
    ** clicking before/after audio



20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.
20240925: Full BLE communication working, Marco/Polo server connection, only attempts comms after Initialized wav played,
          and can play audio file in sync.
20240928: Animation (eyes only) synced to skit playback, Matter controller triggers random skit playback
20241005: Jaw animation works pretty well now.
