/*
  Two Chatting Skulls with Skull Jaws Synced to Audio

  Main file.

*/
#include <algorithm>  // For std::sort
#include <vector>
#include <tuple>
#include "bluetooth_audio.h"
#include "FS.h"      // Ensure ESP32 board package is installed
#include "SD.h"      // Ensure ESP32 board package is installed
#include <HCSR04.h>  // Install HCSR04 library via Arduino Library Manager
#include "audio_player.h"
#include "light_controller.h"
#include "servo_controller.h"
#include "sd_card_manager.h"
#include "skull_audio_animator.h"
#include <Servo.h>  // Use ESP32-specific Servo library
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp32-hal-log.h"
#include "config_manager.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "nvs_flash.h"

const bool SKIT_DEBUG = true;  // Will set eyes to 100% brightness when it's supposed to be talking and 10% when it's not.

const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED
LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);

// Ultrasonic sensor
UltraSonicDistanceSensor* distanceSensor = nullptr;  // NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
const int TRIGGER_PIN = 2;                           // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;                             // Pin number ultrasonic echo detection
unsigned long lastUltrasonicRead = 0;                // Last time the ultrasonic sensor was read

SDCardManager* sdCardManager = nullptr;
SDCardContent sdCardContent;

bool isPrimary = false;
AudioPlayer* audioPlayer = nullptr;  // We'll initialize this later
bluetooth_audio bluetoothAudio;      // Declare the bluetoothAudio object
SkullAudioAnimator* skullAudioAnimator = nullptr;  // We'll initialize this later

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

// Servo
const int SERVO_PIN = 15;  // Servo control pin
const int SERVO_MIN_DEGREES = 0;
const int SERVO_MAX_DEGREES = 80;  //Anything past this will grind the servo horn into the interior of the skull, probably breaking something.
ServoController servoController;

bool initializeBluetooth(const String& speakerName, int volume) {
  if (!audioPlayer || !skullAudioAnimator) {
    Serial.println("Error: audioPlayer or skullAudioAnimator is not initialized.");
    return false;
  }
  skullAudioAnimator->setBluetoothConnected(false);
  skullAudioAnimator->setAudioReadyToPlay(false);
  bluetoothAudio.begin(speakerName.c_str(), AudioPlayer::provideAudioFrames);
  bluetoothAudio.set_volume(volume);
  bool connected = bluetoothAudio.is_connected();
  Serial.printf("Bluetooth connected to %s: %d, Volume: %d\n", speakerName.c_str(), connected, volume);
  return connected;
}

void custom_crash_handler() {
  Serial.println("detected!");
  Serial.printf("Free memory at crash: %d bytes\n", ESP.getFreeHeap());

  // Print some basic debug info
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  Serial.printf("ESP32 Chip Revision: %d\n", chip_info.revision);
  Serial.printf("CPU Cores: %d\n", chip_info.cores);
  Serial.printf("Flash Size: %d MB\n", spi_flash_get_chip_size() / (1024 * 1024));

  // Print last error
  Serial.printf("Last error: %s\n", esp_err_to_name(esp_get_minimum_free_heap_size()));

  // Flush serial output
  Serial.flush();

  // Wait a bit before restarting
  delay(1000);

  // Restart the ESP32
  esp_restart();
}

void setup() {
  Serial.begin(115200);

  // Register custom crash handler
  esp_register_shutdown_handler((shutdown_handler_t)custom_crash_handler);

  delay(1000);

  Serial.println("\n\n\n\n\n\nStarting setup ... ");

  // Initialize light controller first for blinking
  lightController.begin();

  // Initialize SD Card Manager
  sdCardManager = new SDCardManager(audioPlayer);
  bool sdCardInitialized = false;

  while (!sdCardInitialized) {
    sdCardInitialized = sdCardManager->begin();
    if (!sdCardInitialized) {
      Serial.println("SD Card: Mount Failed! Retrying...");
      blinkEyesForFailure(3);  // 3 blinks for SD card failure
      delay(500);
    }
  }

  // Now that SD card is initialized, load configuration
  ConfigManager& config = ConfigManager::getInstance();
  bool configLoaded = false;

  while (!configLoaded) {
    configLoaded = config.loadConfig();
    if (!configLoaded) {
      Serial.println("Failed to load configuration. Retrying...");
      blinkEyesForFailure(5);  // 5 blinks for config file failure
      delay(500);
    }
  }

  // Configuration loaded successfully, now we can use it
  String bluetoothSpeakerName = config.getBluetoothSpeakerName();
  String role = config.getRole();
  int ultrasonicTriggerDistance = config.getUltrasonicTriggerDistance();
  int speakerVolume = config.getSpeakerVolume();

  // Initialize ultrasonic sensor (for both primary and secondary)
  distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, ultrasonicTriggerDistance);

  // Ping ultrasonic sensor 10 times for initialization and logging
  for (int i = 0; i < 10; i++) {
    float distance = distanceSensor->measureDistanceCm();
    Serial.println("Ultrasonic distance: " + String(distance) + "cm");
    delay(100);  // Wait between readings
  }

  audioPlayer = new AudioPlayer(servoController);
  audioPlayer->begin();

  // Initialize SkullAudioAnimator after audioPlayer and servoController are set up
  skullAudioAnimator = new SkullAudioAnimator(*audioPlayer, servoController);
  skullAudioAnimator->begin();

  // Determine role based on settings.txt
  if (role.equals("primary")) {
    isPrimary = true;
    lightController.blinkEyes(4);  // Blink eyes 4 times for Primary
    Serial.println("This skull is configured as PRIMARY");
  } else if (role.equals("secondary")) {
    isPrimary = false;
    lightController.blinkEyes(2);  // Blink eyes twice for Secondary
    Serial.println("This skull is configured as SECONDARY");
  } else {
    lightController.blinkEyes(2);  // Blink eyes twice for Secondary
    Serial.println("Invalid role in settings.txt. Defaulting to SECONDARY");
    isPrimary = false;
  }

  initializeBluetooth(bluetoothSpeakerName, speakerVolume);

  // Initialize servo
  servoController.initialize(SERVO_PIN, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // Load SD card content
  sdCardContent = sdCardManager->loadContent();
  if (sdCardContent.skits.empty()) {
    Serial.println("No skits found on SD card. Halting setup.");
    blinkEyesForFailure(10);  // 10 blinks for no skits
    return;
  }

  // Announce "System initialized" and role
  Serial.printf("Playing initialization audio\n");
  skullAudioAnimator->playNext(isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav");

  // Find the "Skit - names" skit
  ParsedSkit namesSkit = skullAudioAnimator->findSkitByName(sdCardContent.skits, "Skit - names");
  if (namesSkit.audioFile != "" && namesSkit.txtFile != "") {
    // Queue the "Skit - names" skit to play next
    Serial.println("'Skit - names' found; playing next.");
    skullAudioAnimator->playSkitNext(namesSkit);
  } else {
    Serial.println("'Skit - names' not found.");
  }
}

void blinkEyesForFailure(int numBlinks) {
  for (int i = 0; i < numBlinks; i++) {
    lightController.setEyeBrightness(255);
    delay(100);
    lightController.setEyeBrightness(0);
    delay(100);
  }
  delay(500);  // Pause between sets of blinks
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastMillis = 0;
  static unsigned long lastMemoryCheck = 0;

  // Reset the watchdog timer
  esp_task_wdt_reset();

  // Every 1000ms output "loop() running" and check memory
  if (currentMillis - lastMillis >= 1000) {
    size_t freeHeap = ESP.getFreeHeap();
    Serial.printf("%d loop() running. Free memory: %d bytes\n", currentMillis, freeHeap);
    lastMillis = currentMillis;
  }

  // Check memory every 100ms
  if (currentMillis - lastMemoryCheck >= 100) {
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {  // Adjust this threshold as needed
      Serial.printf("WARNING: Low memory: %d bytes\n", freeHeap);
    }
    lastMemoryCheck = currentMillis;
  }

  // Update SkullAudioAnimator
  if (skullAudioAnimator) {
    skullAudioAnimator->update();
  }

  // Allow other tasks to run
  delay(1);

  static unsigned long lastAudioProgress = 0;
  static unsigned long lastAudioCheck = 0;
  unsigned long currentTime = millis();

  if (skullAudioAnimator && skullAudioAnimator->isCurrentlyPlaying() && currentTime - lastAudioCheck > 5000) {
    if (skullAudioAnimator->getTotalBytesRead() == lastAudioProgress) {
      Serial.println("WARNING: Audio playback seems to be stalled!");
      skullAudioAnimator->logState();
    }
    lastAudioProgress = skullAudioAnimator->getTotalBytesRead();
    lastAudioCheck = currentTime;
  }
}