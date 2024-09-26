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


TROUBLESHOOTING:
- If it won't compile citing, library references, especially if you've just changed branches or did something large and disruptive,
  it may be a cachine issue with Arduino IDE. To clear the cache, compile for a different board (which will fail) and then switch
  back to the actual board and recompile.



ISSUES

  TODO
  ** start overall log to document these issues/components
  ** connect to second skull; maybe use existing bluetooth library which has examples: https://github.com/pschatzmann/ESP32-A2DP
  ** try ArduinoFFT as simple audio analysis we can use to sync skull jaw motion to audi.o
  ** do a pre-pass on the audio to get max volume so we can peg that at max jaw movement. PER SKULL. Should be doable cuz we can do it on startup and save with the skit/skitline info. So it can be slowish.
  ** fix primary skull's servo seat; carve out jawbone a bit more so it catches better
  ** external 5v, 1A power needed for servo
    - original issue was it would crash when servo would run, because it was drawing too many amps (.8 at stall whereas esp32 supplied max .4?), which dropped the voltage enough to crash SD reader
    - wall wart + usb cable: I really don't want to have to plug in the skulls
    - battery pack: ISSUE: shuts off after a couple of seconds of zero current draw, soln: take 100mA of power every <2 seconds to keep it active
    - power board, providing power to both the ESP32 board and the servo: I think I only have one
  ** rebuild secondary perfboard with dupont connectors
  ** staging: mount on sticks, make name signs, figure out where to put electronics + batteries, figure out where/how to hide speakers
  ** why does skull_communication have access to skullAudioAnimator? That seems like the wrong inversion.
      Perhaps skull_communication should have a callback, coordinated by skull_audio_animator or the INO
      file, which sets skull_communication->Enabled(true/false) (for when it's playing audio), and
      the callback should be for when the primary gets PLAY_FILE_ACK and should start playing the file.
      - REFACTORING
        - need to give skull_audio_animator AP.isPlaying (better to raise events on start/end with filename), AP.getPlaybackTime(),
          AP.playingAudioFrames(audioFrames BT just grabbed)
        - Use existing ESP32 audio libraries/SD reader libraries so I'm not reinventing the wheel: https://chatgpt.com/c/66e9d09e-68c4-800a-a1ce-618cef69b694
  ** OPTIMIZATION:
    - audio sync USUALLY 95%+ but...
      - what if secondary is playing audio when it gets the call to play a different file? it SHOULD just ignore it,
          and maybe primary should ignore the lack of ACK as well.. just next time it'll see if everything is free. Also ensure
          we have access to the audio file before ACKing? Any other verifications we need before we're 100% ready?
        - do we maybe need a better sync? Like readyfile(file) then playFile(file). readyFile() has primary ensure it can read the
          file and Secondary doesn't ack until the same. Then they're both ready for playFile().
  ** NEXT:
    ** finish refactoring of proper architecture (see paper scrap)
      - pull frames into INO via callback and feed to skull_audio_animator
      - refactor skull_audio_animator.. it's a mess
    ** audio sync code for playing the same file on both (with prep/ack/deny/etc)
    ** kill radioManger (it's integrated in a bunch of places still)
      ** remove radioManager from audioPlayer
    ** future: ultrasonic distance needs to be done from multiple averages shots because the sensors results are noisy


20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.
29240925: Full BLE communication working, Marco/Polo server connection, only attempts comms after Initialized wav played,
          and can play audio file in sync.
