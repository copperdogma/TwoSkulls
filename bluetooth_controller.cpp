/*
    Respoonsible for A2DP audio streaming and BLE skull-to-skull communication.

    Yes, ideally this should be two separate classes.

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

// This is from https://github.com/nkolban/ESP32_BLE_Arduino
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#define SERVER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
char title[160] = {"Skull characteristic value!"};

static bool serverHasClientConnected = false;
static boolean clientShouldConnect = false;
static boolean clientIsConnectedToServer = false;
BLEAdvertisedDevice* bluetooth_controller::myDevice = nullptr;

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
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

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
    {
        if (bluetooth_controller::instance) {
            bluetooth_controller::instance->setBLEServerConnectionStatus(true);
            bluetooth_controller::instance->setConnectionState(ConnectionState::CONNECTED);
        }
        Serial.println("BT-BLE: Client connected!");
        Serial.print("BT-BLE: Client address: ");
        for (int i = 0; i < 6; i++)
        {
            char str[3];
            sprintf(str, "%02X", param->connect.remote_bda[i]);
            Serial.print(str);
            if (i < 5)
                Serial.print(":");
        }
        Serial.println();

        Serial.print("BT-BLE: Connection ID: ");
        Serial.println(param->connect.conn_id);

        Serial.print("BT-BLE: Connection interval: ");
        Serial.println(param->connect.conn_params.interval);

        Serial.print("BT-BLE: Connection latency: ");
        Serial.println(param->connect.conn_params.latency);

        Serial.print("BT-BLE: Connection timeout: ");
        Serial.println(param->connect.conn_params.timeout);
    }

    void onDisconnect(BLEServer *pServer)
    {
        if (bluetooth_controller::instance) {
            bluetooth_controller::instance->setBLEServerConnectionStatus(false);
            bluetooth_controller::instance->setConnectionState(ConnectionState::DISCONNECTED);
        }
        Serial.println("BT-BLE: Client disconnected");

        // Restart advertising
        BLEDevice::startAdvertising();
        Serial.println("BT-BLE: Restarted advertising after disconnection");
    }
};

void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
    if (id == ESP_AVRC_MD_ATTR_TITLE)
    {
        strncpy(title, (const char *)text, 160);
        pCharacteristic->setValue(title);
    }
}

bluetooth_controller *bluetooth_controller::instance = nullptr;

bluetooth_controller::bluetooth_controller() 
    : m_speaker_name(""), 
      m_clientIsConnectedToServer(false),
      m_serverHasClientConnected(false),
      m_connectionState(ConnectionState::DISCONNECTED),
      m_lastReconnectAttempt(0),
      scanStartTime(0)  // Initialize scanStartTime
{
    instance = this; // This line ensures proper initialization of the static instance
}

void bluetooth_controller::setCharacteristicValue(const char *value)
{
    pCharacteristic->setValue(value);
}

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

    // Start BLE (skull-to-skull communication)
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
    Create the BLE service and characteristic that the primary skull will read from and write to.

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

    BLEDevice::init("SkullSecondary-Server");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVER_SERVICE_UUID);

    // Create single read/write characteristic with indications
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_INDICATE);

    pCharacteristic->setValue("Hello from SkullSecondary");
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Sets up indications
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BT-BLE: Single characteristic defined! Now you can read/write/receive indications from the Primary skull!");
}

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        if (bluetooth_controller::instance) {
            bluetooth_controller::instance->setBLEClientConnectionStatus(true);
        }
    }

    void onDisconnect(BLEClient *pclient)
    {
        if (bluetooth_controller::instance) {
            bluetooth_controller::instance->setBLEClientConnectionStatus(false);
        }
        Serial.println("BT-BLE: Disconnected from SkullSecondary-Server");
    }
};

static const unsigned long SCAN_INTERVAL = 10000; // 10 seconds between scan attempts
static const unsigned long SCAN_DURATION = 5000;  // 5 seconds scan duration

void bluetooth_controller::update()
{
    if (m_isPrimary)
    {
        handleConnectionState();
    }
    // Reduce the frequency of status updates
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 5000) // Update every 5 seconds
    {
        Serial.printf("BT-BLE: Current connection state: %d (%s)\n", 
                      static_cast<int>(m_connectionState),
                      getConnectionStateString(m_connectionState).c_str());
        Serial.printf("BT-BLE: Client connected: %s, Server has client: %s\n",
                      m_clientIsConnectedToServer ? "true" : "false",
                      m_serverHasClientConnected ? "true" : "false");
        lastStatusUpdate = millis();
    }
}

void bluetooth_controller::handleConnectionState()
{
    switch (m_connectionState)
    {
    case ConnectionState::DISCONNECTED:
        attemptReconnection();
        break;
    case ConnectionState::SCANNING:
        if (millis() - scanStartTime > SCAN_DURATION)
        {
            pBLEScanner->stop();
            m_connectionState = ConnectionState::DISCONNECTED;
            Serial.println("BT-BLE: Scan completed, no device found.");
        }
        break;
    case ConnectionState::CONNECTING:
        // Connection attempt is ongoing, wait for callback
        break;
    case ConnectionState::CONNECTED:
        if (!m_clientIsConnectedToServer)
        {
            Serial.println("BT-BLE: Connection lost. Moving to DISCONNECTED state.");
            disconnectFromServer();
            m_connectionState = ConnectionState::DISCONNECTED;
        }
        break;
    }
}

void bluetooth_controller::attemptReconnection()
{
    static unsigned long lastScanAttempt = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastScanAttempt >= SCAN_INTERVAL)
    {
        lastScanAttempt = currentTime;
        Serial.println("BT-BLE: Attempting to reconnect...");
        startScan();
        m_connectionState = ConnectionState::SCANNING;
    }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
public:
    MyAdvertisedDeviceCallbacks(bluetooth_controller* controller) : m_controller(controller) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVER_SERVICE_UUID)))
        {
            BLEDevice::getScan()->stop();
            m_controller->setMyDevice(new BLEAdvertisedDevice(advertisedDevice));
            m_controller->setConnectionState(ConnectionState::CONNECTING);
            Serial.println("BT-BLE: Found SkullSecondary-Server. Attempting to connect.");
            m_controller->connectToServer();
        }
    }

private:
    bluetooth_controller* m_controller;
};

void bluetooth_controller::startScan()
{
    if (pBLEScanner == nullptr)
    {
        Serial.println("BT-BLE: Scanner not initialized. Initializing now.");
        initializeBLEClient();
    }
    pBLEScanner->stop();
    pBLEScanner->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(this));
    pBLEScanner->start(SCAN_DURATION / 1000, false);
    Serial.println("BT-BLE: Started scan for SkullSecondary-Server...");
    scanStartTime = millis();
}

bool bluetooth_controller::connectToServer()
{
    Serial.print("BT-BLE: Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    Serial.println("BT-BLE: - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remote BLE Server.
    pClient->connect(myDevice);
    Serial.println("BT-BLE: - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVER_SERVICE_UUID));
    if (pRemoteService == nullptr)
    {
        Serial.print("BT-BLE: Failed to find our service UUID: ");
        Serial.println(SERVER_SERVICE_UUID);
        pClient->disconnect();
        return false;
    }
    Serial.println("BT-BLE: - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
    if (pRemoteCharacteristic == nullptr)
    {
        Serial.print("BT-BLE: Failed to find our characteristic UUID: ");
        Serial.println(CHARACTERISTIC_UUID);
        pClient->disconnect();
        return false;
    }
    Serial.println("BT-BLE: - Found our characteristic");

    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead())
    {
        std::string value = pRemoteCharacteristic->readValue();
        Serial.print("BT-BLE: The characteristic value was: ");
        Serial.println(value.c_str());
    }

    // Register for indications
    registerForIndications();

    m_connectionState = ConnectionState::CONNECTED;
    m_clientIsConnectedToServer = true;
    return true;
}

void bluetooth_controller::disconnectFromServer()
{
    if (pClient != nullptr && pClient->isConnected())
    {
        pClient->disconnect();
    }
    m_clientIsConnectedToServer = false;
    m_connectionState = ConnectionState::DISCONNECTED;
}

void bluetooth_controller::registerForIndications() {
    if (pRemoteCharacteristic->canIndicate()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
        Serial.println("BT-BLE: Registered for indications");
    } else {
        Serial.println("BT-BLE: Characteristic does not support indications");
    }
}

void bluetooth_controller::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (instance) {
        std::string value((char*)pData, length);
        instance->handleIndication(value);
    }
}

void bluetooth_controller::handleIndication(const std::string& value) {
    Serial.print("BT-BLE: Received indication: ");
    Serial.println(value.c_str());
    indicationReceived = true;
}

bool bluetooth_controller::setRemoteCharacteristicValue(const std::string& value) {
    if (m_clientIsConnectedToServer && pRemoteCharacteristic) {
        indicationReceived = false;
        pRemoteCharacteristic->writeValue(value);
        
        // Wait for indication (with timeout)
        unsigned long startTime = millis();
        while (!indicationReceived && (millis() - startTime < 5000)) {
            delay(10);
        }
        
        if (indicationReceived) {
            Serial.println("BT-BLE: Successfully set characteristic value and received indication");
            return true;
        } else {
            Serial.println("BT-BLE: Failed to receive indication after setting characteristic value");
            return false;
        }
    } else {
        Serial.println("BT-BLE: Not connected or characteristic not available");
        return false;
    }
}

void bluetooth_controller::initializeBLEClient()
{
    Serial.println("BT-BLE: Starting as BLE PRIMARY (client)");

    BLEDevice::init("SkullPrimary-Client");
    pBLEScanner = BLEDevice::getScan();
    pBLEScanner->setInterval(1349);
    pBLEScanner->setWindow(449);
    pBLEScanner->setActiveScan(true);
    
    // Instead of calling startScan() here, set the state to DISCONNECTED
    m_connectionState = ConnectionState::DISCONNECTED;
    m_lastReconnectAttempt = 0; // This will trigger a scan on the next update() call
    
    Serial.println("BT-BLE: Client initialized, ready to start scanning.");
}

void bluetooth_controller::setBLEClientConnectionStatus(bool status)
{
    m_clientIsConnectedToServer = status;
    m_connectionState = status ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
}

void bluetooth_controller::setBLEServerConnectionStatus(bool status)
{
    m_serverHasClientConnected = status;
    m_connectionState = status ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
}

const String &bluetooth_controller::get_speaker_name() const
{
    return m_speaker_name;
}

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

// Add this method implementation
void bluetooth_controller::set_volume(uint8_t volume)
{
    Serial.printf("BT-A2DP: Setting bluetooth speaker volume to %d\n", volume);
    a2dp_source.set_volume(volume);
}

bool bluetooth_controller::isA2dpConnected()
{
    return a2dp_source.is_connected();
}

bool bluetooth_controller::clientIsConnectedToServer() const
{
    return m_clientIsConnectedToServer;
}

bool bluetooth_controller::serverHasClientConnected() const
{
    return m_serverHasClientConnected;
}

int bluetooth_controller::audio_callback_trampoline(Frame* frame, int frame_count)
{
    if (instance && instance->audio_provider_callback)
    {
        return instance->audio_provider_callback(frame, frame_count);
    }
    return 0;
}

// Add this helper method to the bluetooth_controller class
std::string bluetooth_controller::getConnectionStateString(ConnectionState state) {
    switch (state) {
        case ConnectionState::DISCONNECTED: return "DISCONNECTED";
        case ConnectionState::SCANNING: return "SCANNING";
        case ConnectionState::CONNECTING: return "CONNECTING";
        case ConnectionState::CONNECTED: return "CONNECTED";
        default: return "UNKNOWN";
    }
}