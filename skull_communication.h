#ifndef SKULL_COMMUNICATION_H
#define SKULL_COMMUNICATION_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

enum class Command : int {
    KEEPALIVE = 0,          // Maintain connection
    CONNECTION_REQUEST = 1, // Request to establish a connection
    CONNECTION_ACK = 2,     // Acknowledge a connection request
    PLAY_FILE = 3           // Instruct peer to play a specific file
};

class SkullCommunication {
public:
    SkullCommunication(bool isPrimary, const String& macAddress, const String& otherMacAddress);
    void begin();
    void update();
    void sendPlayCommand(const char* filename);
    bool isPeerConnected() const { return m_isPeerConnected; }

private:
    bool isPrimary;
    uint8_t myMac[6];
    uint8_t otherSkullMac[6];
    unsigned long lastKeepAlive = 0;
    bool isPeerAdded = false;
    bool m_isPeerConnected = false;
    const unsigned long KEEPALIVE_INTERVAL = 5000; // 5 seconds

    void sendKeepAlive();
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printMacAddress(const uint8_t* macAddress, const char* description);

    static const int MAX_CONNECTION_ATTEMPTS = 5;
    static const int CONNECTION_RETRY_DELAY = 5000; // 5 seconds

    static SkullCommunication* instance;
};

#endif // SKULL_COMMUNICATION_H