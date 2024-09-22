#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include <functional>
#include <string> // Added for String usage

class bluetooth_controller {
public:
    bluetooth_controller();
    void begin(const String& speaker_name, std::function<int32_t(Frame*, int32_t)> audioProviderCallback, bool isPrimary);
    void set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj));
    bool is_connected();
    void set_volume(uint8_t volume);
    const String& get_speaker_name() const;
    void setCharacteristicValue(const char *value);
    static void resetBluetooth();
    static void clearBluetoothNVS();

private:
    static bluetooth_controller* instance;
    BluetoothA2DPSource a2dp_source;
    bool is_bluetooth_connected;
    std::function<int32_t(Frame*, int32_t)> audio_provider_callback;
    unsigned long last_reconnection_attempt;
    static const unsigned long RECONNECTION_INTERVAL = 5000; // 5 seconds

    static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr);
    void attempt_reconnection();
    String m_speaker_name;
    bool m_isPrimary;

    static int audio_callback_trampoline(Frame* frame, int frame_count);
};

#endif // BLUETOOTH_CONTROLLER_H