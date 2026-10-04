#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// UUID for ELM327 service and characteristic
#define SERVICE_UUID        "FFE0" 
#define CHARACTERISTIC_UUID "FFE1"

// Global variables
static BLEScan* pBLEScan = nullptr;
static BLEAdvertisedDevice* pAdvertisedDevice = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static bool deviceFound = false;
static String ELM_DEVICE_NAME = "OBDII"; // Adjust to match your ELM327 device name

// Callback for receiving data from ELM327
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                         uint8_t* pData, size_t length, bool isNotify) {
    String response = "";
    for (int i = 0; i < length; i++) {
        response += (char)pData[i];
    }
    Serial.print("OBD Response: ");
    Serial.println(response);
}

// Class to handle device discovery
class DeviceCallback: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String deviceName = advertisedDevice.getName().c_str();
        
        if (deviceName.indexOf(ELM_DEVICE_NAME) >= 0) {
            Serial.println("Found ELM327!");
            Serial.print("Name: ");
            Serial.println(deviceName);
            Serial.print("Address: ");
            Serial.println(advertisedDevice.getAddress().toString().c_str());
            
            if (pAdvertisedDevice) {
                delete pAdvertisedDevice;
            }
            pAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
            deviceFound = true;
            
            BLEDevice::getScan()->stop();
        }
    }
};

// Function to send commands to ELM327
bool sendOBDCommand(String command) {
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Characteristic not available!");
        return false;
    }

    command += "\r";
    pRemoteCharacteristic->writeValue(command.c_str(), command.length());
    Serial.print("Sending: ");
    Serial.println(command);
    delay(100);
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting ESP32 OBD Scanner");

    // Initialize BLE
    BLEDevice::init("");

    // Configure scanning
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new DeviceCallback());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

bool connectToELM327() {
    if (!pAdvertisedDevice) {
        return false;
    }

    if (pClient != nullptr) {
        delete pClient;
    }
    
    pClient = BLEDevice::createClient();
    Serial.println("Connecting to ELM327...");

    // Connect to device
    if (!pClient->connect(pAdvertisedDevice)) {
        Serial.println("Connection failed!");
        return false;
    }
    
    Serial.println("Connected to ELM327!");

    // Get service
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find service!");
        pClient->disconnect();
        return false;
    }

    // Get characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find characteristic!");
        pClient->disconnect();
        return false;
    }

    // Set up notification callback
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    // Initialize ELM327
    delay(1000);
    sendOBDCommand("ATZ");     // Reset
    delay(1000);
    sendOBDCommand("ATE0");    // Echo off
    delay(100);
    sendOBDCommand("ATL0");    // Linefeeds off
    delay(100);
    sendOBDCommand("ATS0");    // Spaces off
    delay(100);
    sendOBDCommand("ATH0");    // Headers off
    delay(100);
    sendOBDCommand("ATSP0");   // Auto protocol
    delay(100);

    return true;
}

void loop() {
    if (!deviceFound) {
        Serial.println("Searching for ELM327...");
        pBLEScan->start(5, false);
        delay(1000);
    } else if (!pClient || !pClient->isConnected()) {
        if (connectToELM327()) {
            // Start reading data after connection
            sendOBDCommand("010C"); // RPM PID
            delay(1000);
        } else {
            deviceFound = false;
        }
    } else {
        // Periodically read data
        sendOBDCommand("010C"); // RPM
        delay(1000);
        sendOBDCommand("010D"); // Speed
        delay(1000);
        sendOBDCommand("0105"); // Engine temperature
        delay(1000);
    }
}