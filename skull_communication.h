#ifndef SKULL_COMMUNICATION_H
#define SKULL_COMMUNICATION_H

#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>  // For String

class SkullCommunication {
public:
    SkullCommunication(bool isPrimary, const String& macAddress, const String& otherMacAddress);
    void begin();
    void update();
    void sendPlayCommand(const char* filename);

private:
    bool isPrimary;
    bool isConnected = false;
    unsigned long lastKeepAlive = 0;
    const unsigned long KEEPALIVE_INTERVAL = 5000; // 5 seconds

    uint8_t myMac[6];
    uint8_t otherSkullMac[6];

    void sendKeepAlive();
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printMacAddress(const uint8_t* macAddress, const char* description);  // Added return type 'void'
};

#endif // SKULL_COMMUNICATION_H