#ifndef BLUETOOTH_CONTROLLER_H
#define BLUETOOTH_CONTROLLER_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"  // This should include the Frame struct definition
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <functional>
#include <string>

// Add this near the top of the file, after the includes
enum class ConnectionState {
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    CONNECTED
};

class bluetooth_controller {
public:
    bluetooth_controller();
    void begin(const String& speaker_name, std::function<int32_t(Frame*, int32_t)> audioProviderCallback, bool isPrimary);
    void set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void* obj));
    bool isA2dpConnected();
    void set_volume(uint8_t volume);
    const String& get_speaker_name() const;
    void setCharacteristicValue(const char *value);
    void update();  // Add this line

    static bluetooth_controller *instance;
    bool setRemoteCharacteristicValue(const std::string& value);
    bool registerForIndications();  // Changed from void to bool

    bool clientIsConnectedToServer() const;
    bool serverHasClientConnected() const;  // Add this new method
    void setBLEClientConnectionStatus(bool status);
    void setBLEServerConnectionStatus(bool status);  // Add this new method

    // Make these methods public
    void startScan();
    bool connectToServer();
    ConnectionState getConnectionState() const { return m_connectionState; }
    void setConnectionState(ConnectionState state) { m_connectionState = state; }

    void setMyDevice(BLEAdvertisedDevice* device) { myDevice = device; }

private:
    BLEScan* pBLEScanner;
    BLEClient* pClient;
    BLECharacteristic* pCharacteristic;
    BLERemoteCharacteristic* pRemoteCharacteristic;

    void initializeBLEServer();
    void initializeBLEClient();
    static int audio_callback_trampoline(Frame* frame, int frame_count);
    static void connection_state_changed(esp_a2d_connection_state_t state, void* ptr);
    void handleBLEClient();

    bool m_isPrimary;
    String m_speaker_name;
    std::function<int32_t(Frame*, int32_t)> audio_provider_callback;
    unsigned long last_reconnection_attempt;
    BluetoothA2DPSource a2dp_source;

    static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void handleIndication(const std::string& value);

    bool indicationReceived;
    bool m_clientIsConnectedToServer;
    bool m_serverHasClientConnected;  // Add this new member variable

    ConnectionState m_connectionState;
    unsigned long m_lastReconnectAttempt;
    unsigned long connectionStartTime;  // Add this line

    void handleConnectionState();
    void attemptReconnection();
    void disconnectFromServer();

    static BLEAdvertisedDevice* myDevice;
    unsigned long scanStartTime;

    std::string getConnectionStateString(ConnectionState state);
};

#endif // BLUETOOTH_CONTROLLER_H