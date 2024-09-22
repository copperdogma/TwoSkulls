/*
  Two Chatting Skulls with Skull Jaws Synced to Audio

  Main file.

*/
#include <Arduino.h>
#include "bluetooth_controller.h"
#include "FS.h"     // Ensure ESP32 board package is installed
#include "SD.h"     // Ensure ESP32 board package is installed
#include <HCSR04.h> // Install HCSR04 library via Arduino Library Manager
#include "light_controller.h"
#include "servo_controller.h"
#include "sd_card_manager.h"
#include "skull_audio_animator.h"
#include "audio_player.h"
#include "config_manager.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include <esp_coexist.h>
#include "nvs_flash.h"
#include "skull_communication.h"
#include "esp_sleep.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "radio_manager.h"

const int LEFT_EYE_PIN = 32;  // GPIO pin for left eye LED
const int RIGHT_EYE_PIN = 33; // GPIO pin for right eye LED
LightController lightController(LEFT_EYE_PIN, RIGHT_EYE_PIN);

// Ultrasonic sensor
UltraSonicDistanceSensor *distanceSensor = nullptr; // NOTE: you can set a max distance with third param, e.g. (100cm cutoff): distanceSensor(TRIGGER_PIN, ECHO_PIN, 100);
const int TRIGGER_PIN = 2;                          // Pin number ultrasonic pulse trigger
const int ECHO_PIN = 22;                            // Pin number ultrasonic echo detection

SDCardManager *sdCardManager = nullptr;
SDCardContent sdCardContent;

bool isPrimary = false;
ServoController servoController;
bluetooth_controller bluetoothController;
AudioPlayer *audioPlayer = nullptr;
// CAMKILL:SkullCommunication *skullCommunication = nullptr;
RadioManager radioManager;

// Exponential smoothing
struct AudioState
{
  double smoothedPosition = 0;
  double maxObservedRMS = 0;
  bool isJawClosed = true;
  int chunkCounter = 0;
};

AudioState audioState;

const double ALPHA = 0.2;             // Smoothing factor; 0-1, lower=smoother
const double SILENCE_THRESHOLD = 200; // Higher = vol needs to be louder to be considered "not silence", max 2000?
const int MIN_MOVEMENT_THRESHOLD = 3; // Minimum degree change to move the servo
const double MOVE_EXPONENT = 0.2;     // 0-1, smaller = more movement
const double RMS_MAX = 32768.0;       // Maximum possible RMS value for 16-bit audio

const int CHUNKS_NEEDED = 17; // Number of chunks needed for 100ms (34)

// Servo
const int SERVO_PIN = 15; // Servo control pin
const int SERVO_MIN_DEGREES = 0;
const int SERVO_MAX_DEGREES = 80; // Anything past this will grind the servo horn into the interior of the skull, probably breaking something.

esp_adc_cal_characteristics_t adc_chars;

void custom_crash_handler()
{
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

void onMessageSent(const struct_message &msg)
{
  if (isPrimary && msg.message == Message::CONNECTION_REQUEST)
  {
    lightController.blinkEyes(1); // 1 blink for wifi connection request
    if (bluetoothController.is_connected() && !audioPlayer->isAudioPlaying())
    {
      audioPlayer->playNext("/audio/Marco.wav");
    }
  }

  if (!isPrimary && msg.message == Message::CONNECTION_ACK)
  {
    lightController.blinkEyes(1); // 1 blink for wifi connection received
    if (bluetoothController.is_connected() && !audioPlayer->isAudioPlaying())
    {
      // The CONNECTION_REQUEST/CONNECTION_ACK is so fast that the Polo will play while Marco is still playing.
      // Wait 1000ms before playing Polo.
      delay(1500);
      audioPlayer->playNext("/audio/Polo.wav");
    }
  }

  if (!isPrimary && msg.message == Message::PLAY_FILE_ACK)
  {
    // Immediately play the names skit to coincide with the secondary skyll accepting the command so we're in sync.
    if (bluetoothController.is_connected() && !audioPlayer->isAudioPlaying())
    {
      audioPlayer->playNext("/audio/Skit - names.wav");
    }
  }
}

void onMessageReceived(const struct_message &msg)
{
}

void setup()
{
  Serial.begin(115200);

  // Register custom crash handler
  esp_register_shutdown_handler((shutdown_handler_t)custom_crash_handler);

  delay(1000);

  Serial.println("\n\n\n\n\n\nStarting setup ... ");

  // Initialize light controller first for blinking
  lightController.begin();

  // Initialize servo
  servoController.initialize(SERVO_PIN, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);

  // Initialize SD Card Manager
  sdCardManager = new SDCardManager();

  // Attempt to initialize the SD card until successful
  while (!sdCardManager->begin())
  {
    Serial.println("SD Card: Mount Failed! Retrying...");
    lightController.blinkEyes(3); // 3 blinks for SD card failure
    delay(500);
  }

  // Load SD card content
  sdCardContent = sdCardManager->loadContent();
  if (sdCardContent.skits.empty())
  {
    Serial.println("No skits found on SD card.");
  }

  // Now that SD card is initialized, load configuration
  ConfigManager &config = ConfigManager::getInstance();
  bool configLoaded = false;

  while (!configLoaded)
  {
    configLoaded = config.loadConfig();
    if (!configLoaded)
    {
      Serial.println("Failed to load configuration. Retrying...");
      lightController.blinkEyes(5); // 5 blinks for config file failure
      delay(500);
    }
  }

  // Configuration loaded successfully, now we can use it
  String bluetoothSpeakerName = config.getBluetoothSpeakerName();
  String role = config.getRole();
  int ultrasonicTriggerDistance = config.getUltrasonicTriggerDistance();
  int speakerVolume = config.getSpeakerVolume();

  // Determine role based on settings.txt
  if (role.equals("primary"))
  {
    isPrimary = true;
    lightController.blinkEyes(4); // Blink eyes 4 times for Primary
    Serial.println("This skull is configured as PRIMARY");
  }
  else if (role.equals("secondary"))
  {
    isPrimary = false;
    lightController.blinkEyes(2); // Blink eyes twice for Secondary
    Serial.println("This skull is configured as SECONDARY");
  }
  else
  {
    lightController.blinkEyes(2); // Blink eyes twice for Secondary
    Serial.printf("Invalid role in settings.txt ('%s'). Defaulting to SECONDARY\n", role.c_str());
    isPrimary = false;
  }

  // Initialize AudioPlayer
  esp_coex_preference_set(ESP_COEX_PREFER_WIFI);

  audioPlayer = new AudioPlayer(*sdCardManager, radioManager);

  // Announce "System initialized" and role
  String initAudioFilePath = isPrimary ? "/audio/Initialized - Primary.wav" : "/audio/Initialized - Secondary.wav";
  Serial.printf("Playing initialization audio: %s\n", initAudioFilePath.c_str());
  audioPlayer->playNext(initAudioFilePath);
  Serial.printf("Queued initialization audio: %s\n", initAudioFilePath.c_str());

  // Queue the "Skit - names" skit to play next
  // ParsedSkit namesSkit = sdCardManager->findSkitByName(sdCardContent.skits, "Skit - names");
  // audioPlayer->playNext(namesSkit.audioFile.c_str());
  // Serial.printf("'Skit - names' found; queueing audio: %s\n", namesSkit.audioFile.c_str());

  // Initialize ultrasonic sensor (for both primary and secondary)
  distanceSensor = new UltraSonicDistanceSensor(TRIGGER_PIN, ECHO_PIN, ultrasonicTriggerDistance);

  // Ping ultrasonic sensor 10 times for initialization and logging
  for (int i = 0; i < 10; i++)
  {
    float distance = distanceSensor->measureDistanceCm();
    Serial.println("Ultrasonic distance: " + String(distance) + "cm");
    delay(50); // Wait between readings
  }

  // CAMKILL: (also kill confi settings for mac addresses)
  // Initialize SkullCommunication after determining the role
  // String primaryMacAddress = config.getPrimaryMacAddress();
  // String secondaryMacAddress = config.getSecondaryMacAddress();

  // String macAddress = isPrimary ? primaryMacAddress : secondaryMacAddress;
  // String otherMacAddress = isPrimary ? secondaryMacAddress : primaryMacAddress;

  // skullCommunication = new SkullCommunication(isPrimary, macAddress, otherMacAddress, &radioManager);
  // skullCommunication->registerSendCallback(onMessageSent);
  // skullCommunication->registerReceiveCallback(onMessageReceived);
  // skullCommunication->begin();

  // Initialize Bluetooth after AudioPlayer.
  // Include the callback so that the bluetooth_controller library can call the AudioPlayer's
  // provideAudioFrames method to get more audio data when the bluetooth speaker needs it.
  bluetoothController.begin(bluetoothSpeakerName, [](Frame *frame, int32_t frame_count)
                       { return audioPlayer->provideAudioFrames(frame, frame_count); }, isPrimary);
  bluetoothController.set_volume(speakerVolume);

  // Set the initial state of the eyes to dim
  lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);

  // Configure ADC
  // TODO: what is this? Is it needed? At the very least, document it.
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  // CAMKILL:
  //  // Set play file callback for skull_communication
  //  skullCommunication->setPlayFileCallback([](String filePath)
  //                                          {
  //    // This callback is called when a PLAY_FILE_ACK is received and it's time to play audio
  //    audioPlayer->playNext(filePath); });

  audioPlayer->setPlaybackStartCallback([](const String &filePath)
                                        { Serial.printf("MAIN: Started playing audio: %s\n", filePath.c_str()); });

  audioPlayer->setPlaybackEndCallback([](const String &filePath)
                                      { Serial.printf("MAIN: Finished playing audio: %s\n", filePath.c_str()); });

  audioPlayer->setAudioFramesProvidedCallback([](const String &filePath, const Frame *frames, int32_t frameCount)
                                              {
                                                // CAMKILL: Serial.printf("Provided %d frames for %s\n", frameCount, filePath.c_str());
                                              });
}

void loop()
{
  unsigned long currentMillis = millis();
  static unsigned long lastMillis = 0;

  // Reset the watchdog timer
  esp_task_wdt_reset();

  // Update the audio player's state
  bool isAudioPlaying = audioPlayer->isAudioPlaying();

  // Every 5000ms output "loop() running" and check memory, reset reason, and power info
  if (currentMillis - lastMillis >= 5000)
  {
    size_t freeHeap = ESP.getFreeHeap();
    esp_reset_reason_t reset_reason = esp_reset_reason();

    // Read ADC and convert to voltage
    uint32_t adc_reading = adc1_get_raw(ADC1_CHANNEL_0);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);

    Serial.printf("%lu loop() running. Free memory: %d bytes, ", currentMillis, freeHeap);
    Serial.printf("Bluetooth connected: %s, ", bluetoothController.is_connected() ? "true" : "false");
    Serial.printf("isAudioPlaying: %s, ", isAudioPlaying ? "true" : "false");
    // CAMKILL: Serial.printf("Peer connected: %s, ", skullCommunication->isPeerConnected() ? "true" : "false");
    Serial.printf("Voltage: %d mV, ", voltage);
    // CAMKILL: Serial.printf("lastHeardTime: %lu\n", skullCommunication->getLastHeardTime());
    Serial.printf("\n");

    if (reset_reason == ESP_RST_BROWNOUT)
    {
      Serial.println("WARNING: Last reset was due to brownout!");
    }

    lastMillis = currentMillis;
  }

  // // Update SkullCommunication
  // if (bluetoothAudio.is_connected())
  // {
  //   skullCommunication->enableCommunication();
  //   skullCommunication->update();
  // }
  // else
  // {
  //   skullCommunication->disableCommunication();
  // }

  // // Test sending play command every 10 seconds
  // static unsigned long lastPlayCommand = 0;
  // if (isPrimary && skullCommunication->isPeerConnected() && bluetoothAudio.is_connected() && currentMillis - lastPlayCommand >= 10000)
  // {
  //   skullCommunication->sendPlayCommand("/audio/Skit - names.wav");
  //   lastPlayCommand = currentMillis;
  // }

  // Test updating BLE characteristic every 10 seconds
  // static unsigned long lastCharacteristicUpdate = 0;
  // if (currentMillis - lastCharacteristicUpdate >= 10000)
  // {
  //   String message = "Hello from the primary skull at " + String(currentMillis) + "ms";
  //   bluetoothController.setCharacteristicValue(message.c_str());
  //   lastCharacteristicUpdate = currentMillis;
  //   Serial.printf("Updated BLE characteristic with message: %s\n", message.c_str());
  // }

  // Allow other tasks to run
  delay(1);
}