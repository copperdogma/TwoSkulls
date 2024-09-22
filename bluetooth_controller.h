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
    bool isA2dpConnected();
    void set_volume(uint8_t volume);
    const String& get_speaker_name() const;
    void setCharacteristicValue(const char *value);
    void update();

    static bluetooth_controller *instance;
    bool setRemoteCharacteristicValue(const std::string& value);
    void registerForIndications();

    bool clientIsConnectedToServer() const;
    bool serverHasClientConnected() const;  // Add this new method
    void setBLEClientConnectionStatus(bool status);
    void setBLEServerConnectionStatus(bool status);  // Add this new method

private:
    BLEScan* pBLEScanner;
    BLEClient* pClient;
    BLECharacteristic* pCharacteristic;
    BLERemoteCharacteristic* pRemoteCharacteristic;

    void initializeBLEServer();
    void initializeBLEClient();
    void startScan(); // Add this line
    static int audio_callback_trampoline(Frame* frame, int frame_count);
    static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr);
    bool connectToServer();
    void handleBLEClient();

    bool m_isPrimary;
    String m_speaker_name;
    std::function<int32_t(Frame*, int32_t)> audio_provider_callback;
    unsigned long last_reconnection_attempt;
    BluetoothA2DPSource a2dp_source;

    static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void handleIndication(const std::string& value);

    bool indicationReceived;

    // Move this from being a static variable to a class member
    bool m_clientIsConnectedToServer;
    bool m_serverHasClientConnected;  // Add this new member variable
};

#endif // BLUETOOTH_CONTROLLER_H