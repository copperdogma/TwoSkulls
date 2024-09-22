/*
    Respoonsible for A2DP audio streaming and BLE skull-to-skull communication.

    Yes, ideally this should be two separate classes.
*/

//  Maybe one for setup, then we set params, then start the service? Is that what it is?
/*


    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

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
#include <BLEServer.h>
#include <BLEUtils.h>
#define SERVER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define READ_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define READWRITE_CHARACTERISTIC_UUID "47f1de41-c535-4e55-9b01-e6d065c6e581"

BLECharacteristic *pReadCharacteristic;
BLECharacteristic *pReadWriteCharacteristic;
char title[160] = {"Skull characteristic value!"};

// Add a callback class for handling writes to the characteristic
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

void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
    if (id == ESP_AVRC_MD_ATTR_TITLE)
    {
        strncpy(title, (const char *)text, 160);
        pReadCharacteristic->setValue(title);
    }
}

bluetooth_controller *bluetooth_controller::instance = nullptr;

bluetooth_controller::bluetooth_controller() : is_bluetooth_connected(false), last_reconnection_attempt(0), m_speaker_name("")
{
    instance = this; // This line ensures proper initialization of the static instance
}

void bluetooth_controller::setCharacteristicValue(const char *value)
{
    pReadCharacteristic->setValue(value);
}

void bluetooth_controller::begin(const String &speaker_name, std::function<int32_t(Frame *, int32_t)> audioProviderCallback, bool isPrimary)
{
    Serial.println("BT: Initializing Bluetooth...");

    m_isPrimary = isPrimary;
    m_speaker_name = speaker_name;
    audio_provider_callback = audioProviderCallback;

    // Start A2DP (audio streaming)
    Serial.printf("BT-A2DP: Starting as A2DP source, connecting to speaker name: %s\n", m_speaker_name);

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

void bluetooth_controller::initializeBLEServer()
{
    Serial.println("BT-BLE: Starting as BLE SECONDARY (server)");

    BLEDevice::init("SkullSecondary-Server");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVER_SERVICE_UUID);

    // Create read-only characteristic
    pReadCharacteristic = pService->createCharacteristic(
        READ_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ);

    pReadCharacteristic->setValue("Hello from SkullSecondary");

    // Create read/write characteristic
    pReadWriteCharacteristic = pService->createCharacteristic(
        READWRITE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

    pReadWriteCharacteristic->setValue("Initial writable value");
    pReadWriteCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BT-BLE: Characteristics defined! Now you can read/write them from the Primary skull!");
}

// CAMKILL: yeah do this:
// Add these new global variables
static boolean doConnect = false;
static boolean connected = false;
static BLEAdvertisedDevice *myDevice;

// Add this new class definition
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        connected = true;
    }

    void onDisconnect(BLEClient *pclient)
    {
        connected = false;
        Serial.println("BT-BLE: Disconnected from SkullSecondary-Server");
    }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVER_SERVICE_UUID)))
        {
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
        }
    }
};

bool bluetooth_controller::connectToServer()
{
    Serial.print("BT-BLE: Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    Serial.println("BT-BLE: - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    pClient->connect(myDevice);
    Serial.println("BT-BLE: - Connected to server");

    BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVER_SERVICE_UUID));
    if (pRemoteService == nullptr)
    {
        Serial.print("BT-BLE: Failed to find our service UUID: ");
        Serial.println(SERVER_SERVICE_UUID);
        pClient->disconnect();
        return false;
    }
    Serial.println("BT-BLE: - Found our service");

    pRemoteReadCharacteristic = pRemoteService->getCharacteristic(BLEUUID(READ_CHARACTERISTIC_UUID));
    pRemoteReadWriteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(READWRITE_CHARACTERISTIC_UUID));

    if (pRemoteReadCharacteristic == nullptr || pRemoteReadWriteCharacteristic == nullptr)
    {
        Serial.println("BT-BLE: Failed to find our characteristic UUID");
        pClient->disconnect();
        return false;
    }
    Serial.println("BT-BLE: - Found our characteristics");

    if (pRemoteReadCharacteristic->canRead())
    {
        std::string value = pRemoteReadCharacteristic->readValue();
        Serial.print("BT-BLE: The read characteristic value was: ");
        Serial.println(value.c_str());
    }

    return true;
}

void bluetooth_controller::initializeBLEClient()
{
    Serial.println("BT-BLE: Starting as BLE PRIMARY (client)");

    BLEDevice::init("SkullPrimary-Client");
    pBLEScanner = BLEDevice::getScan();
    pBLEScanner->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScanner->setInterval(1349);
    pBLEScanner->setWindow(449);
    pBLEScanner->setActiveScan(true);
    pBLEScanner->start(5, false);
    Serial.println("BT-BLE: BLE Client initialized and scanning for SkullSecondary-Server...");
}

void bluetooth_controller::handleBLEClient()
{
    if (doConnect)
    {
        if (connectToServer())
        {
            Serial.println("BT-BLE: We are now connected to the SkullSecondary-Server.");
        }
        else
        {
            Serial.println("BT-BLE: We have failed to connect to the SkullSecondary-Server; there is nothing more we will do.");
        }
        doConnect = false;
    }

    if (connected)
    {
        // Here you can perform any operations you need when connected
        // For example, you might want to read or write to the characteristics periodically
    }
}

// Add this new method to be called in your main loop for the primary skull
void bluetooth_controller::update()
{
    if (m_isPrimary)
    {
        handleBLEClient();
    }
}

// The bluetooth library automatically calls this to get audio data from the audio player to the bluetooth module.
// The bluetooth only calls it when there's an active bluetooth connection.
int bluetooth_controller::audio_callback_trampoline(Frame *frame, int frame_count)
{
    if (instance && instance->audio_provider_callback)
    {
        return static_cast<int>(instance->audio_provider_callback(frame, static_cast<int32_t>(frame_count)));
    }
    return 0;
}
// The bluetooth library automatically calls this to get audio data from the audio player to the bluetooth module.
void bluetooth_controller::set_connection_state_callback(void (*callback)(esp_a2d_connection_state_t state, void *obj))
{
    a2dp_source.set_on_connection_state_changed(callback, this);
}

bool bluetooth_controller::is_connected()
{
    return a2dp_source.is_connected();
}

void bluetooth_controller::set_volume(uint8_t volume)
{
    Serial.printf("BT-A2DP: Setting bluetooth speaker volume to %d\n", volume);
    a2dp_source.set_volume(volume);
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
        self->is_bluetooth_connected = false;
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
        Serial.printf("BT-A2DP: Attempting to connect to Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
        Serial.printf("BT-A2DP: Successfully connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        self->is_bluetooth_connected = true;
        break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
        Serial.printf("BT-A2DP: Disconnecting from Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    default:
        Serial.printf("BT-A2DP: Unknown connection state for Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
    }
}