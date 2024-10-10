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
  - Secondary responds with "Polo" audio when connected to Primary
  - Primary starts monitoring ultrasonic sensor
  
  When ultrasonic sensor triggered (Primary Only):
  - Ignore if already playing sequence
  - Ignore if skit ended less than 10 seconds ago
  - Choose random skit to play, weighted to those least played. Never play same skit twice in a row.

  Skit Chosen
  - Primary sends which skit they're playing to Secondary
  - Secondary sends ACK if it's ready to play
  - If Primary receives ACK it starts playing immediately, otherwise it does nothing and waits for another ultrasonic trigger
  - Both skulls load audio and associated txt file and play/execute animations
  - Each analyzes the audio they're playing in real-time, syncing their servo jaw motions to the audio


  LED Blinking Legend
  1 - Wifi connection attempt (PRIMARY)/receipt (SECONDARY)
  2 - this skull is SECONDARY
  4 - this skull is PRIMARY
  3 - SD card mount failed; retrying
  5 - failed to load config.txt; retrying


  Code originall based off: Death_With_Skull_Synced_to_Audio

SETUP:
Board: "ESP32 Wrover Module"
Port: "/dev/cu.usbserial-1430"
Get Board Info
Core Debug Level: "None"
Erase All Flash Before Sketch Upload: "Disabled"
Flash Frequency: "80MHz"
Flash Mode: "QIO"
Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)" (originally "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)" but I ran out of space)
Upload Speed: "460800"

BOARD/LIBRARIES:
v2.0.17: esp32
v5.0.2 : ESP32 ESP32S2 AnalogWrite
v2.0.0 : HCSR04
v1.6.1 : arduinoFFT
v1.8.3 : ESP32-A2DP

SD CARD CONTENTS
/config.txt - config info, should be something like (role = primary or secondary):
      speaker_name=JBL Flip 5
      role=primary
      ultrasonic_trigger_distance=100
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
  - The other powers the servo and the ultrasonic sensor, both of which require 5v.
  - The dual power is necessary because the servo draws too much current for the ESP32 to supply and was crashing the SD card reader.
- Battery Packs
  - PocketJuice Endurance: 8000mAh, output 5v/3.4A max
  - GTOCE N6 Portable Charger: 40000mAh, output (depends on port): max USB-A port 2: 4.5V/5A

LEDs (x2, red, for eyes)
- connect to GPIO 32 and 33, plus single ground 
  - the two LED black wires are combined with a 100ohm resistor to stop them from drawing too many amps and burning out, which leads to a single black ground wire

SD Card pins (from  FZ0829-micro-sd-card-module-with-ESP32.docx):
    SD Card | ESP32
    CS​        D5
    SCK  ​     D18
    MOSI​      D23
    MISO​      D19
    VCC       VIN (voltage input) -- works on 3.3v
    GND       GND

Ultrasonic Sensor HC-SR04
 - Driver: https://github.com/Martinsos/arduino-lib-hc-sr04
 - VCC: 5v
 - TRIG: PIN 2
 - ECHO: PIN 22
 - Distance trigger (set on SD card's config.txt file): 300cm

Servo: Power HD HD-1160A
- 4.8v-6v, stall torque: 3.0kg.com, max current: 0.8A, 0.12s/60deg speed, Pulse width Modulation
- speed = 0.12s/60deg = 500 deg/s max speed



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
  ** stress test: run them for an hour
    ** test battery pack life
  ** ISSUES
    ** clicking before/after audio
    ** if ultrasonic distance has object within it's max distance (300cm) when it starts, it'll just trigger over and over
      - FIX: initialize with 10-sample check to see what exists in that range, and only trigger if it's something that shouldn't interfere with normal operation



20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.
20240925: Full BLE communication working, Marco/Polo server connection, only attempts comms after Initialized wav played,
          and can play audio file in sync.
20240928: Animation (eyes only) synced to skit playback, ultrasonic sensor triggers random skit playback
20241005: Jaw animation works pretty well now.

## Ultrasonic Sensor Calibration

The project now includes an automatic calibration for the ultrasonic sensor at startup. This calibration sets a baseline distance, and object detection is based on deviations from this baseline. The system uses a tolerance of ±10cm from the baseline to determine if an object is detected.

Key points:
- Calibration occurs once at startup
- Baseline distance is an average of 20 readings
- Object detection threshold: baseline ± 10cm
- No periodic recalibration required
