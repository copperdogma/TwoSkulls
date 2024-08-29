/*
  Two Chatting Skulls with Skull Jaws Synced to Audio

  On startup:
  - decides it's Master if it has the ultrasonic sensor attached, otherwise it decides it's the Slave
  - each skull will play their individual (Master/Slave) "initialized" audio so we know they've found their role properly and that they've connected to their individual bluetooth speaker
  - Master: plays "Marco" every 5 seconds until connnected to Slave
  - Slave: plays "Polo" every 5 seconds until conencted to Master
  - Once both connected, they'll play skit: "Morning, Jeff." "Morning, Tony."
  - Then Master starts monitoring ultrasonic sensor.

  Ongoing:
  - heartbeat between skulls every second to ensure they're still responding

  When ultrasonic sensor triggered (Master Only):
  - Ignore if already playing sequence
  - Ignore if skit ended less than 5 seconds ago
  - Choose random skit to play, weighted to those least played. Never play same skit twice in a row.

  Skit Chosen
  - Master sends START: which skit they're playing to Slave with 2-second countdown
  - Slave sends ACK
  - If Master receives ACK within the 2 seconds, it starts playing when countdown ends, otherwise it sends another START request
  - Both skulls load audio and associated txt file, ready to play, in advance
  - When countdown ends they start executing the skit, playing the sections of audio assigned to them by the associated txt file
  - Each analyzes the audio they're playing in real-time, syncing their servo jaw motions to the audio

Animation instruction file format
M=master, S=slave
- default to close on init, and close at end of every sequence assigned to it
- When indicating jaw position, "duration" refers to how long it should take to open/close skull to that position. A duration of 0 is "as fast as you can".
- listen = dynamically analyze audio and sync jaw servo to that audio, i.e.: try to match the sound
speaker,timestamp,duration,(jaw position)
M,30200,2000      - Master (2s): I think you're just the worst person I've ever-
S,31500,1000      - Slave (1s, cutting off Master): I love you.
M,36000,300,.9    - Master (300ms): opens jaw most of the way
M,37000,0,0       - Master (0s): snaps jaw closed
S,47000,2000      - Slave (2s): sorry, I was on the phone... you were saying?



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
  ** master/slave init: check for ultrasonic sensor. true=master, false=slave
  ** master/slave setup: connect to different bluetooth speakers, play different init file... maybe make struct?
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

bool isMaster = false;

//CAMKILL:
//const char* AUDIO_INITIALIZED = "/audio/system_initialized.wav";

const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED

// LED PWD setup
const int PWM_FREQUENCY = 5000;  // 5 kHz
const int PWM_RESOLUTION = 8;    // 8-bit resolution, 0-255
const int PWM_MAX = 255;         // Maximum PWM value

// Ultrasonic sensor
const int TRIGGER_PIN = 2;  // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;    // Pin number ultrasonic echo detection

// NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
//CAMKILL: death verison, now initialized only if exists (master): UltraSonicDistanceSensor distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
UltraSonicDistanceSensor* distanceSensor = nullptr;

struct Skit {
  String wavFile;
  String txtFile;
};

struct SDCardContent {
  std::vector<Skit> skits;
  String masterInitAudio;
  String slaveInitAudio;
};

// Audio analyzer
const int SAMPLES = 128;        // Must be a power of 2
const int SAMPLE_RATE = 44100;  // Default for audio library needs to be 44100
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLE_RATE);

// Servo
const int SERVO_PIN = 15;  // Pin number ultrasonic pulse trigger
const int SERVO_MIN_DEGREES = 0;
const int SERVO_MAX_DEGREES = 70;  //Anything past this will grind the servo horn into the interior of the skull, probably breaking something.
Servo jawServo = Servo();

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

bool determineMasterRole() {
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
    Serial.println("Ultrasonic sensor detected. This device is the Master.");
    return true;
  } else {
    Serial.println("Ultrasonic sensor not detected. This device is the Slave.");
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

//CAMKILL: death version
// void initSDCard() {
//   File root = SD.open("/audio");
//   std::vector<String> folderNames;

//   // Collect all relevant folder names
//   File folder = root.openNextFile();
//   while (folder) {
//     String folderName = folder.name();
//     if (folder.isDirectory() && folderName.startsWith("mode")) {
//       folderNames.push_back(folderName);
//     }
//     folder = root.openNextFile();
//   }

//   // Sort the folder names
//   std::sort(folderNames.begin(), folderNames.end());

//   // Process each folder in sorted order
//   for (const String& folderName : folderNames) {
//     File folder = SD.open("/audio/" + folderName);
//     if (folder) {
//       processModeFolder(folder);
//     }
//   }

//   if (modeCount == 0) {
//     Serial.println("SD Card: ERROR: No mode* folders found on SD card.");
//   }
// }

// void processModeFolder(fs::File& folder) {
//   Serial.printf("SD Card: Processing folder %s ---\n", folder.name());
//   std::vector<String> fileList;
//   bool hasTitle = false;
//   fs::File file = folder.openNextFile();
//   while (file) {
//     Serial.printf("   SD Card: Processing file %s\n", file.name());
//     if (String(file.name()).endsWith("title.wav")) {
//       hasTitle = true;
//     } else {
//       fileList.push_back("/audio/" + String(folder.name()) + "/" + String(file.name()));
//     }
//     file = folder.openNextFile();
//   }

//   if (!hasTitle) {
//     Serial.printf("   SD Card: Error: Folder %s is missing title.wav. Ignoring folder.\n", folder.name());
//     return;
//   } else {
//     Serial.printf("   SD Card: OK: Folder %s contains title.wav. Saving folder data with %i audio files.\n", folder.name(), fileList.size());
//   }

//   // Save the folder and its files in the next available slot in the array
//   modeFiles[modeCount].modeName = folder.name();
//   modeFiles[modeCount].modeTitleAudioFile = "/audio/" + String(folder.name()) + "/title.wav";
//   modeFiles[modeCount].files = fileList;
//   modeCount++;
// }

// String getModeAudioPath(int modeIndex) {
//   if (modeIndex < modeCount && !modeFiles[modeIndex].modeName.isEmpty()) {
//     return modeFiles[modeIndex].modeTitleAudioFile;
//   } else {
//     Serial.println("getModeAudioPath: ERROR: Invalid mode index or empty mode name.");
//     return String("");  // Return an empty string if invalid
//   }
// }

// void playRandomAudioFileFromCurrentMode() {
//   if (!modeFiles[currentModeIndex].files.empty()) {
//     int randomIndex;
//     do {
//       randomIndex = random(0, modeFiles[currentModeIndex].files.size());
//     } while (randomIndex == modeFiles[currentModeIndex].lastPlayedIndex && modeFiles[currentModeIndex].files.size() > 1);

//     modeFiles[currentModeIndex].lastPlayedIndex = randomIndex;  // Update last played index
//     String randomModeAudioFile = modeFiles[currentModeIndex].files[randomIndex];
//     audioPlayer->playNow(randomModeAudioFile.c_str());
//   } else {
//     Serial.println("No audio files in current mode.");
//   }
// }

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
  if (fileExists(SD, "/audio/Initialized - Master.wav")) {
    content.masterInitAudio = "/audio/Initialized - Master.wav";
  } else {
    Serial.println("SD Card: ERROR: Missing Master initialization audio file");
  }

  if (fileExists(SD, "/audio/Initialized - Slave.wav")) {
    content.slaveInitAudio = "/audio/Initialized - Slave.wav";
  } else {
    Serial.println("SD Card: ERROR: Missing Slave initialization audio file");
  }

  if (content.masterInitAudio.isEmpty() || content.slaveInitAudio.isEmpty()) {
    Serial.println("SD Card: ERROR: Missing initialization audio file(s)");
    return content;
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

  if (content.skits.empty()) {
    Serial.println("SD Card: ERROR: No valid skit files found");
  } else {
    Serial.println("SD Card: Found " + String(content.skits.size()) + " valid skits");
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

// Function to get initialization audio path
String getInitAudio(const SDCardContent& content, bool isMaster) {
  return isMaster ? content.masterInitAudio : content.slaveInitAudio;
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

  // Determine Master/Slave role
  isMaster = determineMasterRole();
  if (isMaster) {
    blinkEyes(3);  // Blink eyes 3 times for Master
  } else {
    blinkEyes(1);  // Blink eyes once for Slave
  }

  // LED eyes

  // LED PWD setup
  // ledcSetup(0, PWM_FREQUENCY, PWM_RESOLUTION); // Channel 0 for left eye
  // ledcSetup(1, PWM_FREQUENCY, PWM_RESOLUTION); // Channel 1 for right eye
  // ledcAttachPin(LEFT_EYE_PIN, 0);
  // ledcAttachPin(RIGHT_EYE_PIN, 1);

  // Initialize ultrasonic sensor only if Master
  if (isMaster && distanceSensor == nullptr) {
    blinkEyes(3);  // Blink eyes 3 times for Master
    distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
  } else {
    blinkEyes(1);  // Blink eyes once for Slave
  }

  // Bluetooth
  a2dp_source.set_auto_reconnect(true);
  a2dp_source.set_on_connection_state_changed(onBluetoothConnectionStateChanged);
  a2dp_source.start(BLUETOOTH_SPEAKER_NAME, AudioPlayer::provideAudioFrames);  // Use the static method as the callback

  Serial.printf("setup a2dp_source.is_connected(): %d\n", a2dp_source.is_connected());

  a2dp_source.set_volume(100);


  // StartPos = 0
  // MaxEndPos   = 70
  Serial.println("Initializing servo - basic");
  Serial.println("Servo animation init: 0 degrees");
  jawServo.write(SERVO_PIN, 0);
  Serial.println("Servo animation init: 70 degrees");
  delay(500);
  jawServo.write(SERVO_PIN, 70);
  Serial.println("Servo animation init complete; resetting to 0 degrees");
  delay(500);
  jawServo.write(SERVO_PIN, 0);

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

  // if (!SD.begin()) {
  //   Serial.println("SD Card: Mount Failed! Halting");
  //   while (1) {}  // Halt the code
  // }
  // Serial.println("SD Card: Mounted successfully");
  initSDCard();

  //CAMKILL: death version
  // Announce "System initialized"
  // Serial.printf("Playing %s\n", AUDIO_INITIALIZED);
  //CAMKILL: death version, works fine:
  //audioPlayer->playNow(AUDIO_INITIALIZED);

  // Announce "System initialized" and role
  Serial.printf("Playing initialization audio\n");
  audioPlayer->playNext(isMaster ? "/audio/Initialized - Master.wav" : "/audio/Initialized - Slave.wav");

  //CAMKILL: no more modes
  // // Announce the name of the initial mode
  // String currentModeAudioPath = getModeAudioPath(currentModeIndex);
  // audioPlayer->playNext(currentModeAudioPath.c_str());
}

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
const double ALPHA = 0.025;  // Smoothing factor; lower=smoother

const double EXAGGERATION_FACTOR = 10;
int exaggeratePosition(int pos) {
  if (pos < 35) {  // Mid-point of 0-70
    return pos / EXAGGERATION_FACTOR;
  } else {
    return 35 + (pos - 35) * EXAGGERATION_FACTOR;
  }
}

const int CHUNKS_NEEDED = 17;  // Number of chunks needed for 100ms (34)
int chunkCounter = 0;          // Counter for accumulated chunks

std::vector<int16_t> rawSamples;
std::vector<double> fftMagnitudes;
std::vector<int> servoPositions;


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

  //TODO: kill, but might need this code to remember how the audio-changing works
  // // Check mode-changing button with a non-blocking delay
  // if (currentMillis - lastButtonCheck >= DEBOUNCE_DELAY) {
  //   lastButtonCheck = currentMillis;
  //   int buttonState = digitalRead(MODE_BUTTON_PIN);

  //   // If the button state has changed
  //   if (buttonState != lastButtonState) {
  //     lastButtonState = buttonState;

  //     // Play the next audio file on button press
  //     if (buttonState == LOW) {
  //       Serial.println("Switching modes.");
  //       int nextIndex = (currentModeIndex + 1) % 4;
  //       currentModeIndex = nextIndex;
  //       String currentModeAudioPath = getModeAudioPath(currentModeIndex);
  //       audioPlayer->playNow(currentModeAudioPath.c_str());
  //     }
  //   }
  // }

  //TODO: put back after modifying, and this is somethign only the master should do
  // if (currentMillis - lastUltrasonicRead >= ULTRASONIC_READ_INTERVAL) {
  //   lastUltrasonicRead = currentMillis;
  //   float distance = distanceSensor->measureDistanceCm();
  //   //Serial.println("Ultrasonic distance: " + String(distance) + "cm");
  //   // If something is within range and there isn't audio already playing, trigger a random audio file from the current mode.
  //   if (distance > 0 && !audioPlayer->isCurrentlyPlaying()) {
  //     playRandomAudioFileFromCurrentMode();
  //   }
  // }

  // Play any queued audio.
  audioPlayer->update();

  // Step 1: Sampling Audio Data
  uint8_t* audioBuffer = audioPlayer->getCurrentAudioBuffer();
  size_t audioBufferSize = audioPlayer->getCurrentAudioBufferSize();
  //KILL:Serial.println("Sampling audioBufferSize: " + String(audioBufferSize));
  for (int i = 0; i < SAMPLES && i < audioBufferSize / 4; i++) {
    int16_t sample = (audioBuffer[i * 4 + 1] << 8) | audioBuffer[i * 4];
    vReal[i] = sample;
    vImag[i] = 0;
    //rawSamples.push_back(sample);
  }

  // Step 2: FFT Execution
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();

  // for (int i = 0; i < SAMPLES; i++) {
  //   fftMagnitudes.push_back(vReal[i]);
  // }

  // Step 3: Servo Control
  double dominantFrequency = FFT.MajorPeak();
  int servoPositionDegrees = map(dominantFrequency, 0, 5000, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // Exponential Smoothing
  int exaggeratedPosition = exaggeratePosition(servoPositionDegrees);
  smoothedPosition = ALPHA * exaggeratedPosition + (1 - ALPHA) * smoothedPosition;
  smoothedPosition = constrain(smoothedPosition, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  //servoPositions.push_back(round(smoothedPosition));

  //chunkCounter++;

  jawServo.write(SERVO_PIN, round(smoothedPosition));

  // if (chunkCounter >= CHUNKS_NEEDED) {
  //   // Output the accumulated data
  //   Serial.println("---- 100ms Data Snapshot ----");
  //   // Find the max values
  //   int maxRawSample = 0;
  //   if (!rawSamples.empty()) {
  //     maxRawSample = *std::max_element(rawSamples.begin(), rawSamples.end());
  //   } else {
  //     Serial.println("rawSamples is empty");
  //   }
  //   double maxFFTMagnitude = *std::max_element(fftMagnitudes.begin(), fftMagnitudes.end());
  //   int maxServoPosition = *std::max_element(servoPositions.begin(), servoPositions.end());

  //   // Print the max values
  //   Serial.println("Printing max values");
  //   Serial.println("Max Raw Sample: " + String(maxRawSample));
  //   Serial.println("Max FFT Magnitude: " + String(maxFFTMagnitude));
  //   Serial.println("Max Servo Position: " + String(maxServoPosition));

  //   // Clear data and reset counter
  //   rawSamples.clear();
  //   fftMagnitudes.clear();
  //   servoPositions.clear();
  //   chunkCounter = 0;
  // }
}