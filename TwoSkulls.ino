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

const bool SKIT_DEBUG = true;  // Will set eyes to 100% brightness when it's supposed to be talking and 10% when it's not.

const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED

// Ultrasonic sensor
const int TRIGGER_PIN = 2;  // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;    // Pin number ultrasonic echo detection

// NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
UltraSonicDistanceSensor* distanceSensor = nullptr;

struct SDCardContent {
  std::vector<ParsedSkit> skits;
  String primaryInitAudio;
  String secondaryInitAudio;
};

SDCardContent sdCardContent;

// Servo
const int SERVO_PIN = 15;  // Servo control pin
const int SERVO_MIN_DEGREES = 0;
const int SERVO_MAX_DEGREES = 80;  //Anything past this will grind the servo horn into the interior of the skull, probably breaking something.
ServoController servoController;

const unsigned long ULTRASONIC_READ_INTERVAL = 300;  // Ultrasonic read interval in ms
unsigned long lastUltrasonicRead = 0;                // Last time the ultrasonic sensor was read

// Exponential smoothing
double smoothedPosition = 0;
double maxObservedRMS = 0;
bool isJawClosed = false;
const double ALPHA = 0.2;              // Smoothing factor; 0-1, lower=smoother
const double SILENCE_THRESHOLD = 200;  // Higher = vol needs to be louder to be considered "not silence", max 2000?
const int MIN_MOVEMENT_THRESHOLD = 3;  // Minimum degree change to move the servo
const double MOVE_EXPONENT = 0.2;      // 0-1, smaller = more movement
const double RMS_MAX = 32768.0;        // Maximum possible RMS value for 16-bit audio

const int CHUNKS_NEEDED = 17;  // Number of chunks needed for 100ms (34)
int chunkCounter = 0;          // Counter for accumulated chunks

std::vector<int16_t> rawSamples;
std::vector<double> fftMagnitudes;
std::vector<int> servoPositions;

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

SDCardContent initSDCard() {
  SDCardContent content;

  if (!SD.begin()) {
    Serial.println("SD Card: Mount Failed!");
    return content;
  }
  Serial.println("SD Card: Mounted successfully");

  // Check for initialization files
  Serial.println("Required file '/audio/Initialized - Primary.wav' " + 
    String(audioPlayer->fileExists(SD, "/audio/Initialized - Primary.wav") ? "found." : "missing."));
  Serial.println("Required file '/audio/Initialized - Secondary.wav' " + 
    String(audioPlayer->fileExists(SD, "/audio/Initialized - Secondary.wav") ? "found." : "missing."));

  if (audioPlayer->fileExists(SD, "/audio/Initialized - Primary.wav")) {
    content.primaryInitAudio = "/audio/Initialized - Primary.wav";
  }
  if (audioPlayer->fileExists(SD, "/audio/Initialized - Secondary.wav")) {
    content.secondaryInitAudio = "/audio/Initialized - Secondary.wav";
  }

  // Process skit files
  File root = SD.open("/audio");
  if (!root || !root.isDirectory()) {
    Serial.println("SD Card: Failed to open /audio directory");
    return content;
  }

  std::vector<String> skitFiles;
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.startsWith("Skit") && fileName.endsWith(".wav")) {
      skitFiles.push_back(fileName);
    }
    file = root.openNextFile();
  }

  Serial.println("Processing " + String(skitFiles.size()) + " skits:");
  for (const auto& fileName : skitFiles) {
    String baseName = fileName.substring(0, fileName.lastIndexOf('.'));
    String txtFileName = baseName + ".txt";
    if (audioPlayer->fileExists(SD, ("/audio/" + txtFileName).c_str())) {
      ParsedSkit parsedSkit = audioPlayer->parseSkitFile("/audio/" + fileName, "/audio/" + txtFileName);
      content.skits.push_back(parsedSkit);
      Serial.println("- Processing skit '" + fileName + "' - success. (" + String(parsedSkit.lines.size()) + " lines)");
    } else {
      Serial.println("- Processing skit '" + fileName + "' - WARNING: missing txt file.");
    }
  }

  return content;
}

AudioPlayer* audioPlayer = new AudioPlayer(servoController);
bluetooth_audio bluetoothAudio;

// Add these lines near the top of the file, with other global declarations
bool isPrimary = false;

// Add this near the top of the file with other constants
const char* BLUETOOTH_SPEAKER_NAME = "JBL Flip 5";

bool initializeBluetooth() {
  audioPlayer->setBluetoothConnected(false);
  audioPlayer->setAudioReadyToPlay(false);
  bluetoothAudio.begin(BLUETOOTH_SPEAKER_NAME, AudioPlayer::provideAudioFrames);
  bluetoothAudio.set_volume(100);
  return bluetoothAudio.is_connected();
}

void setup() {
  Serial.begin(115200);

  Serial.println("\n\n\n\n\n\nStarting setup ... ");

  lightController.begin();

  // Determine Primary/Secondary role
  isPrimary = determinePrimaryRole();
  if (isPrimary) {
    lightController.blinkEyes(4);  // Blink eyes 4 times for Primary
  } else {
    lightController.blinkEyes(2);  // Blink eyes twice for Secondary
  }

  // Initialize ultrasonic sensor only if Primary
  if (isPrimary && distanceSensor == nullptr) {
    distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
  }

  initializeBluetooth();

  // Initialize servo
  servoController.initialize(SERVO_PIN, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // SD Card initialization
  bool sdCardInitialized;
  sdCardInitialized = SD.begin();

  while (!sdCardInitialized) {
    Serial.println("SD Card: Mount Failed! Retrying...");
    audioPlayer->setJawPosition(30);
    delay(500);
    sdCardInitialized = SD.begin();
    audioPlayer->setJawPosition(0);
    delay(500);
  }

  // Initialize SD card and load content
  sdCardContent = initSDCard();

  // Announce "System initialized" and role
  Serial.printf("Playing initialization audio\n");
  audioPlayer->playNext(isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav");

  // Find the "Skit - names" skit
  ParsedSkit namesSkit = audioPlayer->findSkitByName(sdCardContent.skits, "Skit - names");
  if (namesSkit.audioFile != "" && namesSkit.txtFile != "") {
    // Queue the "Skit - names" skit to play next
    audioPlayer->playSkitNext(namesSkit);
  } else {
    Serial.println("'Skit - names' not found.");
  }
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

  // Process audio and update jaw position
  if (audioPlayer->isCurrentlyPlaying()) {
    uint8_t* audioBuffer = audioPlayer->getCurrentAudioBuffer();
    size_t audioBufferSize = audioPlayer->getCurrentAudioBufferSize();

    // Process audio in chunks of SAMPLES
    for (size_t offset = 0; offset < audioBufferSize; offset += AudioPlayer::SAMPLES * 2) {
      std::vector<int16_t> samples(AudioPlayer::SAMPLES);
      for (int i = 0; i < AudioPlayer::SAMPLES && (offset + i * 2 + 1) < audioBufferSize; i++) {
        samples[i] = (audioBuffer[offset + i * 2 + 1] << 8) | audioBuffer[offset + i * 2];
      }

      double rms = audioPlayer->calculateRMS(samples.data(), samples.size());

      // Perform FFT if needed
      audioPlayer->performFFT();
      // You can now get FFT results using audioPlayer->getFFTResult(index)

      if (audioPlayer->isPlayingSkit()) {
        const ParsedSkit& currentSkit = audioPlayer->getCurrentSkit();
        if (!currentSkit.lines.empty() && audioPlayer->getCurrentSkitLine() < currentSkit.lines.size()) {
          const ParsedSkitLine& line = currentSkit.lines[audioPlayer->getCurrentSkitLine()];
          bool isThisSkullSpeaking = (isPrimary && line.speaker == 'A') || (!isPrimary && line.speaker == 'B');
          if (isThisSkullSpeaking && line.jawPosition < 0) {  // Dynamic jaw movement
            int targetPosition = servoController.mapRMSToPosition(rms, SILENCE_THRESHOLD);
            servoController.updatePosition(targetPosition, ALPHA, MIN_MOVEMENT_THRESHOLD);
          } else if (isThisSkullSpeaking && line.jawPosition >= 0) {
            int targetPosition = map(line.jawPosition * 100, 0, 100, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);
            servoController.setPosition(targetPosition);
          } else {
            servoController.setPosition(SERVO_MIN_DEGREES);
          }
        }
      } else {
        // For non-skit audio, always animate
        int targetPosition = servoController.mapRMSToPosition(rms, SILENCE_THRESHOLD);
        servoController.updatePosition(targetPosition, ALPHA, MIN_MOVEMENT_THRESHOLD);
      }
    }
  } else {
    // If no audio is playing, ensure jaw is closed
    servoController.setPosition(SERVO_MIN_DEGREES);
  }

  // Allow other tasks to run
  delay(1);
}