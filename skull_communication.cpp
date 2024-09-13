#include "esp_wifi.h"
#include "skull_communication.h"
#include "skull_audio_animator.h"

extern SkullAudioAnimator *skullAudioAnimator;

SkullCommunication* SkullCommunication::instance = nullptr;

// Structure for messages
typedef struct struct_message
{
    Command command;          // Changed from int to Command
    char filename[32];
} struct_message;

struct_message myData;

SkullCommunication::SkullCommunication(bool isPrimary, const String &macAddress, const String &otherMacAddress)
    : isPrimary(isPrimary)
{
    instance = this;
    sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &myMac[0], &myMac[1], &myMac[2], &myMac[3], &myMac[4], &myMac[5]);
    sscanf(otherMacAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &otherSkullMac[0], &otherSkullMac[1], &otherSkullMac[2], &otherSkullMac[3], &otherSkullMac[4], &otherSkullMac[5]);
}

void SkullCommunication::begin()
{
    Serial.println("COMMS: Initializing...");
    WiFi.mode(WIFI_STA);
    Serial.println("COMMS: WiFi mode set to STATION");

    if (esp_wifi_set_mac(WIFI_IF_STA, myMac) == ESP_OK)
    {
        Serial.println("COMMS: Successfully set device MAC address");
    }
    else
    {
        Serial.println("COMMS: Failed to set device MAC address");
    }

    printMacAddress(myMac, "COMMS: This device's MAC Address: ");
    printMacAddress(otherSkullMac, "COMMS: Other skull's MAC Address: ");

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("COMMS: Error initializing ESP-NOW");
        return;
    }
    Serial.println("COMMS: ESP-NOW initialized successfully");

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);
    Serial.println("COMMS: Callbacks registered");

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, otherSkullMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) == ESP_OK)
    {
        Serial.println("COMMS: Peer added successfully");
        isPeerAdded = true;
    }
    else
    {
        Serial.println("COMMS: Failed to add peer");
    }

    Serial.println("COMMS: Initialization complete");
}

void SkullCommunication::update()
{
    unsigned long currentMillis = millis();

    if (!m_isPeerConnected)
    {
        if (isPrimary && currentMillis - lastKeepAlive > CONNECTION_RETRY_DELAY)
        {
            myData.command = Command::CONNECTION_REQUEST; // Changed from 2 to enum
            esp_err_t result = esp_now_send(otherSkullMac, reinterpret_cast<uint8_t*>(&myData), sizeof(myData));
            if (result == ESP_OK)
            {
                Serial.println("COMMS: Connection request sent");
            }
            else
            {
                Serial.println("COMMS: Failed to send connection request");
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
    if (!m_isPeerConnected)
    {
        Serial.println("COMMS: Cannot send play command, peer not connected");
        return;
    }

    myData.command = Command::PLAY_FILE; // Changed from 1 to enum
    strncpy(myData.filename, filename, sizeof(myData.filename));
    esp_err_t result = esp_now_send(otherSkullMac, reinterpret_cast<uint8_t*>(&myData), sizeof(myData));

    if (result == ESP_OK)
    {
        Serial.println("COMMS: Play command sent");
    }
    else
    {
        Serial.println("COMMS: Error sending play command");
    }
}

void SkullCommunication::sendKeepAlive()
{
    myData.command = Command::KEEPALIVE; // Changed from 0 to enum
    esp_err_t result = esp_now_send(otherSkullMac, reinterpret_cast<uint8_t*>(&myData), sizeof(myData));

    if (result == ESP_OK)
    {
        Serial.println("COMMS: Keepalive sent");
    }
    else
    {
        Serial.println("COMMS: Error sending keepalive");
    }
}

void SkullCommunication::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("COMMS: Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void SkullCommunication::onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    struct_message receivedData;
    memcpy(&receivedData, incomingData, sizeof(receivedData));

    switch (receivedData.command)
    {
    case Command::KEEPALIVE:
        Serial.println("COMMS: Received keepalive");
        break;
    case Command::CONNECTION_REQUEST:
        Serial.println("Received connection request");
        myData.command = Command::CONNECTION_ACK; // Changed from 3
        esp_now_send(mac, reinterpret_cast<uint8_t*>(&myData), sizeof(myData));
        instance->m_isPeerConnected = true;
        break;
    case Command::CONNECTION_ACK:
        Serial.println("COMMS: Connected! Received connection acknowledgment");
        instance->m_isPeerConnected = true;
        break;
    }
    case Command::PLAY_FILE:
        Serial.printf("COMMS: Received play command for file: %s\n", receivedData.filename);
        if (skullAudioAnimator)
        {
            skullAudioAnimator->playNext(receivedData.filename);
        }
        break;
}

void SkullCommunication::printMacAddress(const uint8_t* macAddress, const char* description) {
    Serial.print(description);
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}