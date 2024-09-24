/*
    Bluetooth Controller Class

    This class manages both A2DP audio streaming and BLE skull-to-skull communication for an ESP32-WROVER platform.
    It handles the following main functionalities:
    1. A2DP audio streaming to a Bluetooth speaker
    2. BLE communication between two skull devices (primary and secondary)

    The class can operate in two modes:
    - Primary mode: Acts as a BLE client, connecting to a secondary skull
    - Secondary mode: Acts as a BLE server, waiting for a primary skull to connect

    Note: Ideally, this should be split into two separate classes for better separation of concerns.

    Dependencies:
    - ESP32 BLE and A2DP libraries
    - SoundData.h from the ESP32-A2DP library (for the Frame struct used in A2DP streaming)

    Note: Frame (for A2DP audio streaming) is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

    struct __attribute__((packed)) Frame {
        int16_t channel1;
        int16_t channel2;

        Frame(int v=0){
            channel1 = channel2 = v;
        }

        Frame(int ch1, int ch2){
            channel1 = ch1;
            channel2 = ch2;
        }
    };
*/

#include "bluetooth_controller.h"
#include <cstring>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "nvs_flash.h"

// BLE-related includes and definitions
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#define SERVER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Constants for BLE scanning and connection
static const unsigned long SCAN_INTERVAL = 10000;      // 10 seconds between scan attempts
static const unsigned long SCAN_DURATION = 10000;      // 10 seconds scan duration
static const unsigned long CONNECTION_TIMEOUT = 30000; // 30 seconds connection timeout


// Global variables for BLE communication
BLECharacteristic *pCharacteristic;
char title[160] = {"Skull characteristic value!"}; // Stores the current title/value of the BLE characteristic

// Connection status flags
static bool serverHasClientConnected = false;                  // Indicates if the BLE server (secondary skull) has a connected client
static boolean clientShouldConnect = false;                    // Flag to initiate client connection attempt
static boolean clientIsConnectedToServer = false;              // Indicates if the BLE client (primary skull) is connected to a server
BLEAdvertisedDevice *bluetooth_controller::myDevice = nullptr; // Stores the discovered BLE server device

// BLE scanning-related variables
static BLEScan *pBLEScan = nullptr; // BLE scanner object
static bool isScanning = false;     // Flag to track if BLE scanning is in progress

// Callback class for handling BLE characteristic writes
class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        // This method is called when a client writes to the characteristic
        // It prints the new value to the Serial monitor for debugging
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            Serial.println("*********");
            Serial.print("New value: ");
            for (int i = 0; i < value.length(); i++)
            {
                Serial.print(value[i]);
            }
            Serial.println();
            Serial.println("*********");
        }
    }
};

// Callback class for handling BLE server events (connect/disconnect)
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
    {
        // This method is called when a client connects to the BLE server
        if (bluetooth_controller::instance)
        {
            bluetooth_controller::instance->setBLEServerConnectionStatus(true);
            bluetooth_controller::instance->setConnectionState(ConnectionState::CONNECTED);
        }
        Serial.println("BT-BLE: Client connected!");
        // TODO: Implement logging of client address and connection details
    }

    void onDisconnect(BLEServer *pServer)
    {
        // This method is called when a client disconnects from the BLE server
        if (bluetooth_controller::instance)
        {
            bluetooth_controller::instance->setBLEServerConnectionStatus(false);
            bluetooth_controller::instance->setConnectionState(ConnectionState::DISCONNECTED);
        }
        Serial.println("BT-BLE: Client disconnected");

        // Restart advertising to allow new connections
        BLEDevice::startAdvertising();
        Serial.println("BT-BLE: Restarted advertising after disconnection");
    }
};

// Callback function for AVRC metadata
void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    // This function handles AVRC (Audio/Video Remote Control) metadata updates
    // It's primarily used to update the title of the currently playing audio
    Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
    if (id == ESP_AVRC_MD_ATTR_TITLE)
    {
        strncpy(title, (const char *)text, 160);
        pCharacteristic->setValue(title);
    }
}

// Static instance pointer initialization
bluetooth_controller *bluetooth_controller::instance = nullptr;

// Constructor
bluetooth_controller::bluetooth_controller()
    : m_speaker_name(""),
      m_clientIsConnectedToServer(false),
      m_serverHasClientConnected(false),
      m_connectionState(ConnectionState::DISCONNECTED),
      m_lastReconnectAttempt(0),
      scanStartTime(0),
      connectionStartTime(0)
{
    instance = this; // Ensure proper initialization of the static instance
}

// Set the value of the BLE characteristic
void bluetooth_controller::setCharacteristicValue(const char *value)
{
    pCharacteristic->setValue(value);
}

// Initialize the Bluetooth controller
void bluetooth_controller::begin(const String &speaker_name, std::function<int32_t(Frame *, int32_t)> audioProviderCallback, bool isPrimary)
{
    Serial.println("BT: Initializing Bluetooth...");

    m_isPrimary = isPrimary;
    m_speaker_name = speaker_name;
    audio_provider_callback = audioProviderCallback;

    // Start A2DP (audio streaming)
    Serial.printf("BT-A2DP: Starting as A2DP source, connecting to speaker name: %s\n", m_speaker_name.c_str());

    a2dp_source.set_default_bt_mode(ESP_BT_MODE_BTDM); // Essential to use A2DP and BLE at the same time
    a2dp_source.set_auto_reconnect(true);
    a2dp_source.set_on_connection_state_changed(connection_state_changed, this);
    a2dp_source.start(m_speaker_name.c_str(), audio_callback_trampoline);

    // Initialize BLE based on whether this is the primary or secondary skull
    if (m_isPrimary)
    {
        initializeBLEClient();
    }
    else
    {
        initializeBLEServer();
    }

    Serial.println("BT: Bluetooth initialization complete.");
}

/*
    Initialize the BLE server (for secondary skull).

    Note: This service/characteristic comes up (correctly) in a BLE scanner as:

        **Advertised Services**
        Unknown Service    -- the service itself
        UUID: 4FAFC201-1FB5-459E-8FCC-C5C9C331914B

        **Attribute Table**
        Unknown Service  -- generic access service (standard Bluetooth service)
        UUID: 1800
        PRIMARY SERVICE

        Unknown Service  -- generic attribute service (standard Bluetooth service)
        UUID: 1801
        PRIMARY SERVICE

        Unknown Service  -- the service itself
        UUID: 4FAFC201-1FB5-459E-8FCC-C5C9C331914B
        PRIMARY SERVICE

        Unknown Characteristic  -- the characteristic itself
        UUID: BEB5483E-36E1-4688-B7F5-EA07361B26A8
        Properties: Read, Write, and Indicate
        Value: Hello from SkullSecondary
        Value Sent: Testing can

        Unknown Descriptor  -- the Client Characteristic Configuration Descriptor (CCCD) we added for indications
        UUID: 2902
        Value: Disabled
        Value Sent: N/A

        ---

        Device Name: **SkullSecondary-Server**

*/
void bluetooth_controller::initializeBLEServer()
{
    Serial.println("BT-BLE: Starting as BLE SECONDARY (server)");

    // Initialize BLE device, create server and service
    BLEDevice::init("SkullSecondary-Server");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVER_SERVICE_UUID);

    // Create characteristic with read, write, and indicate properties
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_INDICATE);

    pCharacteristic->setValue("Hello from SkullSecondary");
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Add descriptor for indications
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    // Set up advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BT-BLE: Single characteristic defined! Now you can read/write/receive indications from the Primary skull!");
}

// Callback class for BLE client events
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        Serial.println("BT-BLE: Client connected callback triggered");
        if (bluetooth_controller::instance)
        {
            bluetooth_controller::instance->setBLEClientConnectionStatus(true);
        }
    }

    void onDisconnect(BLEClient *pclient)
    {
        Serial.println("BT-BLE: Client disconnected callback triggered");
        if (bluetooth_controller::instance)
        {
            bluetooth_controller::instance->setBLEClientConnectionStatus(false);
        }
    }
};

// Main update function for the Bluetooth controller
void bluetooth_controller::update()
{
    if (m_isPrimary)
    {
        static unsigned long lastStatusUpdate = 0;
        unsigned long currentTime = millis();

        switch (m_connectionState)
        {
        case ConnectionState::DISCONNECTED:
            if (currentTime - m_lastReconnectAttempt > SCAN_INTERVAL)
            {
                m_lastReconnectAttempt = currentTime;
                startScan();
            }
            break;

        case ConnectionState::SCANNING:
            // Scanning is handled in the BLEAdvertisedDeviceCallbacks
            if (!isScanning && myDevice != nullptr)
            {
                m_connectionState = ConnectionState::CONNECTING;
                connectionStartTime = currentTime;
            }
            break;

        case ConnectionState::CONNECTING:
            if (currentTime - connectionStartTime > CONNECTION_TIMEOUT)
            {
                Serial.println("BT-BLE: Connection attempt timed out. Restarting scan.");
                disconnectFromServer();
                m_connectionState = ConnectionState::DISCONNECTED;
            }
            else if (connectToServer())
            {
                m_connectionState = ConnectionState::CONNECTED;
            }
            break;

        case ConnectionState::CONNECTED:
            if (!m_clientIsConnectedToServer || (pClient && !pClient->isConnected()))
            {
                Serial.println("BT-BLE: Connection lost. Moving to DISCONNECTED state.");
                disconnectFromServer();
                m_connectionState = ConnectionState::DISCONNECTED;
            }
            break;
        }

        // Periodic status update
        if (currentTime - lastStatusUpdate > 30000)
        {
            Serial.printf("BT-BLE: Current connection state: %s\n",
                          getConnectionStateString(m_connectionState).c_str());
            Serial.printf("BT-BLE: Client connected: %s, Server has client: %s\n",
                          m_clientIsConnectedToServer ? "true" : "false",
                          m_serverHasClientConnected ? "true" : "false");
            lastStatusUpdate = currentTime;
        }
    }
}

// Check if the server is still advertising
bool bluetooth_controller::isServerAdvertising()
{
    if (myDevice == nullptr)
    {
        return false;
    }

    BLEScanResults scanResults = BLEDevice::getScan()->start(1);
    for (int i = 0; i < scanResults.getCount(); i++)
    {
        BLEAdvertisedDevice device = scanResults.getDevice(i);
        if (device.getAddress().equals(myDevice->getAddress()))
        {
            return true;
        }
    }
    return false;
}

// Callback class for handling advertised devices during scanning
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
public:
    MyAdvertisedDeviceCallbacks(bluetooth_controller *controller) : m_controller(controller) {}

    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVER_SERVICE_UUID)))
        {
            Serial.print("BT-BLE: Found our server: ");
            Serial.println(advertisedDevice.toString().c_str());
            m_controller->setMyDevice(new BLEAdvertisedDevice(advertisedDevice));
            m_controller->setConnectionState(ConnectionState::CONNECTING);
            BLEDevice::getScan()->stop();
        }
    }

private:
    bluetooth_controller *m_controller;
};

// Start BLE scanning
void bluetooth_controller::startScan()
{
    if (isScanning)
    {
        Serial.println("BT-BLE: Already scanning, stopping current scan");
        pBLEScan->stop();
        delay(100); // Give some time for the scan to stop
    }

    Serial.println("BT-BLE: Starting scan...");
    m_connectionState = ConnectionState::SCANNING;

    if (pBLEScan == nullptr)
    {
        pBLEScan = BLEDevice::getScan();
    }
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(this));
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);

    isScanning = true;
    bool scanStarted = pBLEScan->start(SCAN_DURATION, [](BLEScanResults results)
                                       {
        Serial.println("BT-BLE: Scan completed");
        isScanning = false; }, false);

    if (scanStarted)
    {
        Serial.println("BT-BLE: Scan started successfully");
    }
    else
    {
        Serial.println("BT-BLE: Failed to start scan");
        isScanning = false;
    }
}

// Connect to the BLE server
bool bluetooth_controller::connectToServer()
{
    if (myDevice == nullptr)
    {
        Serial.println("BT-BLE: No device to connect to.");
        return false;
    }

    Serial.print("BT-BLE: Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    Serial.println("BT-BLE: Created client");

    pClient->setClientCallbacks(new MyClientCallback());
    Serial.println("BT-BLE: Set client callbacks");

    // Connect to the remote BLE Server
    BLEAddress address = myDevice->getAddress();
    esp_ble_addr_type_t type = myDevice->getAddressType();
    if (pClient->connect(address, type))
    {
        Serial.println("BT-BLE: Connected to the server");
        pClient->setMTU(517); // Set MTU after connection

        // Discover service
        BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVER_SERVICE_UUID));
        if (pRemoteService == nullptr)
        {
            Serial.println("BT-BLE: Failed to find our service UUID");
            pClient->disconnect();
            return false;
        }

        // Discover characteristic
        pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
        if (pRemoteCharacteristic == nullptr)
        {
            Serial.println("BT-BLE: Failed to find our characteristic UUID");
            pClient->disconnect();
            return false;
        }

        m_connectionState = ConnectionState::CONNECTED;
        m_clientIsConnectedToServer = true;
        return true;
    }

    Serial.println("BT-BLE: Failed to connect to the server");
    return false;
}

// Disconnect from the BLE server
void bluetooth_controller::disconnectFromServer()
{
    if (pClient != nullptr)
    {
        if (pClient->isConnected())
        {
            pClient->disconnect();
        }
        delete pClient;
        pClient = nullptr;
    }
    m_clientIsConnectedToServer = false;
    m_connectionState = ConnectionState::DISCONNECTED;
    Serial.println("BT-BLE: Disconnected from server");
}

// Register for indications from the remote characteristic
bool bluetooth_controller::registerForIndications()
{
    if (pRemoteCharacteristic->canIndicate())
    {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
        Serial.println("BT-BLE: Registered for indications");
        return true;
    }
    else
    {
        Serial.println("BT-BLE: Characteristic does not support indications");
        return false;
    }
}

// Static callback function for handling notifications/indications
void bluetooth_controller::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    if (instance)
    {
        std::string value((char *)pData, length);
        instance->handleIndication(value);
    }
}

// Handle received indications
void bluetooth_controller::handleIndication(const std::string &value)
{
    Serial.print("BT-BLE: Received indication: ");
    Serial.println(value.c_str());
    indicationReceived = true;
}

// Set the value of the remote characteristic
bool bluetooth_controller::setRemoteCharacteristicValue(const std::string &value)
{
    if (m_clientIsConnectedToServer && pRemoteCharacteristic)
    {
        indicationReceived = false;
        pRemoteCharacteristic->writeValue(value);

        // Wait for indication (with timeout)
        unsigned long startTime = millis();
        while (!indicationReceived && (millis() - startTime < 5000))
        {
            delay(10);
        }

        if (indicationReceived)
        {
            Serial.println("BT-BLE: Successfully set characteristic value and received indication");
            return true;
        }
        else
        {
            Serial.println("BT-BLE: Failed to receive indication after setting characteristic value");
            return false;
        }
    }
    else
    {
        Serial.println("BT-BLE: Not connected or characteristic not available");
        return false;
    }
}

// Initialize the BLE client (for primary skull)
void bluetooth_controller::initializeBLEClient()
{
    Serial.println("BT-BLE: Starting as BLE PRIMARY (client)");
    if (!BLEDevice::getInitialized())
    {
        BLEDevice::init("SkullPrimary-Client");
        if (!BLEDevice::getInitialized())
        {
            Serial.println("BT-BLE: Failed to initialize BLEDevice!");
            return;
        }
    }
    startScan();
}

// Set the BLE client connection status
void bluetooth_controller::setBLEClientConnectionStatus(bool status)
{
    m_clientIsConnectedToServer = status;
    m_connectionState = status ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    Serial.printf("BT-BLE: Client connection status changed to %s\n", status ? "connected" : "disconnected");
}

// Set the BLE server connection status
void bluetooth_controller::setBLEServerConnectionStatus(bool status)
{
    m_serverHasClientConnected = status;
    m_connectionState = status ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    Serial.printf("BT-BLE: Server connection status changed to %s\n", status ? "connected" : "disconnected");
}

// Get the speaker name
const String &bluetooth_controller::get_speaker_name() const
{
    return m_speaker_name;
}

// Handle A2DP connection state changes
void bluetooth_controller::connection_state_changed(esp_a2d_connection_state_t state, void *ptr)
{
    bluetooth_controller *self = static_cast<bluetooth_controller *>(ptr);

    switch (state)
    {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
        Serial.printf("BT-A2DP: Not connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
        Serial.printf("BT-A2DP: Attempting to connect to Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
        Serial.printf("BT-A2DP: Successfully connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
        Serial.printf("BT-A2DP: Disconnecting from Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    default:
        Serial.printf("BT-A2DP: Unknown connection state for Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
    }
}

// Set the volume for the A2DP connection
void bluetooth_controller::set_volume(uint8_t volume)
{
    Serial.printf("BT-A2DP: Setting bluetooth speaker volume to %d\n", volume);
    a2dp_source.set_volume(volume);
}

// Check if A2DP is connected
bool bluetooth_controller::isA2dpConnected()
{
    return a2dp_source.is_connected();
}

// Check if the BLE client is connected to the server
bool bluetooth_controller::clientIsConnectedToServer() const
{
    return m_clientIsConnectedToServer;
}

// Check if the BLE server has a client connected
bool bluetooth_controller::serverHasClientConnected() const
{
    return m_serverHasClientConnected;
}

// Static trampoline function for audio callback
int bluetooth_controller::audio_callback_trampoline(Frame *frame, int frame_count)
{
    if (instance && instance->audio_provider_callback)
    {
        return instance->audio_provider_callback(frame, frame_count);
    }
    return 0;
}

// Helper method to get a string representation of the connection state
std::string bluetooth_controller::getConnectionStateString(ConnectionState state)
{
    // This method converts the ConnectionState enum to a human-readable string
    switch (state)
    {
    case ConnectionState::DISCONNECTED:
        return "DISCONNECTED";
    case ConnectionState::SCANNING:
        return "SCANNING";
    case ConnectionState::CONNECTING:
        return "CONNECTING";
    case ConnectionState::CONNECTED:
        return "CONNECTED";
    default:
        return "UNKNOWN";
    }
}