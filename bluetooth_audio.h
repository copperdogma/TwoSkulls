#ifndef BLUETOOTH_AUDIO_H
#define BLUETOOTH_AUDIO_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include <functional>

class bluetooth_audio {
public:
    bluetooth_audio();
    void begin(const char* speaker_name, std::function<int32_t(Frame*, int32_t)> audioProviderCallback);
    bool initializeBluetooth(const String& speakerName, int volume);
    void set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj));
    bool is_connected();
    void set_volume(uint8_t volume);
    const char* get_speaker_name() const;

    static void resetBluetooth();
    static void clearBluetoothNVS();

private:
    static bluetooth_audio* instance;
    BluetoothA2DPSource a2dp_source;
    bool is_bluetooth_connected;
    std::function<int32_t(Frame*, int32_t)> audio_provider_callback;
    unsigned long last_reconnection_attempt;
    static const unsigned long RECONNECTION_INTERVAL = 5000; // 5 seconds

    static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr);
    void attempt_reconnection();
    char _speaker_name[64];

    static int audio_callback_trampoline(Frame* frame, int frame_count);
};

#endif // BLUETOOTH_AUDIO_H