#include "esp_wifi.h"
#include "skull_communication.h"
#include "skull_audio_animator.h"

extern SkullAudioAnimator *skullAudioAnimator;

SkullCommunication *SkullCommunication::instance = nullptr;
struct_message myData; // Added definition here

SkullCommunication::SkullCommunication(bool isPrimary, const String &macAddress, const String &otherMacAddress)
    : isPrimary(isPrimary)
{
    instance = this;
    sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &myMac[0], &myMac[1], &myMac[2], &myMac[3], &myMac[4], &myMac[5]);
    sscanf(otherMacAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &otherSkullMac[0], &otherSkullMac[1], &otherSkullMac[2], &otherSkullMac[3], &otherSkullMac[4], &otherSkullMac[5]);
}

void SkullCommunication::registerSendCallback(MessageCallback callback)
{
    onSendCallback = callback;
}

void SkullCommunication::registerReceiveCallback(MessageCallback callback)
{
    onReceiveCallback = callback;
}

void SkullCommunication::begin()
{
    Serial.printf("COMMS: Initializing as %s...\n", isPrimary ? "PRIMARY" : "SECONDARY");
    WiFi.mode(WIFI_STA);
    Serial.println("COMMS: WiFi mode set to STATION");

    // If experiencing brownouts, try this. (untested)
    // esp_wifi_set_max_tx_power(8); // Set to minimum power (8dBm)

    if (esp_wifi_set_mac(WIFI_IF_STA, myMac) == ESP_OK)

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

    // This callback could be useful for debugging but I found it decided there were a LOT of send failures that didn't seem correct.
    // esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);

    addPeer("Peer added successfully", "Failed to add peer");

    Serial.println("COMMS: Initialization complete");
}

void SkullCommunication::addPeer(const char *successMessage, const char *failureMessage)
{
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, otherSkullMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) == ESP_OK)
    {
        Serial.printf("COMMS: %s\n", successMessage);
    }
    else
    {
        Serial.printf("COMMS: %s\n", failureMessage);
    }
}

void SkullCommunication::update()
{
    // If not primary, do nothing. Secondary just passively listens for messages.
    if (!isPrimary)
    {
        return;
    }

    unsigned long currentMillis = millis();

    // Check for timeout
    if (m_isPeerConnected && (currentMillis - lastHeardTime > TIMEOUT_INTERVAL))
    {
        m_isPeerConnected = false;
        Serial.println("COMMS: Lost connection due to lastHeardTime timeout");
    }

    // Attempt reconnection if not connected
    if (!m_isPeerConnected && (currentMillis - lastSentTime > CONNECTION_RETRY_DELAY))
    {
        sendMessage(Message::CONNECTION_REQUEST, "CONNECTION_REQUEST sent", "Failed to send CONNECTION_REQUEST");
    }

    // Send KEEPALIVE if idle
    if (m_isPeerConnected && (currentMillis - lastSentTime > KEEPALIVE_INTERVAL))
    {
        sendMessage(Message::KEEPALIVE, "KEEPALIVE sent", "Failed to send KEEPALIVE");
    }
}

void SkullCommunication::sendMessage(Message message, const char *successMessage, const char *failureMessage)
{
    if (!esp_now_is_peer_exist(otherSkullMac))
    {
        Serial.println("COMMS: Peer not in list, attempting to re-add");
        addPeer("Peer re-added successfully", "Failed to re-add peer");
    }

    myData.message = message;
    esp_err_t result = esp_now_send(otherSkullMac, reinterpret_cast<uint8_t *>(&myData), sizeof(myData));
    lastSentTime = millis();
    if (result == ESP_OK)
    {
        Serial.printf("COMMS: %s\n", successMessage);
        if (onSendCallback)
        {
            onSendCallback(myData);
        }
    }
    else
    {
        Serial.printf("COMMS: %s\n", failureMessage);
    }
}

void SkullCommunication::sendPlayCommand(const char *filename)
{
    if (!isPrimary)
    {
        Serial.println("COMMS: Cannot send play command, not primary skull");
        return;
    }

    if (!m_isPeerConnected)
    {
        Serial.println("COMMS: Cannot send play command, peer not connected");
        return;
    }

    if (!esp_now_is_peer_exist(otherSkullMac))
    {
        Serial.println("COMMS: Peer not in list, attempting to re-add");
        addPeer("Peer re-added successfully", "Failed to re-add peer");
    }

    myData.message = Message::PLAY_FILE;
    strncpy(myData.filename, filename, sizeof(myData.filename));
    esp_err_t result = esp_now_send(otherSkullMac, reinterpret_cast<uint8_t *>(&myData), sizeof(myData));
    lastSentTime = millis();

    if (result == ESP_OK)
    {
        Serial.println("COMMS: Play command sent");
        if (onSendCallback)
        {
            onSendCallback(myData);
        }
    }
    else
    {
        Serial.println("COMMS: Error sending play command");
    }
}

void SkullCommunication::onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    instance->m_isPeerConnected = true; // Ensure connection is marked as active
    instance->lastHeardTime = millis(); // Update last heard time
    instance->sendFailures = 0;         // Reset failure count on successful receive

    struct_message receivedData;
    memcpy(&receivedData, incomingData, sizeof(receivedData));

    // Primary responsible for initializing connection, sending keepalives, detecting disconnections,
    // and telling secondary what to play.
    // Primary should only ever RECEIVE acks.
    if (instance->isPrimary)
    {
        switch (receivedData.message)
        {
        case Message::CONNECTION_REQUEST:
            Serial.println("COMMS: WARNING: Received CONNECTION_REQUEST despite being PRIMARY. Should never happen.");
            break;
        case Message::CONNECTION_ACK:
            Serial.println("COMMS: Connected! Received CONNECTION_ACK");
            break;
        case Message::KEEPALIVE:
            Serial.println("COMMS: WARNING: Received KEEPALIVE despite being PRIMARY. Should never happen.");
            break;
        case Message::KEEPALIVE_ACK:
            Serial.println("COMMS: Received KEEPALIVE_ACK");
            break;
        case Message::PLAY_FILE:
            Serial.println("COMMS: WARNING: Received PLAY_FILE despite being PRIMARY. Should never happen.");
            break;
        default:
            Serial.printf("COMMS: Received unknown message: %d\n", receivedData.message);
            break;
        }
    }

    // Secondary skull only sends keepalives, acks, and plays audio on demand.
    else
    {
        switch (receivedData.message)
        {
        case Message::CONNECTION_REQUEST:
        {
            Serial.println("COMMS: Received CONNECTION_REQUEST");

            instance->sendMessage(Message::CONNECTION_ACK, "CONNECTION_ACK sent", "Failed to send CONNECTION_ACK");
            break;
        }
        case Message::CONNECTION_ACK:
            Serial.println("COMMS: WARNING: Received CONNECTION_ACK despite being SECONDARY. Should never happen.");
            break;
        case Message::KEEPALIVE:
        {
            Serial.println("COMMS: Received KEEPALIVE");

            instance->sendMessage(Message::KEEPALIVE_ACK, "KEEPALIVE_ACK sent", "Failed to send KEEPALIVE_ACK");
            break;
        }
        case Message::KEEPALIVE_ACK:
            Serial.println("COMMS: WARNING: Received KEEPALIVE_ACK despite being SECONDARY. Should never happen.");
            break;
        case Message::PLAY_FILE:
            // Only secondary skull should be told to play anything. The primary decides what to play.
            Serial.printf("COMMS: Received play command for file: %s\n", receivedData.filename);
            if (skullAudioAnimator)
            {
                skullAudioAnimator->playNext(receivedData.filename);
            }
            break;
        default:
            Serial.printf("COMMS: Received unknown message: %d\n", receivedData.message);
            break;
        }
    }

    // After processing the message
    if (instance->onReceiveCallback)
    {
        instance->onReceiveCallback(receivedData);
    }
}

void SkullCommunication::printMacAddress(const uint8_t *macAddress, const char *description)
{
    Serial.print(description);
    for (int i = 0; i < 6; i++)
    {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5)
            Serial.print(":");
    }
    Serial.println();
}