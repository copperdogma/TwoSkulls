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
  ** Primary/Secondary init: check for ultrasonic sensor. true=Primary, false=Secondary
  ** Primary/Secondary setup: connect to different bluetooth speakers, play different init file... maybe make struct?
  ** use githib?
  ** start overall log to document these issues/components
  ** try ArduinoFFT as simple audio analysis we can use to sync skull jaw motion to audio
  ** make permanent mount for servo in skull
  ** try using better servo library to control rotation rate for animation.. or see if default library can do it
  ** why is the audio clicking before and after playing
  ** use the Audio Player Class for other functionality like FFT and SD support: https://github.com/pschatzmann/arduino-audio-tools/wiki/The-Audio-Player-Class

  SD CARD
  - 20240707: Fixed initialization issue. Finally connected the power directly to the board's 3.3v pin to power it. Before I connected the 3.3 pin to the positive bar on the breadboard and it only worked 1/20 times. Bizarre. Took forever to debug.
  - The pins I am using are NOT what the docs say to use.. Not sure where I got them, but it works. But I also can't see in the setup where I told it to use those pins. But perhaps those are just the standard SPI communication pins for this board?
    - WHY are there NO pins set for the SD carD? Not in here, not in audio_player.cpp/.h... Am I just using the default pins expected by SD.h so we're good? At least doc that.

  ** wire this hardware up more permanently

20231022: Created.
20231023: Exposed buffer + buffer size so they could be read in loop() by audio analysis code.
20231023: Audio now analyzed in loop() and used to drive servo. It works, but it's just sort of spastic.
20231023: Changed servo init to just quickly open fully and then close.

*/
#include <algorithm>  // For std::sort
#include <vector>
#include <tuple>
#include "BluetoothA2DPSource.h"
#include "FS.h"
#include "SD.h"
#include <HCSR04.h>
#include "audio_player.h"

#include <arduinoFFT.h>
#include <Servo.h>

const bool SKIT_DEBUG = true;  // Will set eyes to 100% brightness when it's supposed to be talking and 10% when it's not.

const char* BLUETOOTH_SPEAKER_NAME = "JBL Flip 5";
// NOTE: To connect to a new speaker:
//    set a2dp_source.set_auto_reconnect(false);
//    The name of your speaker may NOT be what you expect. Check the output log on Debug to see what it says about devices it found.
//    It seems to keep a strong memory of the last speaker it connected to, so every time you want to connect to a new one
//    you seem to need to set auto_reconnect to false and manually pair it the first time.
//const char* BLUETOOTH_SPEAKER_NAME = "WH-CH710N";
//const char* BLUETOOTH_SPEAKER_NAME = "Cam’s AirPods3 - Find My"; // aka Cam Marsollier's AirPods

// ESP32 doesn't natively support A2DP so I'm using a third-party library: https://github.com/pschatzmann/ESP32-A2DP
BluetoothA2DPSource a2dp_source;

// The supported audio codec in ESP32 A2DP is SBC. SBC audio stream is encoded
// from PCM data normally formatted as 44.1kHz sampling rate, two-channel 16-bit sample data
// If audio is playing back at double speed, you've given it mono instead of stereo files.

int currentModeIndex = 0;  // Current audio file index

// Stores mode names and corresponding file lists
struct ModeFiles {
  String modeName;
  String modeTitleAudioFile;
  std::vector<String> files;
  int lastPlayedIndex = -1;
};
ModeFiles modeFiles[10];  // Assume we have at most 10 modes
int modeCount = 0;

bool isPrimary = false;

//CAMKILL:
//const char* AUDIO_INITIALIZED = "/audio/system_initialized.wav";

const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED

// LED PWD setup
const int PWM_CHANNEL_LEFT = 0;
const int PWM_CHANNEL_RIGHT = 1;
const int PWM_FREQUENCY = 5000;  // 5 kHz
const int PWM_RESOLUTION = 8;    // 8-bit resolution, 0-255
const int PWM_MAX = 255;         // Maximum PWM value

// Ultrasonic sensor
const int TRIGGER_PIN = 2;  // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;    // Pin number ultrasonic echo detection

// NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
//CAMKILL: death verison, now initialized only if exists (Primary): UltraSonicDistanceSensor distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
UltraSonicDistanceSensor* distanceSensor = nullptr;

struct Skit {
  String wavFile;
  String txtFile;
};

struct SDCardContent {
  std::vector<Skit> skits;
  String primaryInitAudio;
  String secondaryInitAudio;
};

SDCardContent sdCardContent;

struct SkitLine {
  char speaker;
  unsigned long timestamp;
  unsigned long duration;
  float jawPosition;
};

std::vector<SkitLine> currentSkit;
unsigned long skitStartTime = 0;
size_t currentSkitLine = 0;
bool isPlayingSkit = false;

struct AudioSegment {
  unsigned long start;
  unsigned long duration;
  bool shouldPlay;
};

std::vector<AudioSegment> audioSegments;
size_t currentSegment = 0;

// Audio analyzer
const int SAMPLES = 256;        // Must be a power of 2
const int SAMPLE_RATE = 44100;  // Default for audio library needs to be 44100
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLE_RATE);

// Servo
const int SERVO_PIN = 15;  // Pin number ultrasonic pulse trigger
const int SERVO_MIN_DEGREES = 0;
const int SERVO_MAX_DEGREES = 70;  //Anything past this will grind the servo horn into the interior of the skull, probably breaking something.
Servo jawServo = Servo();


unsigned long lastConnectionCheck = 0;                 // Last time the Bluetooth connection was checked
const unsigned long CONNECTION_CHECK_INTERVAL = 1000;  // Connection check interval in ms

unsigned long lastButtonCheck = 0;        // Last time the button was checked
const unsigned long DEBOUNCE_DELAY = 50;  // Debounce delay in ms
int lastButtonState = HIGH;               // Last state of the button
volatile bool isButtonPressed = false;    // Button press has happened, to be handled as soon as we can

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

// Function prototypes
double calculateRMS(const int16_t* samples, int numSamples);
int mapRMSToServoPosition(double rms);
void updateServoPosition(int targetPosition);

// const double EXAGGERATION_FACTOR = 10;
// int exaggeratePosition(int pos) {
//   if (pos < 35) {  // Mid-point of 0-70
//     return pos / EXAGGERATION_FACTOR;
//   } else {
//     return 35 + (pos - 35) * EXAGGERATION_FACTOR;
//   }
// }

const int CHUNKS_NEEDED = 17;  // Number of chunks needed for 100ms (34)
int chunkCounter = 0;          // Counter for accumulated chunks

std::vector<int16_t> rawSamples;
std::vector<double> fftMagnitudes;
std::vector<int> servoPositions;

void setEyeBrightness(int brightness) {
  ledcWrite(PWM_CHANNEL_LEFT, brightness);
  ledcWrite(PWM_CHANNEL_RIGHT, brightness);
}

void blinkEyes(int numBlinks) {
  for (int i = 0; i < numBlinks; i++) {
    digitalWrite(LEFT_EYE_PIN, HIGH);
    digitalWrite(RIGHT_EYE_PIN, HIGH);
    delay(200);  // On for 200ms
    digitalWrite(LEFT_EYE_PIN, LOW);
    digitalWrite(RIGHT_EYE_PIN, LOW);
    delay(200);  // Off for 200ms
  }
  // Ensure eyes are on after blinking
  digitalWrite(LEFT_EYE_PIN, HIGH);
  digitalWrite(RIGHT_EYE_PIN, HIGH);
}

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

void onBluetoothConnectionStateChanged(esp_a2d_connection_state_t state, void* obj) {
  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    Serial.printf("Lost connection to Bluetooth speaker '%s'.\n", BLUETOOTH_SPEAKER_NAME);
  } else if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    Serial.printf("Successfully connected to Bluetooth speaker '%s'.\n", BLUETOOTH_SPEAKER_NAME);
  }
}

std::vector<SkitLine> parseSkitFile(const String& txtFilePath) {
  std::vector<SkitLine> skit;
  File file = SD.open(txtFilePath);
  if (!file) {
    Serial.println("Failed to open skit file: " + txtFilePath);
    return skit;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    SkitLine skitLine;
    int commaIndex1 = line.indexOf(',');
    int commaIndex2 = line.indexOf(',', commaIndex1 + 1);
    int commaIndex3 = line.indexOf(',', commaIndex2 + 1);

    skitLine.speaker = line.charAt(0);
    skitLine.timestamp = line.substring(commaIndex1 + 1, commaIndex2).toInt();
    skitLine.duration = line.substring(commaIndex2 + 1, commaIndex3).toInt();

    if (commaIndex3 != -1) {
      skitLine.jawPosition = line.substring(commaIndex3 + 1).toFloat();
    } else {
      skitLine.jawPosition = -1;  // Indicating dynamic jaw movement
    }

    skit.push_back(skitLine);
  }

  file.close();
  return skit;
}

void playAudio(const String& wavFile, const String& txtFile = "") {
  if (txtFile.length() > 0 && SD.exists(txtFile)) {
    // This is a skit file
    currentSkit = parseSkitFile(txtFile);
    skitStartTime = millis();
    currentSkitLine = 0;
    isPlayingSkit = true;
  } else {
    // This is a regular audio file or skit without txt file
    currentSkit.clear();
    isPlayingSkit = false;
  }

  // Queue the audio file to play next
  audioPlayer->playNext(wavFile.c_str());
}

void playRandomSkit() {
  if (sdCardContent.skits.empty()) {
    Serial.println("No skits available to play.");
    return;
  }
  int randomIndex = random(sdCardContent.skits.size());
  const Skit& chosenSkit = sdCardContent.skits[randomIndex];
  playSkit(chosenSkit);
}

void playSkit(const Skit& skit) {
  currentSkit = parseSkitFile(skit.txtFile);
  skitStartTime = 0;  // Will be set when audio actually starts
  currentSkitLine = 0;
  isPlayingSkit = true;
  audioPlayer->playNext(skit.wavFile.c_str());
}

void updateSkitPlayback() {
    if (!isPlayingSkit || currentSkit.empty()) return;

    if (skitStartTime == 0 && audioPlayer->isCurrentlyPlaying()) {
        skitStartTime = millis();
    }

    if (skitStartTime == 0) return;  // Audio hasn't started yet

    unsigned long currentTime = millis() - skitStartTime;

    while (currentSkitLine < currentSkit.size()) {
        const SkitLine& line = currentSkit[currentSkitLine];

        if (currentTime < line.timestamp) {
            break;  // Not time for this line yet
        }

        if (currentTime >= line.timestamp && currentTime < line.timestamp + line.duration) {
            // It's time for this line
            bool isThisSkullSpeaking = (isPrimary && line.speaker == 'A') || (!isPrimary && line.speaker == 'B');
            
            if (SKIT_DEBUG) {
                setEyeBrightness(isThisSkullSpeaking ? 255 : 25);  // 100% or 10% brightness
            }

            if (isThisSkullSpeaking) {
                if (line.jawPosition >= 0) {
                    updateServoPosition(map(line.jawPosition * 100, 0, 100, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES));
                } else {
                    processAudioForJaw();
                }
            } else {
                updateServoPosition(SERVO_MIN_DEGREES);
            }
            return;  // Exit the function, we'll check the next line on the next call
        }

        // Move to next line
        currentSkitLine++;
    }

    // If we've reached here, the skit is over
    if (currentSkitLine >= currentSkit.size() && !audioPlayer->isCurrentlyPlaying()) {
        isPlayingSkit = false;
        updateServoPosition(SERVO_MIN_DEGREES);
        if (SKIT_DEBUG) {
            setEyeBrightness(255);  // Set eyes back to full brightness when skit is over
        }
    }
}

void processAudioForJaw() {
  uint8_t* audioBuffer = audioPlayer->getCurrentAudioBuffer();
  size_t audioBufferSize = audioPlayer->getCurrentAudioBufferSize();

  // Process audio in chunks of SAMPLES
  for (size_t offset = 0; offset < audioBufferSize; offset += SAMPLES * 2) {
    // Convert uint8_t buffer to int16_t samples (mono)
    int16_t samples[SAMPLES];
    for (int i = 0; i < SAMPLES && (offset + i * 2 + 1) < audioBufferSize; i++) {
      samples[i] = (audioBuffer[offset + i * 2 + 1] << 8) | audioBuffer[offset + i * 2];
    }

    // Calculate RMS and update servo position
    double rms = calculateRMS(samples, SAMPLES);
    int targetPosition = mapRMSToServoPosition(rms);
    updateServoPosition(targetPosition);
  }
}

// Use this function for non-skit audio files
void playNonSkitAudio(const String& wavFile) {
  playAudio(wavFile);
}

bool fileExists(fs::FS& fs, const char* path) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    return false;
  }
  file.close();
  return true;
}

SDCardContent initSDCard() {
  SDCardContent content;

  if (!SD.begin()) {
    Serial.println("SD Card: Mount Failed!");
    return content;
  }
  Serial.println("SD Card: Mounted successfully");

  // Check for initialization files
  if (fileExists(SD, "/audio/Initialized - Primary.wav")) {
    content.primaryInitAudio = "/audio/Initialized - Primary.wav";
  } else {
    Serial.println("SD Card: ERROR: Missing Primary initialization audio file");
  }

  if (fileExists(SD, "/audio/Initialized - Secondary.wav")) {
    content.secondaryInitAudio = "/audio/Initialized - Secondary.wav";
  } else {
    Serial.println("SD Card: ERROR: Missing Secondary initialization audio file");
  }

  // Process skit files
  File root = SD.open("/audio");
  if (!root || !root.isDirectory()) {
    Serial.println("SD Card: Failed to open /audio directory");
    return content;
  }

  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.startsWith("Skit") && fileName.endsWith(".wav")) {
      String baseName = fileName.substring(0, fileName.lastIndexOf('.'));
      String txtFileName = baseName + ".txt";
      if (fileExists(SD, ("/audio/" + txtFileName).c_str())) {
        content.skits.push_back({ "/audio/" + fileName, "/audio/" + txtFileName });
      } else {
        Serial.println("SD Card: WARNING: Missing txt file for " + fileName);
      }
    }
    file = root.openNextFile();
  }

  return content;
}

// Function to get a random skit
Skit getRandomSkit(const std::vector<Skit>& skits) {
  if (skits.empty()) {
    return { "", "" };  // Return empty skit if no skits available
  }
  int randomIndex = random(skits.size());
  return skits[randomIndex];
}

Skit findSkitByName(const std::vector<Skit>& skits, const String& name) {
  for (const auto& skit : skits) {
    if (skit.wavFile.endsWith(name + ".wav")) {
      return skit;
    }
  }
  return { "", "" };  // Return empty skit if not found
}

// Function to get initialization audio path
String getInitAudio(const SDCardContent& content, bool isPrimary) {
  return isPrimary ? content.primaryInitAudio : content.secondaryInitAudio;
}

void prepareSkitPlayback(const Skit& skit) {
  currentSkit = parseSkitFile(skit.txtFile);
  audioSegments.clear();

  for (const auto& line : currentSkit) {
    bool shouldPlay = (isPrimary && line.speaker == 'A') || (!isPrimary && line.speaker == 'B');
    audioSegments.push_back({ line.timestamp, line.duration, shouldPlay });
  }

  // Sort segments by start time
  std::sort(audioSegments.begin(), audioSegments.end(),
            [](const AudioSegment& a, const AudioSegment& b) {
              return a.start < b.start;
            });

  skitStartTime = 0;
  currentSegment = 0;
}

double calculateRMS(const int16_t* samples, int numSamples) {
  double sum = 0;
  for (int i = 0; i < numSamples; i++) {
    sum += samples[i] * samples[i];
  }
  return sqrt(sum / numSamples);
}

int mapRMSToServoPosition(double rms) {
  // Check for silence or very low volume
  if (rms < SILENCE_THRESHOLD) {
    return SERVO_MIN_DEGREES;  // Close the jaw completely
  }

  // Update max observed RMS
  if (rms > maxObservedRMS) {
    maxObservedRMS = rms;
  }

  // Normalize RMS to 0-1 range, using the max observed RMS
  double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);

  // Apply non-linear mapping for more pronounced mouth movement
  double mappedValue = pow(normalizedRMS, MOVE_EXPONENT);

  // Increase the minimum jaw opening
  int minJawOpening = SERVO_MIN_DEGREES + 5;  // 5 degrees minimum opening

  return map(mappedValue * 1000, 0, 1000, minJawOpening, SERVO_MAX_DEGREES);
}

void updateServoPosition(int targetPosition) {
  // Apply exponential smoothing
  smoothedPosition = ALPHA * targetPosition + (1 - ALPHA) * smoothedPosition;

  // Round and constrain the position
  int newPosition = round(smoothedPosition);
  newPosition = constrain(newPosition, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // Only move the servo if the change is significant
  static int lastServoPosition = SERVO_MIN_DEGREES;
  if (abs(newPosition - lastServoPosition) > MIN_MOVEMENT_THRESHOLD) {
    jawServo.write(SERVO_PIN, newPosition);
    lastServoPosition = newPosition;
  }
}

AudioPlayer* audioPlayer = new AudioPlayer();

void setup() {
  Serial.begin(115200);

  Serial.println("\n\n\n\n\n\nStarting setup ... ");

  // LED standard on/off setup
  pinMode(LEFT_EYE_PIN, OUTPUT);
  pinMode(RIGHT_EYE_PIN, OUTPUT);

  // Turn eyes on
  digitalWrite(LEFT_EYE_PIN, HIGH);
  digitalWrite(RIGHT_EYE_PIN, HIGH);

  // Determine Primary/Secondary role
  isPrimary = determinePrimaryRole();
  if (isPrimary) {
    blinkEyes(3);  // Blink eyes 3 times for Primary
  } else {
    blinkEyes(1);  // Blink eyes once for Secondary
  }

  // LED eyes

  // LED PWM setup
  if (SKIT_DEBUG) {
    ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(LEFT_EYE_PIN, PWM_CHANNEL_LEFT);
    ledcAttachPin(RIGHT_EYE_PIN, PWM_CHANNEL_RIGHT);
  }

  // Initialize ultrasonic sensor only if Primary
  if (isPrimary && distanceSensor == nullptr) {
    blinkEyes(3);  // Blink eyes 3 times for Primary
    distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
  } else {
    blinkEyes(1);  // Blink eyes once for Secondary
  }

  // Bluetooth
  a2dp_source.set_auto_reconnect(true);
  a2dp_source.set_on_connection_state_changed(onBluetoothConnectionStateChanged);
  a2dp_source.start(BLUETOOTH_SPEAKER_NAME, AudioPlayer::provideAudioFrames);  // Use the static method as the callback

  Serial.printf("setup a2dp_source.is_connected(): %d\n", a2dp_source.is_connected());

  a2dp_source.set_volume(100);


  // Initialize servo
  jawServo.attach(SERVO_PIN);

  Serial.println("Initializing servo - basic");
  Serial.printf("Servo animation init: %d degrees\n", SERVO_MIN_DEGREES);  // 0
  jawServo.write(SERVO_PIN, SERVO_MIN_DEGREES);
  Serial.printf("Servo animation init: %d degrees\n", SERVO_MAX_DEGREES);  // 70
  delay(500);
  jawServo.write(SERVO_PIN, SERVO_MAX_DEGREES);
  Serial.println("Servo animation init complete; resetting to 0 degrees");
  delay(500);
  jawServo.write(SERVO_PIN, SERVO_MIN_DEGREES);

  // SD Card initialization
  bool sdCardInitialized;
  sdCardInitialized = SD.begin();

  while (!sdCardInitialized) {
    Serial.println("SD Card: Mount Failed! Retrying...");
    jawServo.write(SERVO_PIN, 30);
    delay(500);
    sdCardInitialized = SD.begin();
    jawServo.write(SERVO_PIN, 0);
    delay(500);
  }

  // Initialize SD card and load content
  sdCardContent = initSDCard();

  // Initialize skit playback
  currentSkit.clear();
  skitStartTime = 0;
  currentSkitLine = 0;
  isPlayingSkit = false;

  // Announce "System initialized" and role
  Serial.printf("Playing initialization audio\n");
  audioPlayer->playNext(isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav");

  // TEMP/TESTING: Find the "Skit - names" skit
  Skit namesSkit = findSkitByName(sdCardContent.skits, "Skit - names");
  if (namesSkit.wavFile != "" && namesSkit.txtFile != "") {
    // Queue the "Skit - names" skit to play next
    playAudio(namesSkit.wavFile, namesSkit.txtFile);
  } else {
    Serial.println("'Skit - names' not found.");
  }
}


void loop() {
  unsigned long currentMillis = millis();

  // Blink eyes
  // digitalWrite(LEFT_EYE_PIN, HIGH);
  // digitalWrite(RIGHT_EYE_PIN, HIGH);
  // delay(1000);  // On for 1 second
  // digitalWrite(LEFT_EYE_PIN, LOW);
  // digitalWrite(RIGHT_EYE_PIN, LOW);
  // delay(1000);  // Off for 1 second

  // // NOTE: dim/brighten code works but it kills the audio sync, probably by disrupting the loop.. we'd need to hook into the currentMills system which is non-blocking
  // // Dim/brighten eyes
  // static int brightness = 0;
  // static int fadeAmount = 5;

  // // Set the brightness of both LEDs
  // ledcWrite(0, brightness);
  // ledcWrite(1, brightness);

  // // Change the brightness for next time through the loop
  // brightness = brightness + fadeAmount;

  // // Reverse the direction of the fading at the ends of the fade
  // if (brightness <= 0 || brightness >= PWM_MAX) {
  //   fadeAmount = -fadeAmount;
  // }

  // // Small delay to see the dimming effect
  // delay(30);

  // Check Bluetooth connection state every second
  if (currentMillis - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    lastConnectionCheck = currentMillis;
    if (!a2dp_source.is_connected()) {
      Serial.printf("Trying to connect to Bluetooth speaker '%s' ...\n", BLUETOOTH_SPEAKER_NAME);
    }
  }


  //TODO: put back after modifying, and this is somethign only the Primary should do
  // if (currentMillis - lastUltrasonicRead >= ULTRASONIC_READ_INTERVAL) {
  //   lastUltrasonicRead = currentMillis;
  //   float distance = distanceSensor->measureDistanceCm();
  //   //Serial.println("Ultrasonic distance: " + String(distance) + "cm");
  //   // If something is within range and there isn't audio already playing, trigger a random audio file from the current mode.
  //   if (distance > 0 && !audioPlayer->isCurrentlyPlaying()) {
  //     playRandomAudioFileFromCurrentMode();
  //   }
  // }

  // Play any queued audio and update skit playback
  audioPlayer->update();
  updateSkitPlayback();

  if (!isPlayingSkit && SKIT_DEBUG) {
    setEyeBrightness(255);  // Set eyes to full brightness when not in a skit
  }

  // Check if audio is playing
  if (!audioPlayer->isCurrentlyPlaying()) {
    // If no audio is playing and jaw isn't closed, close it
    if (!isJawClosed) {
      updateServoPosition(SERVO_MIN_DEGREES);
      isJawClosed = true;
    }
  } else {
    // Audio is playing, process it
    isJawClosed = false;  // Reset the flag as we're now processing audio

    uint8_t* audioBuffer = audioPlayer->getCurrentAudioBuffer();
    size_t audioBufferSize = audioPlayer->getCurrentAudioBufferSize();

    // Process audio in chunks of SAMPLES
    for (size_t offset = 0; offset < audioBufferSize; offset += SAMPLES * 2) {
      // Convert uint8_t buffer to int16_t samples (mono)
      std::vector<int16_t> samples(SAMPLES);
      for (int i = 0; i < SAMPLES && (offset + i * 2 + 1) < audioBufferSize; i++) {
        samples[i] = (audioBuffer[offset + i * 2 + 1] << 8) | audioBuffer[offset + i * 2];
      }

      // Calculate RMS of the audio chunk
      double rms = calculateRMS(samples.data(), samples.size());

      // Map RMS to servo position
      if (isPlayingSkit) {
        // For skit playback
        if (!currentSkit.empty() && currentSkitLine < currentSkit.size()) {
          const SkitLine& line = currentSkit[currentSkitLine];
          if ((isPrimary && line.speaker == 'A') || (!isPrimary && line.speaker == 'B')) {
            if (line.jawPosition < 0) {  // Dynamic jaw movement
              int targetPosition = mapRMSToServoPosition(rms);
              updateServoPosition(targetPosition);
            }
          }
        }
      } else {
        // For non-skit audio, always animate
        int targetPosition = mapRMSToServoPosition(rms);
        updateServoPosition(targetPosition);
      }
    }
  }

  // Allow other tasks to run
  delay(1);
}