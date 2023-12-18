#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "HTML.h"

#include <PubSubClient.h>

#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#define SS_PIN 4  // Pin D2 pada NodeMCU
#define RST_PIN 5 // Pin D1 pada NodeMCU

const char *SSID = "metareksa-iot";
const char *PASS = "";

String SSIDNEW = "", PASSNEW;

const char* MQTT_SERVER = "192.168.1.13";
const int   MQTT_PORT = 1883;
const char* MQTT_USER = "absensi:ilham";
const char* MQTT_PASS = "123";
const char* ROUTING_KEY = "routing.absensi";

const char* MAC_CHAR;
bool WIFI_DATA = false;
bool RMQ_STATUS = false;

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

MFRC522 mfrc522(SS_PIN, RST_PIN);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const int timeZoneOffset = 7 * 60 * 60;

byte mac[6];
String MAC_ADDRESS;

void handleRoot(){
  server.send(200, "text/html", index_html);
}

void handleForm(){
  SSIDNEW = server.arg("SSIDNEW");
  PASSNEW = server.arg("PASSNEW");

  server.send(200, "text/html", success_html);
  delay(2000);

  WiFi.softAPdisconnect(true);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSIDNEW, PASSNEW);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
}

String mac2string(byte arr[]) {
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[3];
    sprintf(buf, "%2X", arr[i]);
    s += buf;
    if (i < 5) s += ':';
  }
  return s;
}

void printMACAddress() {
  WiFi.macAddress(mac);
  MAC_ADDRESS = mac2string(mac);
  Serial.print("Mac address ");
  Serial.println(MAC_ADDRESS);
}

void reconnect() {
  if (SSIDNEW == "") {
    server.handleClient();
  } else {
    Serial.print("Berhasil terhubung ke ");
    Serial.println(SSIDNEW);
    printMACAddress();

    MAC_CHAR = MAC_ADDRESS.c_str();

    String MAC = MAC_ADDRESS+"-";
    
    const char* mac = MAC.c_str();

    while (!client.connected()) {
      Serial.println("Menghubungkan ke broker message");
      if (client.connect(MAC_CHAR, MQTT_USER, MQTT_PASS)) {
        Serial.println("Terhubung ke broker message");
        client.subscribe(ROUTING_KEY);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.print("Gagal, coba lagi dalam 5 detik. ");
        ESP.restart();
        delay(5000);
      }
    }
  }
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += (mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  
  return uid;
}

void setup() {
  Serial.begin(115200);

  SPI.begin();
  mfrc522.PCD_Init();
  timeClient.setTimeOffset(timeZoneOffset);
  timeClient.begin();

  delay(1000);
  WiFi.softAP(SSID, PASS);

  IPAddress IP = WiFi.softAPIP();
  client.setServer(MQTT_SERVER, 1883);
  
  Serial.print("Sambungkan koneksi jaringan anda ke ");
  Serial.println(SSID);
  Serial.print("Buka browser dan masukan IP address berikut ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/action_page", handleForm);

  server.begin();
  printMACAddress();
  delay(1000);
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

    // Serial.println(jsonString);
    sendMessage(jsonString);

    mfrc522.PICC_HaltA();
  }
  
  delay(1000);

  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}

void sendMessage(String message) {
  char msg[message.length() + 1];
  message.toCharArray(msg, sizeof(msg));

  client.publish("Skripsi", msg);
  Serial.println("Pesan terkirim: " + message);
}
