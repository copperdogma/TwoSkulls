/*
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

Animation instruction file format
A=Primary skull, B=Secondary skull
- default to close on init, and close at end of every sequence assigned to it
- When indicating jaw position, "duration" refers to how long it should take to open/close skull to that position. A duration of 0 is "as fast as you can".
- listen = dynamically analyze audio and sync jaw servo to that audio, i.e.: try to match the sound
speaker,timestamp,duration,(jaw position)
A,30200,2000      - Primary (2s): I think you're just the worst person I've ever-
B,31500,1000      - Secondary (1s, cutting off Primary): I love you.
A,36000,300,.9    - Primary (300ms): opens jaw most of the way
A,37000,0,0       - Primary (0s): snaps jaw closed
B,47000,2000      - Secondary (2s): sorry, I was on the phone... you were saying?



  Based off: Death_With_Skull_Synced_to_Audio

SETUP:
Board: "ESP32 Wrover Module"
Port: "/dev/cu.usbserial-1430"
Get Board Info
Core Debug Level: "None"
Erase All Flash Before Sketch Upload: "Disabled"
Flash Frequency: "80MHz"
Flash Mode: "QIO"
Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
Upload Speed: "460800"

BOARD/LIBRARIES:
v2.0.17: esp32
v5.0.2 : ESP32 ESP32S2 AnalogWrite
v2.0.0 : HCSR04
v1.6.1 : arduinoFFT
v1.8.3 : ESP32-A2DP

** TODO: UPDATE LIBRARIES, but beware, ESP32-A2DP have breaking changes 


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
- 4.8v-6v, stall torque: 3.0kg.com, 0.12s/60deg speed, Pulse width Modulation
- speed = 0.12s/60deg = 500 deg/s max speed

ISSUES

  TODO
  ** Primary/Secondary setup: connect to different bluetooth speakers, play different init file... maybe make struct?
  ** use githib?
  
  ** start overall log to document these issues/components
  ** try ArduinoFFT as simple audio analysis we can use to sync skull jaw motion to audio
  ** make permanent mount for servo in skull
  ** why is the audio clicking before and after playing
  ** use the Audio Player Class for other functionality like FFT and SD support: https://github.com/pschatzmann/arduino-audio-tools/wiki/The-Audio-Player-Class
  ** reconnecting to bluetooth is flaky now that I'm relying on the library's auto-reconnect.. can I poke it?
  ** do a pre-pass on the audio to get max volume so we can peg that at max jaw movement. PER SKULL. Should be doable cuz we can do it on startup and save with the skit/skitline info. So it can be slowish.
  ** SD card flakiness: Did a test where I looped 20 times in a row, and if it doesn't read them all first try it does second or third try. Maybe a verified number and a retry till success?

  ** creashing fixes?
    ** Buffer Management: The buffer management in AudioPlayer::_provideAudioFrames could be improved. Consider using a circular buffer or a more efficient memory management strategy.  

  SD CARD
  - 20240707: Fixed initialization issue. Finally connected the power directly to the board's 3.3v pin to power it. Before I connected the 3.3 pin to the positive bar on the breadboard and it only worked 1/20 times. Bizarre. Took forever to debug.
  - The pins I am using are NOT what the docs say to use.. Not sure where I got them, but it works. But I also can't see in the setup where I told it to use those pins. But perhaps those are just the standard SPI communication pins for this board?
    - WHY are there NO pins set for the SD carD? Not in here, not in audio_player.cpp/.h... Am I just using the default pins expected by SD.h so we're good? At least doc that.

20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.

*/
#include <algorithm>  // For std::sort
#include <vector>
#include <tuple>
#include "bluetooth_audio.h"
#include "FS.h"
#include "SD.h"
#include <HCSR04.h>
#include "audio_player.h"
#include "light_controller.h"
#include "servo_controller.h"
#include "sd_card_manager.h"
#include "skull_audio_animator.h"
#include </Users/cam/Library/Arduino15/libraries/Servo/src/Servo.h>

const bool SKIT_DEBUG = true;  // Will set eyes to 100% brightness when it's supposed to be talking and 10% when it's not.

const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED

// Ultrasonic sensor
const int TRIGGER_PIN = 2;  // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;    // Pin number ultrasonic echo detection

// NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
UltraSonicDistanceSensor* distanceSensor = nullptr;

SDCardContent sdCardContent;

// Servo
const int SERVO_PIN = 15;  // Servo control pin
const int SERVO_MIN_DEGREES = 0;
const int SERVO_MAX_DEGREES = 80;  //Anything past this will grind the servo horn into the interior of the skull, probably breaking something.
ServoController servoController;

const unsigned long ULTRASONIC_READ_INTERVAL = 300;  // Ultrasonic read interval in ms
unsigned long lastUltrasonicRead = 0;                // Last time the ultrasonic sensor was read

// Exponential smoothing
struct AudioState {
    double smoothedPosition = 0;
    double maxObservedRMS = 0;
    bool isJawClosed = true;
    int chunkCounter = 0;
};

AudioState audioState;

const double ALPHA = 0.2;              // Smoothing factor; 0-1, lower=smoother
const double SILENCE_THRESHOLD = 200;  // Higher = vol needs to be louder to be considered "not silence", max 2000?
const int MIN_MOVEMENT_THRESHOLD = 3;  // Minimum degree change to move the servo
const double MOVE_EXPONENT = 0.2;      // 0-1, smaller = more movement
const double RMS_MAX = 32768.0;        // Maximum possible RMS value for 16-bit audio

const int CHUNKS_NEEDED = 17;  // Number of chunks needed for 100ms (34)

LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);

bool determinePrimaryRole() {
  distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, 400);  // 400cm max range
  delay(100);                                                                 // Give the sensor time to initialize

  const int NUM_READINGS = 10;
  int validReadings = 0;

  for (int i = 0; i < NUM_READINGS; i++) {
    float distance = distanceSensor->measureDistanceCm();
    Serial.println("Ultrasonic distance: " + String(distance) + "cm");
    if (distance >= 0) {  // Any non-negative reading is considered valid
      validReadings++;
    }
    delay(100);  // Wait between readings
  }

  if (validReadings > 0) {
    Serial.println("Ultrasonic sensor detected. This device is the Primary.");
    return true;
  } else {
    Serial.println("Ultrasonic sensor not detected. This device is the Secondary.");
    delete distanceSensor;
    distanceSensor = nullptr;
    return false;
  }
}

SDCardManager* sdCardManager = nullptr;

bool isPrimary = false;
const char* BLUETOOTH_SPEAKER_NAME = "JBL Flip 5";  // Replace with your actual speaker name
AudioPlayer* audioPlayer = nullptr;  // We'll initialize this later
bluetooth_audio bluetoothAudio;  // Declare the bluetoothAudio object

bool initializeBluetooth() {
  audioPlayer->setBluetoothConnected(false);
  audioPlayer->setAudioReadyToPlay(false);
  bluetoothAudio.begin(BLUETOOTH_SPEAKER_NAME, AudioPlayer::provideAudioFrames);
  bluetoothAudio.set_volume(100);
  return bluetoothAudio.is_connected();
}

SkullAudioAnimator* skullAnimator = nullptr;

void setup() {
  Serial.begin(115200);

  Serial.println("\n\n\n\n\n\nStarting setup ... ");

  lightController.begin();

  audioPlayer = new AudioPlayer(servoController);

  // Determine Primary/Secondary role
  isPrimary = determinePrimaryRole();
  if (isPrimary) {
    lightController.blinkEyes(4);  // Blink eyes 4 times for Primary
    
    // Initialize ultrasonic sensor only if Primary
    if (!distanceSensor) {
      distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
    }
  } else {
    lightController.blinkEyes(2);  // Blink eyes twice for Secondary
  }

  initializeBluetooth();

  // Initialize servo
  servoController.initialize(SERVO_PIN, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // Initialize SD Card Manager
  sdCardManager = new SDCardManager(audioPlayer);
  bool sdCardInitialized = sdCardManager->begin();

  while (!sdCardInitialized) {
    Serial.println("SD Card: Mount Failed! Retrying...");
    audioPlayer->setJawPosition(30);
    delay(500);
    sdCardInitialized = sdCardManager->begin();
    audioPlayer->setJawPosition(0);
    delay(500);
  }

  // Load SD card content
  sdCardContent = sdCardManager->loadContent();

  // Announce "System initialized" and role
  Serial.printf("Playing initialization audio\n");
  audioPlayer->playNext(isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav");

  // Find the "Skit - names" skit
  ParsedSkit namesSkit = audioPlayer->findSkitByName(sdCardContent.skits, "Skit - names");
  if (namesSkit.audioFile != "" && namesSkit.txtFile != "") {
    // Queue the "Skit - names" skit to play next
    Serial.println("'Skit - names' found; playing next.");
    audioPlayer->playSkitNext(namesSkit);
  } else {
    Serial.println("'Skit - names' not found.");
  }

  // Initialize SkullAudioAnimator after audioPlayer and servoController are set up
  skullAnimator = new SkullAudioAnimator(audioPlayer, servoController, isPrimary);
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastMillis = 0;

  // Every 1000ms output "loop() running"
  if (currentMillis - lastMillis >= 1000) {
    Serial.printf("%d loop() running\n", currentMillis);
    lastMillis = currentMillis;
  }

  // Update audio player and process any queued audio
  audioPlayer->update();

  // Update SkullAudioAnimator
  if (skullAnimator) {
    skullAnimator->update();
  }

  // Allow other tasks to run
  delay(1);
}

// Move audio processing to a separate function
void processAudio(const int16_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;

    double rms = 0;
    for (size_t i = 0; i < bufferSize; ++i) {
        rms += buffer[i] * buffer[i];
    }
    rms = sqrt(rms / bufferSize);

    audioState.maxObservedRMS = max(audioState.maxObservedRMS, rms);

    if (rms < SILENCE_THRESHOLD) {
        audioState.smoothedPosition = 0;
        audioState.isJawClosed = true;
    } else {
        double normalizedRMS = rms / RMS_MAX;
        double targetPosition = pow(normalizedRMS, MOVE_EXPONENT) * SERVO_MAX_DEGREES;
        audioState.smoothedPosition = ALPHA * targetPosition + (1 - ALPHA) * audioState.smoothedPosition;
        audioState.isJawClosed = false;
    }

    audioState.chunkCounter++;

    if (audioState.chunkCounter >= CHUNKS_NEEDED) {
        int newPosition = round(audioState.smoothedPosition);
        if (abs(newPosition - servoController.getCurrentPosition()) >= MIN_MOVEMENT_THRESHOLD) {
            servoController.setPosition(newPosition);
        }
        audioState.chunkCounter = 0;
    }
}

// Add this function to clean up resources
void cleanup() {
    delete distanceSensor;
    delete sdCardManager;
    delete skullAnimator;
    delete audioPlayer;
}