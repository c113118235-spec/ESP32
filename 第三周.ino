#include <WiFi.h>
#include <PubSubClient.h>
#include <NimBLEDevice.h>
#include <Arduino.h>

/* ========= WiFi ========= */
const char* ssid = "ASUS";
const char* password = "shih696969";

/* ========= MQTT ========= */
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "ibeacon/esp32";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

/* ========= iBeacon filter ========= */
#define MY_MAJOR 0xAAAA
#define MY_MINOR 0xBBBB

NimBLEScan* pScan;

/* ---------- UUID → Name ---------- */
String uuidToName(const uint8_t* u) {
  String name = "";
  for (int i = 0; i < 16; i++) {
    if (u[i] == 0x00) break;
    if (isPrintable(u[i])) name += char(u[i]);
  }
  return name;
}

/* ---------- WiFi ---------- */
void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

/* ---------- MQTT ---------- */
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    mqttClient.connect("ESP32_iBeacon");
    delay(1000);
  }
}

/* ---------- iBeacon 處理 ---------- */
void handleBeacon(const NimBLEAdvertisedDevice* dev) {
  if (!dev->haveManufacturerData()) return;

  std::string mfg = dev->getManufacturerData();
  if (mfg.length() < 25) return;

  const uint8_t* p = (uint8_t*)mfg.data();

  /* Apple iBeacon header */
  if (p[0] != 0x4C || p[1] != 0x00 || p[2] != 0x02 || p[3] != 0x15) return;

  uint16_t major = (p[20] << 8) | p[21];
  uint16_t minor = (p[22] << 8) | p[23];

  if (major != MY_MAJOR || minor != MY_MINOR) return;

  String name = uuidToName(&p[4]);
  int rssi = dev->getRSSI();

  Serial.println("===== iBeacon Found =====");
  Serial.print("Name  : "); Serial.println(name);
  Serial.print("Major : "); Serial.println(major, HEX);
  Serial.print("Minor : "); Serial.println(minor, HEX);
  Serial.print("RSSI  : "); Serial.println(rssi);
  Serial.println();

  if (mqttClient.connected()) {
    String payload = "{";
    payload += "\"name\":\"" + name + "\",";
    payload += "\"rssi\":" + String(rssi);
    payload += "}";

    mqttClient.publish(mqtt_topic, payload.c_str());
  }
}

/* ---------- setup ---------- */
void setup() {
  Serial.begin(115200);

  connectWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);

  NimBLEDevice::init("");
  pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->start(0, false);

  Serial.println("ESP32 iBeacon Scanner Ready");
}

/* ---------- loop ---------- */
void loop() {
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  NimBLEScanResults results = pScan->getResults();
  for (int i = 0; i < results.getCount(); i++) {
    handleBeacon(results.getDevice(i));
  }

  pScan->clearResults();
  delay(300);
}   