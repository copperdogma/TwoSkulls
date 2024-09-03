/*
  Two Chatting Skulls with Skull Jaws Synced to Audio

  Main file.

*/
#include <algorithm>  // For std::sort
#include <vector>
#include <tuple>
#include "bluetooth_audio.h"
#include "FS.h"  // Ensure ESP32 board package is installed
#include "SD.h"  // Ensure ESP32 board package is installed
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
  if (!audioPlayer) {
    Serial.println("Error: audioPlayer is not initialized.");
    return false;
  }
  audioPlayer->setBluetoothConnected(false);
  audioPlayer->setAudioReadyToPlay(false);
  bluetoothAudio.begin(BLUETOOTH_SPEAKER_NAME, AudioPlayer::provideAudioFrames);
  bluetoothAudio.set_volume(100);
  bool connected = bluetoothAudio.is_connected();
  Serial.printf("Bluetooth connected: %d\n", connected);
  return connected;
}

SkullAudioAnimator* skullAnimator = nullptr;

void custom_crash_handler() {
  Serial.println("Crash detected!");
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

  Serial.println("\n\n\n\n\n\nStarting setup ... ");

  // Enable watchdog timer with a timeout of 8 seconds
  esp_task_wdt_init(8, true);  // Enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);  // Add current thread to WDT watch

  // Register custom crash handler
  esp_register_shutdown_handler((shutdown_handler_t)custom_crash_handler);

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

  if (!sdCardInitialized) {
    Serial.println("SD Card initialization failed. Halting setup.");
    return;
  }

  // Load SD card content
  sdCardContent = sdCardManager->loadContent();
  if (sdCardContent.skits.empty()) {
    Serial.println("No skits found on SD card. Halting setup.");
    return;
  }

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
  static unsigned long lastMemoryCheck = 0;

  // Reset the watchdog timer
  esp_task_wdt_reset();

  // Every 1000ms output "loop() running" and check memory
  if (currentMillis - lastMillis >= 1000) {
    Serial.printf("%d loop() running\n", currentMillis);
    lastMillis = currentMillis;
    printFreeMemory();  // Monitor free memory
  }

  // Check memory every 100ms
  if (currentMillis - lastMemoryCheck >= 100) {
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {  // Adjust this threshold as needed
      Serial.printf("WARNING: Low memory: %d bytes\n", freeHeap);
    }
    lastMemoryCheck = currentMillis;
  }

  // Update audio player and process any queued audio
  if (audioPlayer) {
    audioPlayer->update();
  }

  // Update SkullAudioAnimator
  if (skullAnimator) {
    skullAnimator->update();
  }

  // Allow other tasks to run
  delay(1);
}

// Add function to monitor free memory
void printFreeMemory() {
    size_t freeHeap = ESP.getFreeHeap();
    Serial.printf("Free memory: %d bytes\n", freeHeap);
}