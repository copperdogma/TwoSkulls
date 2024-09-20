#ifndef SKULL_COMMUNICATION_H
#define SKULL_COMMUNICATION_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "radio_manager.h"

enum class Message : int
{
    CONNECTION_REQUEST = 0, // Request to establish a connection
    CONNECTION_ACK = 1,     // Acknowledge a connection request
    // CAMKILL:
    //  KEEPALIVE = 2,          // Maintain connection
    //  KEEPALIVE_ACK = 3,      // Acknowledge a keepalive
    PLAY_FILE = 2,          // Instruct peer to play a specific file
    PLAY_FILE_ACK = 3 // Acknowledge a play file request
};

typedef struct struct_message
{
    Message message;
    char filename[32];
} struct_message;

extern struct_message myData;

typedef void (*MessageCallback)(const struct_message &msg);

class SkullCommunication
{
public:
    SkullCommunication(bool isPrimary, const String &macAddress, const String &otherMacAddress, RadioManager* radioManager);

    void begin();
    void update();
    void sendPlayCommand(const char *filename);
    bool isPeerConnected() const { return m_isPeerConnected; }
    unsigned long getLastHeardTime() const { return lastHeardTime; }

    void registerSendCallback(MessageCallback callback);
    void registerReceiveCallback(MessageCallback callback);

    void setPlayFileCallback(std::function<void(const char*)> callback) { playFileCallback = callback; }

private:
    static const unsigned long RADIO_ACCESS_TIMEOUT_MS = 200; // 200ms timeout for radio access

    bool isPrimary;
    uint8_t myMac[6];
    uint8_t otherSkullMac[6];
    bool isPeerAdded = false;
    bool m_isPeerConnected = false;
    unsigned long lastHeardTime = 0;
    unsigned long lastSentTime = 0;
    String m_audioFileToPlay;
    RadioManager* m_radioManager;

    void addPeer(const char *successMessage, const char *failureMessage);
    void sendMessage(Message message, const char *successMessage, const char *failureMessage);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printMacAddress(const uint8_t *macAddress, const char *description);

    static const int WIFI_CHANNEL = 2;
    static const int CONNECTION_RETRY_DELAY = 7000;       // 7 seconds
    static const unsigned long TIMEOUT_INTERVAL = 11000;  // 11 seconds
    static const unsigned long KEEPALIVE_INTERVAL = 5000; // 5 seconds

    static SkullCommunication *instance;

    MessageCallback onSendCallback = nullptr;
    MessageCallback onReceiveCallback = nullptr;
    std::function<void(bool)> enabledCallback;
    std::function<void(const char*)> playFileCallback;
};

#endif // SKULL_COMMUNICATION_H