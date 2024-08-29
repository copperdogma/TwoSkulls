#include "bluetooth_audio.h"

bluetooth_audio::bluetooth_audio() : is_bluetooth_connected(false), device_name(nullptr), audio_provider_callback(nullptr), last_reconnection_attempt(0) {}

void bluetooth_audio::begin(const char* device_name, music_data_channels_cb_t audioProviderCallback) {
    this->device_name = device_name;
    this->audio_provider_callback = audioProviderCallback;
    a2dp_source.set_auto_reconnect(true);
    Serial.println("Setting connection state callback...");
    a2dp_source.set_on_connection_state_changed(connection_state_changed, this);
    Serial.println("Starting A2DP source...");
    a2dp_source.start(device_name, audioProviderCallback);
    Serial.println("Bluetooth initialization complete.");
}

void bluetooth_audio::set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj)) {
    a2dp_source.set_on_connection_state_changed(callback, this);
}

bool bluetooth_audio::is_connected() {
    return a2dp_source.is_connected();
}

void bluetooth_audio::set_volume(uint8_t volume) {
    a2dp_source.set_volume(volume);
}

void bluetooth_audio::update() {
    unsigned long current_time = millis();
    if (!is_connected() && (current_time - last_reconnection_attempt >= RECONNECTION_INTERVAL)) {
        attempt_reconnection();
        last_reconnection_attempt = current_time;
    }
}

void bluetooth_audio::connection_state_changed(esp_a2d_connection_state_t state, void* ptr) {
    Serial.println("Bluetooth connection_state_changed called.");
    bluetooth_audio* self = static_cast<bluetooth_audio*>(ptr);
    
    switch(state) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.printf("Lost connection to Bluetooth speaker '%s'.\n", self->device_name);
            self->is_bluetooth_connected = false;
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            Serial.printf("Connecting to Bluetooth speaker '%s'...\n", self->device_name);
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.printf("Successfully connected to Bluetooth speaker '%s'.\n", self->device_name);
            self->is_bluetooth_connected = true;
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            Serial.printf("Disconnecting from Bluetooth speaker '%s'...\n", self->device_name);
            break;
        default:
            Serial.printf("Unknown connection state for Bluetooth speaker '%s'.\n", self->device_name);
    }
}

void bluetooth_audio::attempt_reconnection() {
    if (!is_connected()) {
        Serial.printf("Attempting to reconnect to Bluetooth speaker '%s'...\n", device_name);
        a2dp_source.start(device_name, audio_provider_callback);
    }
}