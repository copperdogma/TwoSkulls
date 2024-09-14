#ifndef SKULL_COMMUNICATION_H
#define SKULL_COMMUNICATION_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

enum class Message : int
{
    CONNECTION_REQUEST = 0, // Request to establish a connection
    CONNECTION_ACK = 1,     // Acknowledge a connection request
    KEEPALIVE = 2,          // Maintain connection
    KEEPALIVE_ACK = 3,      // Acknowledge a keepalive
    PLAY_FILE = 4           // Instruct peer to play a specific file
};

class SkullCommunication
{
public:
    SkullCommunication(bool isPrimary, const String &macAddress, const String &otherMacAddress);

    void begin();
    void update();
    void sendPlayCommand(const char *filename);
    bool isPeerConnected() const { return m_isPeerConnected; }

private:
    bool isPrimary;
    uint8_t myMac[6];
    uint8_t otherSkullMac[6];
    bool isPeerAdded = false;
    bool m_isPeerConnected = false;
    unsigned long lastHeardTime = 0;
    unsigned long lastSentTime = 0;
    int sendFailures = 0;

    void addPeer(const char *successMessage, const char *failureMessage);
    void sendMessage(Message message, const char *successMessage, const char *failureMessage);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
    void printMacAddress(const uint8_t *macAddress, const char *description);

    static const int CONNECTION_RETRY_DELAY = 7000; // 7 seconds
    static const unsigned long TIMEOUT_INTERVAL = 10000; // 10 seconds
    static const unsigned long KEEPALIVE_INTERVAL = 5000; // 5 seconds
    static const int MAX_FAILURES = 3;

    static SkullCommunication *instance;
};

#endif // SKULL_COMMUNICATION_H