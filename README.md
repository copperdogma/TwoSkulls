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

  ** use the Audio Player Class for other functionality like FFT and SD support: https://github.com/pschatzmann/arduino-audio-tools/wiki/The-Audio-Player-Class
    - oooo shit... the examples are all A2DP (music sender or receiver). Technically I could try to set up primary as a sender TWICE (once for bluetooth audio, once for connecting to the secondary skull), and set up the secondary as a receiver, then abuse the metadata protocol to control the second skull... But I'm not even sure if it's possible to set up a single device as a dual sender, and I'm not sure I could abuse the protocols just right. Also it feels icky.

What communication method do you plan to use between the skulls? WiFi, Bluetooth, or another method?
Not sure. I'm already using bluetooth. Both skulls are acting as bluetooth senders, connecting to their own individual speakers to play audio. I'm not sure I could set the primary up as a second sender, or even if you can use the bluetooth chip to make multiple connections at once. I need help with this part for sure.

Will the secondary skull need to send any information back to the primary, or is this a one-way communication?
Yes? I'd assume we need some sort of ACK, and perhaps two-way comms to establish a common time to ensure our timing is correct.

Do you want to implement a simple command system (e.g., "play file X", "start now"), or do you need a more complex protocol?
Very simple. Basically establish a connection, primary sends message saying it's primary/secondary ACK, keepalive ping/secondary ACK, primary synchronizes time/secondary ACK, primary tells secondary what file to play starting at what time/secondary ACK

How precise does the synchronization need to be? Are we aiming for millisecond accuracy, or is a slight delay acceptable?
Within 100ms should be fine.

Will both skulls have identical audio files, or will the primary need to specify different files for each skull?
Both have identical files.

How do you want to handle potential communication failures or disconnections?
A primary keep-alive ping ACKed by secondary. If primary doesn't see an ACK for a bit it goes back to it's connection protocol. If secondary doesn't see a ping for a while it goes back to it's connection protocol.
  

  
  SD CARD
  - 20240707: Fixed initialization issue. Finally connected the power directly to the board's 3.3v pin to power it. Before I connected the 3.3 pin to the positive bar on the breadboard and it only worked 1/20 times. Bizarre. Took forever to debug.
  - The pins I am using are NOT what the docs say to use.. Not sure where I got them, but it works. But I also can't see in the setup where I told it to use those pins. But perhaps those are just the standard SPI communication pins for this board?
    - WHY are there NO pins set for the SD carD? Not in here, not in audio_player.cpp/.h... Am I just using the default pins expected by SD.h so we're good? At least doc that.
  - NEEDS SHIELDING from servo, which is electrically noisy. Otherwise it will fail to read within 10 seconds of audio playing. Ask me how I know;)

20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.
