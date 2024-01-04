#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// PIN for RFID
#define SS_PIN 4  // Pin D2 on NodeMCU
#define RST_PIN 5 // Pin D1 on NodeMCU

// Configuration for RMQ
const char* MQTT_SERVER = "103.167.112.188"; //server psti
const int MQTT_PORT = 1883;
const char* MQTT_USER = "/absensi:absensi_rfid"; /vhost:user
const char* MQTT_PASS = "absensi_rfid!";
const char* ROUTING_KEY = "routing_rfid";

const int timeZoneOffset = 7 * 60 * 60;

WiFiClient espClient;
PubSubClient client(espClient);

MFRC522 mfrc522(SS_PIN, RST_PIN);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

byte mac[6];
String MAC_ADDRESS;

void printMACAddress() {
  WiFi.macAddress(mac);
  MAC_ADDRESS = mac2string(mac);
  Serial.print("Mac address ");
  Serial.println(MAC_ADDRESS);
}

String mac2string(byte arr[]) {
  String s;
  for (byte i = 0; i < 6; ++i) {
    char buf[3];
    sprintf(buf, "%2X", arr[i]);
    s += buf;
    if (i < 5) s += ':';
  }
  return s;
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming MQTT messages (if needed)
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  timeClient.setTimeOffset(timeZoneOffset);
  timeClient.begin();

  // Initialize WiFiManager
  WiFiManager wifiManager;

  // Configure WiFi or retrieve saved credentials
  if (!wifiManager.autoConnect("NodeMCU_AP")) {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println("Connected to Wi-Fi");
  printMACAddress();
  MAC_ADDRESS = mac2string(mac);

  // Connect to MQTT server with username and password
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(MAC_ADDRESS.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("Connected to MQTT");
      client.subscribe(ROUTING_KEY);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      ESP.restart();
      delay(5000);
    }
  }
}

void loop() {
  timeClient.update();

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = getUID();
    String macAddress = MAC_ADDRESS;
    String currentTime = timeClient.getFormattedTime();

    DynamicJsonDocument jsonDoc(256);
    jsonDoc["UID"] = uid;
    jsonDoc["MAC_ADDRESS"] = macAddress;
    jsonDoc["SCAN_TIME"] = currentTime;

    String jsonString;
    serializeJson(jsonDoc, jsonString);

    sendMessage(jsonString);

    mfrc522.PICC_HaltA();
  }

  delay(1000);

  client.loop();
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += (mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }

  return uid;
}

void sendMessage(String message) {
  char msg[message.length() + 1];
  message.toCharArray(msg, sizeof(msg));

  client.publish(ROUTING_KEY, msg);
  Serial.println("Message sent: " + message);
}
