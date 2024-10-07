# TwoSkulls
 Two talking animatronic skulls (Halloween 2024)

  Two Chatting Skulls with Skull Jaws Synced to Audio

  On startup:
  - decides it's Primary if it has the ultrasonic sensor attached, otherwise it decides it's the Secondary
  - each skull will play their individual (Primary/Secondary) "initialized" audio so we know they've found their role properly and that they've connected to their individual bluetooth speaker
  - Primary: plays "Marco" every 5 seconds until connnected to Secondary
  - Secondary: plays "Polo" every 5 seconds until conencted to Primary
  - Once both connected, they'll play skit: "Morning, Jeff." "Morning, Tony."
  - Then Primary starts monitoring ultrasonic sensor.

  System Operation:

   - Primary connects to Seondary via WiFi to confirm presence
   - Primary has two modes:
     a. Listening: 
        - Waits for ultrasonic sensor trigger
        - Selects random audio file
        - Instructs Secondary to play that file (Priamry plays it at the exact same time)
     b. Playing: 
        - Ignores triggers until playback completes

  Wifi/Bluetooth Radio: Should have minimal overlap. When playing audio there is no wifi communication and vice versa.


  LED Blinking Legend
  1 - Wifi connection attempt (PRIMARY)/receipt (SECONDARY)
  2 - this skull is SECONDARY
  4 - this skull is PRIMARY
  3 - SD card mount failed; retrying
  5 - failed to load config.txt; retrying

  Ongoing:
  - heartbeat between skulls every second to ensure they're still responding

  When ultrasonic sensor triggered (Primary Only):
  - Ignore if already playing sequence
  - Ignore if skit ended less than 5 seconds ago
  - Choose random skit to play, weighted to those least played. Never play same skit twice in a row.

  Skit Chosen
  - Primary sends START: which skit they're playing to Secondary with 2-second countdown
  - Secondary sends ACK
  - If Primary receives ACK within the 2 seconds, it starts playing when countdown ends, otherwise it sends another START request
  - Both skulls load audio and associated txt file, ready to play, in advance
  - When countdown ends they start executing the skit, playing the sections of audio assigned to them by the associated txt file
  - Each analyzes the audio they're playing in real-time, syncing their servo jaw motions to the audio


  Based off: Death_With_Skull_Synced_to_Audio

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

** TODO: UPDATE LIBRARIES, but beware, ESP32-A2DP have breaking changes 

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


Hardware is powered by 2 x USB-C cables.
- One connects directly to the ESP32. This powers the ESP32 itself (logic, bluetooth, etc.), the LEDs, and the SD card reader.
- The other powers the servo and the ultrasonic sensor, both of which require 5v.
- The dual power is necessary because the servo draws too much current for the ESP32 to supply and was crashing the SD card reader.

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

Servo: Power HD HD-1160A
- 4.8v-6v, stall torque: 3.0kg.com, max current: 0.8A, 0.12s/60deg speed, Pulse width Modulation
- speed = 0.12s/60deg = 500 deg/s max speed

Battery Packs
- Black: PocketJuice Endurance: 8000mAh output 5v/3.4A max
- Yellow: OC-H01(PD): 10000mAh output 5v/2A max


TROUBLESHOOTING:
- If it won't compile citing, library references, especially if you've just changed branches or did something large and disruptive,
  it may be a cachine issue with Arduino IDE. To clear the cache, compile for a different board (which will fail) and then switch
  back to the actual board and recompile.



ISSUES

  TODO
  ** staging: mount on sticks, make name signs, put electronics in bag under their fun hats, figure out where/how to hide speakers
  ** create name skit txt files for remaining skits
  ** stress test: run them for an hour
    ** test battery pack life
    ** test battery packs closing down if no power drawn often enough
  ** ISSUES
    ** battery pack: shuts off after a couple of seconds of zero current draw, soln: take 100mA of power every <2 seconds to keep it active
    ** yellow battery shuts down just after init. It's load is ultrasonic sensor + servo. Black battery works fine.
      - Black: PocketJuice Endurance: 8000mAh output 5v/3.4A max
      - Yellow: OC-H01(PD): 10000mAh output 5v/2A max
      - I'm guessing the simultaneous load is too much for the yellow pack. It must be exceeding 2A for the short duration of the ultrasonic sensor + servo load, but the black pack can handle it because it can handle 3.4A.

    ** does jaw animation lag sometimes? Does eye animation lag at the same time in the same way?



20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.
20240925: Full BLE communication working, Marco/Polo server connection, only attempts comms after Initialized wav played,
          and can play audio file in sync.
20240928: Animation (eyes only) synced to skit playback, ultrasonic sensor triggers random skit playback
20241005: Jaw animation works pretty well now.
