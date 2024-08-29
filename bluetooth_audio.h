#ifndef BLUETOOTH_AUDIO_H
#define BLUETOOTH_AUDIO_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"

class bluetooth_audio {
public:
    bluetooth_audio();
    void begin(const char* device_name, music_data_channels_cb_t audioProviderCallback);
    void set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj));
    bool is_connected();
    void set_volume(uint8_t volume);
    void update();

private:
    BluetoothA2DPSource a2dp_source;
    const char* device_name;
    bool is_bluetooth_connected;
    music_data_channels_cb_t audio_provider_callback;
    unsigned long last_reconnection_attempt;
    static const unsigned long RECONNECTION_INTERVAL = 5000; // 5 seconds

    static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr);
    void attempt_reconnection();
};

#endif // BLUETOOTH_AUDIO_H