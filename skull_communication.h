#ifndef SKULL_COMMUNICATION_H
#define SKULL_COMMUNICATION_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

class SkullCommunication {
public:
    SkullCommunication(bool isPrimary, const String& macAddress, const String& otherMacAddress);
    void begin();
    void update();
    void sendPlayCommand(const char* filename);
    bool isPeerConnected() const { return m_isPeerConnected; }  // Add this line

private:
    bool isPrimary;
    uint8_t myMac[6];
    uint8_t otherSkullMac[6];
    unsigned long lastKeepAlive = 0;
    bool isPeerAdded = false;
    bool m_isPeerConnected = false;  // Change this line
    const unsigned long KEEPALIVE_INTERVAL = 5000; // 5 seconds

    void sendKeepAlive();
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printMacAddress(const uint8_t* macAddress, const char* description);

    static const int MAX_CONNECTION_ATTEMPTS = 5;
    static const int CONNECTION_RETRY_DELAY = 5000; // 5 seconds

    static SkullCommunication* instance;  // Add this line
};

#endif // SKULL_COMMUNICATION_H