#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <functional>
#include <string>

class bluetooth_controller {
public:
    bluetooth_controller();
    void begin(const String& speaker_name, std::function<int32_t(Frame*, int32_t)> audioProviderCallback, bool isPrimary);
    void set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj));
    bool is_connected();
    void set_volume(uint8_t volume);
    const String& get_speaker_name() const;
    void setCharacteristicValue(const char *value);
    void update(); // New method to be called in the main loop

    static bluetooth_controller *instance;

private:
    BLEScan* pBLEScanner;
    BLEClient* pClient;
    BLECharacteristic* pCharacteristic;
    BLERemoteCharacteristic* pRemoteCharacteristic;

    void initializeBLEServer();
    void initializeBLEClient();
    static int audio_callback_trampoline(Frame* frame, int frame_count);
    static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr);
    bool connectToServer();
    void handleBLEClient();

    bool m_isPrimary;
    String m_speaker_name;
    std::function<int32_t(Frame*, int32_t)> audio_provider_callback;
    bool is_bluetooth_connected;
    unsigned long last_reconnection_attempt;
    BluetoothA2DPSource a2dp_source;

    void startScan();  // Add this line
};

#endif // BLUETOOTH_CONTROLLER_H