// CAMKILL: why do I have a begin() AND and initializeBluetooth()?
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

// CAMKILL: BLE hacking
// #include "BluetoothA2DPSink.h"
// This is from https://github.com/nkolban/ESP32_BLE_Arduino
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define READ_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define READWRITE_CHARACTERISTIC_UUID "47f1de41-c535-4e55-9b01-e6d065c6e581"

BLECharacteristic *pReadCharacteristic;
BLECharacteristic *pReadWriteCharacteristic;
char title[160] = {"Skull characteristic value!"};

// Add a callback class for handling writes to the characteristic
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.println("*********");
            Serial.print("New value: ");
            for (int i = 0; i < value.length(); i++) {
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
    instance = this;
}

void bluetooth_controller::setCharacteristicValue(const char *value)
{
    pReadCharacteristic->setValue(value);
}

void bluetooth_controller::begin(const String& speaker_name, std::function<int32_t(Frame *, int32_t)> audioProviderCallback, bool isPrimary)
{
    Serial.println("Initializing Bluetooth...");

    m_isPrimary = isPrimary;
    m_speaker_name = speaker_name;
    audio_provider_callback = audioProviderCallback;

    // Start A2DP (audio streaming)
    Serial.printf("Starting as A2DP source, connecting to speaker name: %s\n", m_speaker_name);

    a2dp_source.set_auto_reconnect(true);
    a2dp_source.set_on_connection_state_changed(connection_state_changed, this);
    a2dp_source.start(m_speaker_name.c_str(), audio_callback_trampoline);


    // Start BLE (skull-to-skull communication)
    Serial.printf("Starting as BLE source as %s\n", m_isPrimary ? "PRIMARY (client)" : "SECONDARY (server)");

    BLEDevice::init(m_isPrimary ? "SkullPrimary" : "SkullSecondary");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create read-only characteristic
    pReadCharacteristic = pService->createCharacteristic(
        READ_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );

    // Add read-only characteristic name
    BLEDescriptor *pReadCharacteristicDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    pReadCharacteristicDescriptor->setValue("ABC Readable Skull Characteristic");
    pReadCharacteristic->addDescriptor(pReadCharacteristicDescriptor);
    pReadCharacteristic->setValue(title);

    // Create read/write characteristic
    pReadWriteCharacteristic = pService->createCharacteristic(
        READWRITE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    // Add read/write characteristic name
    BLEDescriptor *pReadWriteCharacteristicDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    pReadWriteCharacteristicDescriptor->setValue("DEF Writable Skull Characteristic");
    pReadWriteCharacteristic->addDescriptor(pReadWriteCharacteristicDescriptor);

    pReadWriteCharacteristic->setValue("Initial writable value");
    pReadWriteCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Two characteristics defined! One read-only and one read/write.");
    Serial.println("Bluetooth initialization complete.");
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
    Serial.printf("Setting bluetooth speaker volume to %d\n", volume);
    a2dp_source.set_volume(volume);
}

const String& bluetooth_controller::get_speaker_name() const
{
    return m_speaker_name;
}

void bluetooth_controller::connection_state_changed(esp_a2d_connection_state_t state, void *ptr)
{
    bluetooth_controller *self = static_cast<bluetooth_controller *>(ptr);

    switch (state)
    {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
        Serial.printf("Not connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        self->is_bluetooth_connected = false;
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
        Serial.printf("Attempting to connect to Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
        Serial.printf("Successfully connected to Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
        self->is_bluetooth_connected = true;
        break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
        Serial.printf("Disconnecting from Bluetooth speaker '%s'...\n", self->m_speaker_name.c_str());
        break;
    default:
        Serial.printf("Unknown connection state for Bluetooth speaker '%s'.\n", self->m_speaker_name.c_str());
    }
}

void bluetooth_controller::resetBluetooth()
{
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    delay(1000);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);

    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    esp_bluedroid_init();
    esp_bluedroid_enable();
}

void bluetooth_controller::clearBluetoothNVS()
{
    nvs_flash_erase();
    nvs_flash_init();
}
