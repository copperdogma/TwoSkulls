#include "bluetooth_audio.h"
#include <cstring>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "nvs_flash.h"

bluetooth_audio::bluetooth_audio() : is_bluetooth_connected(false), audio_provider_callback(nullptr), last_reconnection_attempt(0) {
    _speaker_name[0] = '\0';
}

void bluetooth_audio::begin(const char* speaker_name, music_data_channels_cb_t audioProviderCallback) {
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
    a2dp_source.start(_speaker_name, audioProviderCallback);

    Serial.println("Bluetooth initialization complete.");
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
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.printf("Successfully connected to Bluetooth speaker '%s'.\n", self->_speaker_name);
            self->is_bluetooth_connected = true;
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