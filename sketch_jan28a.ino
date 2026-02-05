#include <WiFi.h>
#include <PubSubClient.h>
#include <NimBLEDevice.h>
#include <map>
#include <Arduino.h>

/* ---------- WiFi 設定 ---------- */
const char* ssid = "ASUS";
const char* password = "shih696969";

/* ---------- MQTT 設定 ---------- */
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "ibeacon/data";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

/* ---------- NimBLE ---------- */
NimBLEScan* pScan;

/* ---------- UUID 解密 Key ---------- */
const uint8_t UUID_KEY = 0x5A;
const uint16_t MY_MAJOR = 0x0001;
const uint16_t MY_MINOR = 0x0001;
const uint8_t ENCRYPTED_NAME_PREFIX[3] = {0x13, 0x1B, 0x14}; 

/* ---------- UUID 解密 ---------- */
void decryptUUID(uint8_t* uuidBytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uuidBytes[i] ^= UUID_KEY;
  }
}

/* ---------- UUID → 英文名字 ---------- */
String uuidToName(uint8_t* uuid, size_t len) {
  String name = "";
  for (size_t i = 0; i < len; i++) {
    if (uuid[i] == 0x00) break;
    if (isPrintable(uuid[i])) {
      name += char(uuid[i]);
    }
  }
  return name;
}

/* ---------- WiFi 連線 ---------- */
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 20) {
    delay(500);
    Serial.print(".");
    count++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed");
  }
}

/* ---------- MQTT 連線 ---------- */
void reconnectMQTT() {
  if (mqttClient.connected()) return;
  Serial.print("Connecting to MQTT... ");
  if (mqttClient.connect("ESP32_iBeacon")) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
  }
}

/* ---------- 發送與顯示 ---------- */
void sendBeacon(uint8_t* uuidBytes, int major, int minor, int rssi, bool decrypt) {
  uint8_t displayUUID[16];
  memcpy(displayUUID, uuidBytes, 16);
  String displayType = "ENCRYPTED";
  String name = "";

  if (decrypt) {
    if (displayUUID[0] == ENCRYPTED_NAME_PREFIX[0] &&
        displayUUID[1] == ENCRYPTED_NAME_PREFIX[1] &&
        displayUUID[2] == ENCRYPTED_NAME_PREFIX[2]) {
      decryptUUID(displayUUID, 16);
      displayType = "DECRYPTED";
      name = uuidToName(displayUUID, 16);
    }
  }

  char uuidStr[37];
  snprintf(uuidStr, sizeof(uuidStr),
    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
    displayUUID[0], displayUUID[1], displayUUID[2], displayUUID[3],
    displayUUID[4], displayUUID[5],
    displayUUID[6], displayUUID[7],
    displayUUID[8], displayUUID[9],
    displayUUID[10], displayUUID[11], displayUUID[12], displayUUID[13], displayUUID[14], displayUUID[15]
  );

  // Serial 顯示
  Serial.println("===== iBeacon Found =====");
  Serial.print("Type : "); Serial.println(displayType);
  Serial.print("UUID : "); Serial.println(uuidStr);
  Serial.print("Name : "); Serial.println(name);
  Serial.print("Major : "); Serial.println(major);
  Serial.print("Minor : "); Serial.println(minor);
  Serial.print("RSSI : "); Serial.println(rssi);

  // MQTT 發送
  if (mqttClient.connected()) {
    char msg[256];
    snprintf(msg, sizeof(msg),
      "{\"type\":\"%s\",\"name\":\"%s\",\"uuid\":\"%s\",\"major\":%d,\"minor\":%d,\"rssi\":%d}",
      displayType.c_str(), name.c_str(), uuidStr, major, minor, rssi
    );
    mqttClient.publish(mqtt_topic, msg);
    Serial.println("MQTT published");
  }
  Serial.println();
}

/* ---------- 處理 iBeacon ---------- */
void processBeacon(const NimBLEAdvertisedDevice* device) {
  if (!device->haveManufacturerData()) return;
  std::string data = device->getManufacturerData();
  if (data.size() < 25) return;

  const uint8_t* payload = (const uint8_t*)data.data();
  uint16_t company = payload[0] | (payload[1] << 8);
  uint8_t type = payload[2];
  uint8_t length = payload[3];
  if (company != 0x004C || type != 0x02 || length != 0x15) return;

  uint16_t major = (payload[20] << 8) | payload[21];
  uint16_t minor = (payload[22] << 8) | payload[23];
  if (major != MY_MAJOR || minor != MY_MINOR) return;

  uint8_t uuidBytes[16];
  memcpy(uuidBytes, &payload[4], 16);

  int rssi = device->getRSSI();

  // 先發送 ENCRYPTED
  sendBeacon(uuidBytes, major, minor, rssi, false);

  // 再發送 DECRYPTED
  sendBeacon(uuidBytes, major, minor, rssi, true);
}

/* ---------- setup ---------- */
void setup() {
  Serial.begin(115200);
  delay(1000);

  connectWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);

  NimBLEDevice::init("");
  pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(80);
  pScan->start(0, false);

  Serial.println("ESP32 iBeacon Scanner Started");
}

/* ---------- loop ---------- */
void loop() {
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  NimBLEScanResults results = pScan->getResults();
  for (int i = 0; i < results.getCount(); i++) {
    processBeacon(results.getDevice(i));
  }

  pScan->clearResults();
  delay(500); // 每 0.5 秒掃描一次
}
