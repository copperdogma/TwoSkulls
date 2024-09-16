/*
    This is the main communication class for the skulls. It handles all the low-level communication
    between the primary and secondary skulls.

    Communication protocol:
    - Primary sends Secondary a CONNECTION_REQUEST message, Secondary sends back a CONNECTION_ACK message.
    - Primary sends Secondary a PLAY_FILE message, Secondary sends back a PLAY_FILE_ACK message.

    The idea is the Primary and Secondary will start playing the same audio file simultaneously.

    NOTE: This module will not send or receive while audio is playing. A2DP (bluetooth audio) and wifi
          share the radio and don't play well together.
*/
#include "esp_wifi.h"
#include "skull_communication.h"

SkullCommunication *SkullCommunication::instance = nullptr;
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

    // Initialize Wi-Fi as station. Most compatible with Bluetooth, especially when sharing the radio.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Disconnect from any previous network

    // Set the Wi-Fi channel
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Start the Wi-Fi driver
    esp_err_t wifi_start_result = esp_wifi_start();
    if (wifi_start_result != ESP_OK)
    {
        Serial.printf("COMMS: Failed to start Wi-Fi (code: %d)\n", wifi_start_result);
        return;
    }
    Serial.println("COMMS: Wi-Fi started successfully");

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("COMMS: Error initializing ESP-NOW");
        return;
    }
    Serial.println("COMMS: ESP-NOW initialized successfully");

    // After esp_wifi_start(), reapply the channel setting
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);

    addPeer("Peer added successfully", "Failed to add peer");

    Serial.println("COMMS: Initialization complete");
}

void SkullCommunication::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        Serial.println("COMMS: Message sent successfully");
    }
    else
    {
        Serial.printf("COMMS: Message send failed with status: %d\n", status);
    }
}

void SkullCommunication::addPeer(const char *successMessage, const char *failureMessage)
{
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, otherSkullMac, 6);
    peerInfo.channel = WIFI_CHANNEL;
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

    // CAMKILL:
    //  // Check for timeout
    //  if (m_isPeerConnected && (currentMillis - lastHeardTime > TIMEOUT_INTERVAL))
    //  {
    //      m_isPeerConnected = false;
    //      Serial.println("COMMS: Lost connection due to lastHeardTime timeout");
    //  }

    // Attempt reconnection if not connected
    if (!m_isPeerConnected && (currentMillis - lastSentTime > CONNECTION_RETRY_DELAY))
    {
        sendMessage(Message::CONNECTION_REQUEST, "CONNECTION_REQUEST sent", "Failed to send CONNECTION_REQUEST");
    }

    // CAMKILL:
    //  // Send KEEPALIVE if idle
    //  if (m_isPeerConnected && (currentMillis - lastSentTime > KEEPALIVE_INTERVAL))
    //  {
    //      sendMessage(Message::KEEPALIVE, "KEEPALIVE sent", "Failed to send KEEPALIVE");
    //  }

    // If peer is not in the list, try to add it
    if (!esp_now_is_peer_exist(otherSkullMac))
    {
        addPeer("Peer re-added successfully", "Failed to re-add peer");
    }
}

void SkullCommunication::sendMessage(Message message, const char *successMessage, const char *failureMessage)
{
    if (!esp_now_is_peer_exist(otherSkullMac))
    {
        Serial.println("COMMS: Peer not in list, cannot send message");
        return; // Exit the function instead of trying to re-add
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
        Serial.printf("COMMS: %s (code: %d)\n", failureMessage, result);
        // CAMKILL:m_isPeerConnected = false; // Mark as disconnected on send failure
        //  Optionally, trigger a reconnection attempt here
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
        Serial.println("COMMS: Peer not in list, cannot send play command");
        return; // Exit the function instead of trying to re-add
    }

    m_audioFileToPlay = filename;

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
            // CAMKILL:
        // case Message::KEEPALIVE:
        //     Serial.println("COMMS: WARNING: Received KEEPALIVE despite being PRIMARY. Should never happen.");
        //     break;
        // case Message::KEEPALIVE_ACK:
        //     Serial.println("COMMS: Received KEEPALIVE_ACK");
        //     break;
        case Message::PLAY_FILE:
            Serial.println("COMMS: WARNING: Received PLAY_FILE despite being PRIMARY. Should never happen.");
            break;
        case Message::PLAY_FILE_ACK:
            if (instance->playFileCallback) {
                instance->playFileCallback(instance->m_audioFileToPlay.c_str());
            }
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
            // CAMKILL:
        // case Message::KEEPALIVE:
        // {
        //     Serial.println("COMMS: Received KEEPALIVE");

        //     instance->sendMessage(Message::KEEPALIVE_ACK, "KEEPALIVE_ACK sent", "Failed to send KEEPALIVE_ACK");
        //     break;
        // }
        // case Message::KEEPALIVE_ACK:
        //     Serial.println("COMMS: WARNING: Received KEEPALIVE_ACK despite being SECONDARY. Should never happen.");
        //     break;
        // case Message::PLAY_FILE:
        //     // Only secondary skull should be told to play anything. The primary decides what to play.
        //     Serial.printf("COMMS: Received play command for file: %s\n", receivedData.filename);

        //     if (skullAudioAnimator)
        //     {
        //         skullAudioAnimator->playNext(receivedData.filename);
        //     }
        //     break;
        case Message::PLAY_FILE:
            // Only secondary skull should be told to play anything. The primary decides what to play.
            Serial.printf("COMMS: Received play command for file: %s\n", receivedData.filename);

            instance->sendMessage(Message::PLAY_FILE_ACK, "PLAY_FILE_ACK sent", "Failed to send PLAY_FILE_ACK");

            if (instance->playFileCallback) {
                instance->playFileCallback(instance->m_audioFileToPlay.c_str());
            }
            break;
        case Message::PLAY_FILE_ACK:
            Serial.println("COMMS: WARNING: Received PLAY_FILE_ACK despite being SECONDARY. Should never happen.");
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