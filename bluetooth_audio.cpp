/*
    

    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

    struct __attribute__((packed)) Frame {
        int16_t channel1;
        int16_t channel2;

        Frame(int v=0){
            channel1 = channel2 = v;
        }
        
        Frame(int ch1, int ch2){
            channel1 = ch1;
            channel2 = ch2;
        }
    };
*/

#include "bluetooth_audio.h"
#include <cstring>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "nvs_flash.h"

bluetooth_audio* bluetooth_audio::instance = nullptr;

bluetooth_audio::bluetooth_audio() : is_bluetooth_connected(false), last_reconnection_attempt(0) {
    _speaker_name[0] = '\0';
    instance = this;
}

void bluetooth_audio::begin(const char* speaker_name, std::function<int32_t(Frame*, int32_t)> audioProviderCallback) {
    Serial.println("Initializing Bluetooth...");

    if (speaker_name == nullptr) {
        Serial.println("ERROR: Null bluetooth speaker name provided");
        return;
    }
    
    size_t name_length = strlen(speaker_name);
    if (name_length >= sizeof(_speaker_name)) {
        Serial.println("WARNING: Bluetooth speaker name too long, will be truncated");
    }
    
    strlcpy(_speaker_name, speaker_name, sizeof(_speaker_name));
    
    audio_provider_callback = audioProviderCallback;
    Serial.printf("Starting as A2DP source, connecting to speaker name: %s\n", _speaker_name);

    a2dp_source.set_auto_reconnect(true);
    a2dp_source.set_on_connection_state_changed(connection_state_changed, this);
    a2dp_source.start(_speaker_name, audio_callback_trampoline);

    Serial.println("Bluetooth initialization complete.");
}

// The bluetooth library automatically calls this to get audio data from the audio player to the bluetooth module.
// The bluetooth only calls it when there's an active bluetooth connection.
int bluetooth_audio::audio_callback_trampoline(Frame* frame, int frame_count) {
    if (instance && instance->audio_provider_callback) {
        return static_cast<int>(instance->audio_provider_callback(frame, static_cast<int32_t>(frame_count)));
    }
    return 0;
}

bool bluetooth_audio::initializeBluetooth(const String& speakerName, int volume) {
    esp_err_t err;
    
    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Release the memory of BLE if it is not needed
    err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        Serial.printf("Bluetooth memory release failed: %s\n", esp_err_to_name(err));
        // Continue anyway, as this might not be critical
    }

    esp_bt_controller_status_t bt_status = esp_bt_controller_get_status();
    
    if (bt_status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        Serial.println("Bluetooth controller already enabled, skipping initialization and enabling steps");
    } else if (bt_status == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        Serial.println("Initializing Bluetooth controller...");
        if ((err = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
            Serial.printf("Initialize controller failed: %s\n", esp_err_to_name(err));
            return false;
        }
        Serial.println("Bluetooth controller initialized successfully");

        Serial.println("Enabling Bluetooth controller...");
        if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
            Serial.printf("Enable controller failed: %s\n", esp_err_to_name(err));
            Serial.printf("Error code: %d\n", err);
            Serial.printf("Controller status: %d\n", esp_bt_controller_get_status());
            return false;
        }
        Serial.println("Bluetooth controller enabled successfully");
    } else {
        Serial.printf("Controller is in unexpected state: %d\n", bt_status);
        // Instead of returning false, let's try to proceed
        Serial.println("Attempting to continue despite unexpected controller state");
    }

    Serial.println("Initializing Bluedroid...");
    if ((err = esp_bluedroid_init()) != ESP_OK) {
        Serial.printf("Initialize bluedroid failed: %s\n", esp_err_to_name(err));
        return false;
    }
    Serial.println("Bluedroid initialized successfully");

    Serial.println("Enabling Bluedroid...");
    if ((err = esp_bluedroid_enable()) != ESP_OK) {
        Serial.printf("Enable bluedroid failed: %s\n", esp_err_to_name(err));
        return false;
    }
    Serial.println("Bluedroid enabled successfully");

    // Set device name
    Serial.printf("Setting device name to: %s\n", speakerName.c_str());
    if ((err = esp_bt_dev_set_device_name(speakerName.c_str())) != ESP_OK) {
        Serial.printf("Set device name failed: %s\n", esp_err_to_name(err));
        return false;
    }

    this->begin(speakerName.c_str(), nullptr);  // We'll set the audio callback later
    this->set_volume(volume);

    bool connected = this->is_connected();
    Serial.printf("Bluetooth initialized. Connected to %s: %d, Volume: %d\n", speakerName.c_str(), connected, volume);
    return true;  // Return true if initialization is successful, even if not connected yet
}

void bluetooth_audio::set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj)) {
    a2dp_source.set_on_connection_state_changed(callback, this);
}

bool bluetooth_audio::is_connected() {
    return a2dp_source.is_connected();
}

void bluetooth_audio::set_volume(uint8_t volume) {
    Serial.printf("Setting bluetooth speaker volume to %d\n", volume);
    a2dp_source.set_volume(volume);
}

const char* bluetooth_audio::get_speaker_name() const {
    return _speaker_name;
}

void bluetooth_audio::connection_state_changed(esp_a2d_connection_state_t state, void* ptr) {
    bluetooth_audio* self = static_cast<bluetooth_audio*>(ptr);
        
    switch(state) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.printf("Not connected to Bluetooth speaker '%s'.\n", self->_speaker_name);
            self->is_bluetooth_connected = false;
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            Serial.printf("Attempting to connect to Bluetooth speaker '%s'...\n", self->_speaker_name);
            //CAMKILL:self->disableComms(); // Updated from instance->disableComms();
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.printf("Successfully connected to Bluetooth speaker '%s'.\n", self->_speaker_name);
            self->is_bluetooth_connected = true;
            //CAMKILL:self->enableComms(); // Updated from instance->enableComms();
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            Serial.printf("Disconnecting from Bluetooth speaker '%s'...\n", self->_speaker_name);
            break;
        default:
            Serial.printf("Unknown connection state for Bluetooth speaker '%s'.\n", self->_speaker_name);
    }
}

void bluetooth_audio::resetBluetooth() {
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  delay(1000);
  
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  
  esp_bt_controller_enable(ESP_BT_MODE_BTDM);
  esp_bluedroid_init();
  esp_bluedroid_enable();
}

void bluetooth_audio::clearBluetoothNVS() {
  nvs_flash_erase();
  nvs_flash_init();
}
