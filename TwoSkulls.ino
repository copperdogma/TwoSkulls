/*
  Two Chatting Skulls with Skull Jaws Synced to Audio

  Main file.

*/
#include "bluetooth_audio.h"
#include "FS.h"      // Ensure ESP32 board package is installed
#include "SD.h"      // Ensure ESP32 board package is installed
#include <HCSR04.h>  // Install HCSR04 library via Arduino Library Manager
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

// Remove this line
// AudioPlayer* audioPlayer = nullptr;

const int LEFT_EYE_PIN = 32;   // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33;  // GPIO pin for right eye LED
LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);

// Ultrasonic sensor
UltraSonicDistanceSensor* distanceSensor = nullptr;  // NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
const int TRIGGER_PIN = 2;                           // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;                             // Pin number ultrasonic echo detection

SDCardManager* sdCardManager = nullptr;
SDCardContent sdCardContent;

bool isPrimary = false;
ServoController servoController;                   // Keep this line
SkullAudioAnimator* skullAudioAnimator = nullptr;  // Change this line
bluetooth_audio bluetoothAudio;                    // Declare the bluetoothAudio object

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

bool initializeBluetooth(const String& speakerName, int volume) {
  bluetoothAudio.begin(speakerName.c_str(), [](Frame* frame, int32_t frame_count) {
    return skullAudioAnimator->provideAudioFrames(frame, frame_count);
  });
  bluetoothAudio.set_volume(volume);
  bool connected = bluetoothAudio.is_connected();
  Serial.printf("Bluetooth connected to %s: %d, Volume: %d\n", speakerName.c_str(), connected, volume);
  skullAudioAnimator->setBluetoothConnected(connected);
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

  // Initialize servo
  servoController.initialize(SERVO_PIN, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // Remove this line:
  // AudioPlayer* audioPlayer = new AudioPlayer();

  // Initialize SD Card Manager
  sdCardManager = new SDCardManager(skullAudioAnimator);
  bool sdCardInitialized = false;

  while (!sdCardInitialized) {
    sdCardInitialized = sdCardManager->begin();
    if (!sdCardInitialized) {
      Serial.println("SD Card: Mount Failed! Retrying...");
      lightController.blinkEyes(3);  // 3 blinks for SD card failure
      delay(500);
    }
  }

  // Load SD card content
  sdCardContent = sdCardManager->loadContent();
  if (sdCardContent.skits.empty()) {
    Serial.println("No skits found on SD card.");
  }

  // Now that SD card is initialized, load configuration
  ConfigManager& config = ConfigManager::getInstance();
  bool configLoaded = false;

  while (!configLoaded) {
    configLoaded = config.loadConfig();
    if (!configLoaded) {
      Serial.println("Failed to load configuration. Retrying...");
      lightController.blinkEyes(5);  // 5 blinks for config file failure
      delay(500);
    }
  }

  // Configuration loaded successfully, now we can use it
  String bluetoothSpeakerName = config.getBluetoothSpeakerName();
  String role = config.getRole();
  int ultrasonicTriggerDistance = config.getUltrasonicTriggerDistance();
  int speakerVolume = config.getSpeakerVolume();

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

  // Initialize SkullAudioAnimator
  skullAudioAnimator = new SkullAudioAnimator(isPrimary, servoController, lightController, sdCardContent.skits);
  skullAudioAnimator->begin();

  // Announce "System initialized" and role
  String initAudioFilePath = isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav";
  Serial.printf("Playing initialization audio: %s\n", initAudioFilePath.c_str());
  skullAudioAnimator->playNext(initAudioFilePath.c_str());
  Serial.printf("Queued initialization audio: %s\n", initAudioFilePath.c_str());

  // Queue the "Skit - names" skit to play next
  ParsedSkit namesSkit = sdCardManager->findSkitByName(sdCardContent.skits, "Skit - names");
  Serial.println("'Skit - names' found; playing next.");
  skullAudioAnimator->playSkitNext(namesSkit);
  Serial.printf("Queued 'Skit - names' audio: %s\n", namesSkit.audioFile.c_str());

  // Initialize ultrasonic sensor (for both primary and secondary)
  distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, ultrasonicTriggerDistance);

  // Ping ultrasonic sensor 10 times for initialization and logging
  for (int i = 0; i < 10; i++) {
    float distance = distanceSensor->measureDistanceCm();
    Serial.println("Ultrasonic distance: " + String(distance) + "cm");
    delay(50);  // Wait between readings
  }

  // Initialize Bluetooth after SkullAudioAnimator
  initializeBluetooth(bluetoothSpeakerName, speakerVolume);

  // Set the initial state of the eyes to dim
  Serial.println("TwoSkulls: Setting initial eye brightness to dim");
  lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastMillis = 0;

  // Reset the watchdog timer
  esp_task_wdt_reset();

  // Every 5000ms output "loop() running" and check memory
  if (currentMillis - lastMillis >= 5000) {
    size_t freeHeap = ESP.getFreeHeap();
    Serial.printf("%d loop() running. Free memory: %d bytes\n", currentMillis, freeHeap);
    lastMillis = currentMillis;
  }

  // Update SkullAudioAnimator
  skullAudioAnimator->update();

  // Allow other tasks to run
  delay(1);
}