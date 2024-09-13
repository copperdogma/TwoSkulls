#include "esp_wifi.h"
#include "skull_communication.h"
#include "skull_audio_animator.h"

extern SkullAudioAnimator *skullAudioAnimator;

SkullCommunication* SkullCommunication::instance = nullptr;  // Add this line

// Structure for messages
typedef struct struct_message
{
    int command; // 0: keepalive, 1: play file, 2: connection request, 3: connection ack
    char filename[32];
} struct_message;

struct_message myData;

SkullCommunication::SkullCommunication(bool isPrimary, const String &macAddress, const String &otherMacAddress)
    : isPrimary(isPrimary)
{
    instance = this;  // Add this line
    sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &myMac[0], &myMac[1], &myMac[2], &myMac[3], &myMac[4], &myMac[5]);
    sscanf(otherMacAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &otherSkullMac[0], &otherSkullMac[1], &otherSkullMac[2], &otherSkullMac[3], &otherSkullMac[4], &otherSkullMac[5]);
}

void SkullCommunication::begin()
{
    Serial.println("SkullCommunication: Initializing...");
    WiFi.mode(WIFI_STA);
    Serial.println("SkullCommunication: WiFi mode set to STATION");

    if (esp_wifi_set_mac(WIFI_IF_STA, myMac) == ESP_OK)
    {
        Serial.println("SkullCommunication: Successfully set device MAC address");
    }
    else
    {
        Serial.println("SkullCommunication: Failed to set device MAC address");
    }

    printMacAddress(myMac, "SkullCommunication: This device's MAC Address: ");
    printMacAddress(otherSkullMac, "SkullCommunication: Other skull's MAC Address: ");

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("SkullCommunication: Error initializing ESP-NOW");
        return;
    }
    Serial.println("SkullCommunication: ESP-NOW initialized successfully");

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);
    Serial.println("SkullCommunication: Callbacks registered");

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, otherSkullMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) == ESP_OK)
    {
        Serial.println("SkullCommunication: Peer added successfully");
        isPeerAdded = true;
    }
    else
    {
        Serial.println("SkullCommunication: Failed to add peer");
    }

    Serial.println("SkullCommunication: Initialization complete");
}

void SkullCommunication::update()
{
    unsigned long currentMillis = millis();

    if (!m_isPeerConnected)
    {
        if (isPrimary && currentMillis - lastKeepAlive > CONNECTION_RETRY_DELAY)
        {
            myData.command = 2; // Connection request
            esp_err_t result = esp_now_send(otherSkullMac, (uint8_t *)&myData, sizeof(myData));
            if (result == ESP_OK)
            {
                Serial.println("SkullCommunication: Connection request sent");
            }
            else
            {
                Serial.println("SkullCommunication: Failed to send connection request");
            }
            lastKeepAlive = currentMillis;
        }
    }
    else if (isPrimary)
    {
        if (currentMillis - lastKeepAlive > KEEPALIVE_INTERVAL)
        {
            sendKeepAlive();
            lastKeepAlive = currentMillis;
        }
    }
}

void SkullCommunication::sendPlayCommand(const char *filename)
{
    if (!m_isPeerConnected)  // Change this line
    {
        Serial.println("SkullCommunication: Cannot send play command, peer not connected");
        return;
    }

    myData.command = 1;
    strncpy(myData.filename, filename, sizeof(myData.filename));
    esp_err_t result = esp_now_send(otherSkullMac, (uint8_t *)&myData, sizeof(myData));

    if (result == ESP_OK)
    {
        Serial.println("SkullCommunication: Play command sent");
    }
    else
    {
        Serial.println("SkullCommunication: Error sending play command");
    }
}

void SkullCommunication::sendKeepAlive()
{
    myData.command = 0;
    esp_err_t result = esp_now_send(otherSkullMac, (uint8_t *)&myData, sizeof(myData));

    if (result == ESP_OK)
    {
        Serial.println("SkullCommunication: Keepalive sent");
    }
    else
    {
        Serial.println("SkullCommunication: Error sending keepalive");
    }
}

void SkullCommunication::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void SkullCommunication::onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    struct_message receivedData;
    memcpy(&receivedData, incomingData, sizeof(receivedData));

    switch (receivedData.command)
    {
    case 0: // Keepalive
        Serial.println("Received keepalive");
        break;
    case 1: // Play file
        Serial.printf("Received play command for file: %s\n", receivedData.filename);
        if (skullAudioAnimator)
        {
            skullAudioAnimator->playNext(receivedData.filename);
        }
        break;
    case 2: // Connection request
        Serial.println("Received connection request");
        myData.command = 3; // Connection ack
        esp_now_send(mac, (uint8_t *)&myData, sizeof(myData));
        instance->m_isPeerConnected = true;
        break;
    case 3: // Connection ack
        Serial.println("Received connection acknowledgment");
        instance->m_isPeerConnected = true;
        break;
    }
}

void SkullCommunication::printMacAddress(const uint8_t* macAddress, const char* description) {
    Serial.print(description);
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}