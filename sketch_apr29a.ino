/*
 * Hook up 2 TTP223 touch pads to a ESP32 board, on pins 25 (left) & 27 (right)
 * Standard settings for ESP32 are used for compile & download
 * Each key-press gives +-10° on the steering angle
 * The steering angle is notified to the BLE client (Zwift) every second, and then reset.
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include "ble_cus.h"

#define STEERER_SERVICE_UUID_BASE                                                    \
        {                                                                            \
            0x92, 0xe5, 0x9c, 0x94, 0xf3, 0x8f, 0x18, 0x89, 0x8b, 0x40, 0x35, 0x76,  \
            0x00, 0x00, 0x7b, 0x34                                                   \
        }

#define STEERER_SERVICE_UUID        BLEUUID((uint16_t)0x0001)
#define STEERER_CHAR_UUID           BLEUUID((uint16_t)0x0030)
#define RX_CHAR_UUID                BLEUUID((uint16_t)0x0031)
#define TX_CHAR_UUID                BLEUUID((uint16_t)0x0032)

bool deviceConnected = false;
BLECharacteristic *pCharTX;
BLECharacteristic *pCharSteerer;
BLE2902 *p2902CharSteerer;
BLE2902 *p2902CharTX;
float steeringAngle = 0.0;

bool challengeOK = false;
bool ntfTXOn = false;

ble_cus_t bleCusInstance;
ble_cus_init_t bleCusInitData;

static void bleNotifySteeringAngle() {
  ble_cus_steering_value_update(&bleCusInstance, steeringAngle);
  steeringAngle = 0.0;
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

static uint32_t rotate_left32(uint32_t value, uint32_t count) {
    const uint32_t mask = (CHAR_BIT * sizeof(value)) - 1;
    count &= mask;
    return (value << count) | (value >> (-count & mask));
}

static uint32_t hashed(uint64_t seed) {
    uint32_t ret = (seed + 0x16fa5717);
    uint64_t rax = seed * 0xba2e8ba3;
    uint64_t eax = (rax >> 35) * 0xb;
    uint64_t ecx = seed - eax;
    uint32_t edx = rotate_left32(seed, ecx & 0x0F);
    ret ^= edx;
    return ret;
}

class charRXCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string val = pCharacteristic->getValue();
    Serial.print("charRXEventHandler : ");
    for (int i=0;i<val.length();i++){
      Serial.print(val[i],HEX);
      Serial.print('-');
    }
    Serial.println();
    
    // Handle challenge-response mechanism
    if (val[0] == 0x03 && val[1] == 0x10) {
      Serial.println("Received initial challenge request");
      // Issue the challenge of 0x0310yyyy on 0x0032
      uint8_t challenge[] = {0x03, 0x10, 0x4a, 0x89};
      pCharTX->setValue(challenge, sizeof(challenge));
      pCharTX->indicate();
      Serial.println("Sent challenge");
    } else if (val[0] == 0x03 && val[1] == 0x11) {
      Serial.println("Received secondary challenge request");
      // Emit 0x0311ffff on 0x0032
      uint8_t response[] = {0x03, 0x11, 0xff, 0xff};
      pCharTX->setValue(response, sizeof(response));
      pCharTX->indicate();
      challengeOK = true;
      Serial.println("Sent secondary response");
    } else if (val[0] == 0x03 && val[1] == 0x12) {
      Serial.println("Received challenge");
      // Handle challenge response
      uint32_t seed = (val[5] << 24) | (val[4] << 16) | (val[3] << 8) | val[2];
      uint32_t password = hashed(seed);
      uint8_t response[6] = {0x03, val[1], (password & 0x000000FF), (password & 0x0000FF00) >> 8, (password & 0x00FF0000) >> 16, (password & 0xFF000000) >> 24};
      pCharTX->setValue(response, sizeof(response));
      pCharTX->indicate();
      Serial.println("Sent challenge response");
    } else if (val[0] == 0x03 && val[1] == 0x13) {
      Serial.println("Received final response");
      // Handle final response
      uint8_t response[3] = {0x03, val[1], 0xFF};
      pCharTX->setValue(response, sizeof(response));
      pCharTX->indicate();
      challengeOK = true;
      Serial.println("Authentication completed");
    }
  }
};

class charTXNtfCallbacks:public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor* pDescriptor) {
    ntfTXOn = true;
    Serial.println("Notification enabled on TX characteristic");
    
    // Issue initial challenge on TX characteristic
    uint8_t challenge[] = {0x03, 0x10, 0x4a, 0x89};
    pCharTX->setValue(challenge, sizeof(challenge));
    pCharTX->indicate();
    Serial.println("Sent initial challenge");
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Sterzo!");
  pinMode(27,INPUT);
  pinMode(25,INPUT);

  BLEDevice::deinit(true);
  delay(1000);

  BLEDevice::init("stefaan-sterzo");
  Serial.println("BLE initialized");

  BLEServer *pServer = BLEDevice::createServer();
  Serial.println("BLE server created");

  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("Server callbacks set");

  BLEService *pSvcSterzo = pServer->createService(STEERER_SERVICE_UUID);
  Serial.println("Sterzo service created");

  pCharSteerer = pSvcSterzo->createCharacteristic(STEERER_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  Serial.println("Steerer characteristic created");

  BLECharacteristic *pCharRX = pSvcSterzo->createCharacteristic(RX_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  Serial.println("RX characteristic created");

  pCharTX = pSvcSterzo->createCharacteristic(TX_CHAR_UUID, BLECharacteristic::PROPERTY_INDICATE);
  Serial.println("TX characteristic created");

  p2902CharSteerer = new BLE2902();
  p2902CharTX = new BLE2902();
  p2902CharTX->setCallbacks(new charTXNtfCallbacks());
  pCharSteerer->addDescriptor(p2902CharSteerer);
  pCharTX->addDescriptor(p2902CharTX);
  Serial.println("Descriptors added");

  // initial values
  uint8_t defaultValue[4] = {0x0, 0x0, 0x0, 0x0};
  pCharSteerer->setValue(defaultValue, 4); // default angle = 0
  pCharRX->setValue(defaultValue, 1);
  uint8_t challenge[] = {0x03, 0x10, 0x4a, 0x89};
  pCharTX->setValue(challenge, 4);
  Serial.println("Initial values set");

  pCharRX->setCallbacks(new charRXCallbacks());
  Serial.println("RX callbacks set");

  // Initialize BLE Custom Service
  bleCusInitData.evt_handler = NULL;
  ble_cus_init(&bleCusInstance, &bleCusInitData);
  Serial.println("Custom service initialized");

  pSvcSterzo->start();
  Serial.println("Sterzo service started");

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(STEERER_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  Serial.println("Advertising parameters set");

  BLEDevice::startAdvertising();
  Serial.println("Advertising started");

  Serial.println("Sterzo ready!");
}

uint32_t bleNotifyMillis;
bool buttonTaken = false;

void loop() {
  bool buttonLeft = digitalRead(27);
  bool buttonRight = digitalRead(25);
  // TTP223 doesn't need debouncing

  if (!buttonLeft && !buttonRight) buttonTaken = false; // listen for new key
  if (buttonLeft && !buttonTaken) {
    steeringAngle -= 10;
    if (steeringAngle < -40) steeringAngle = -40;
    buttonTaken = true;
    Serial.print("steering angle: ");Serial.println(steeringAngle);
  }
  if (buttonRight && !buttonTaken) {
    steeringAngle += 10;
    if (steeringAngle > 40) steeringAngle = 40;
    buttonTaken = true;
    Serial.print("steering angle: ");Serial.println(steeringAngle);
  }
  // steering angle will be reset after each BLE-notify

  // if Zwift subscribes to the notifications, we handle the challenge-response mechanism
  if (ntfTXOn && deviceConnected) {
    ntfTXOn = false;
    // Handle challenge-response mechanism here
    uint8_t challenge[] = {0x03, 0x10, 0x4a, 0x89};
    pCharTX->setValue(challenge, sizeof(challenge));
    pCharTX->indicate();
  }
  
  if (deviceConnected) {
    // notify steering angle every second
    if (millis() - bleNotifyMillis > 1000) {
      bleNotifySteeringAngle();
      bleNotifyMillis = millis();
    }
  }
}