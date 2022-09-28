#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define PIN_4 4         //D2 RED LIGHT 101
#define PIN_5 5         //D1 BLUE LIGHT 201
#define PIN_12 12       //D6 RED LIGHT 202
#define PIN_13 13       //D7 RED LIGHT 202
#define PIN_2 2         //D4 MOISTURE SENSOR
#define PIN_14 14       //D5 WATER PUMP

const char* WIFI_NAME = "Hai Ha";
const char* WIFI_PASS = "063053726";

const char* BROKER_HOST = "broker.hivemq.com";
const int BROKER_POST = 1883;
const char* BROKER_USERNAME = "";
const char* BROKER_PASSWORD = "";

const char* TOPIC_PUB = "haipham/devices/pub";
const char* TOPIC_SUB = "haipham/devices/sub";
const char* TOPIC_PUB_CONFIG = "haipham/devices/sub/config";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

const long INTERVAL = 5000;
const int LOOP_COUNT = 10;

unsigned long previousMillis = 0;

int realValue = 0;
int value = 0;
char jsonBuffer[256];
int minThreshold = 0;
int desireThreshold = 100;
bool sendPumpData = false;

void callback(char* topic, byte* payload, unsigned int leng) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload);
  int port = doc["port"];

  if (strcmp(topic, TOPIC_SUB) == 0) {
    int value = doc["value"];
    digitalWrite(port, value);
  } else {
    int newMinThreshold = doc["value"]["minThreshold"];
    int newDesireThreshold = doc["value"]["desireThreshold"];
    minThreshold = newMinThreshold;
    desireThreshold = newDesireThreshold;
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(PIN_4, OUTPUT);
  pinMode(PIN_5, OUTPUT);
  pinMode(PIN_12, OUTPUT);
  pinMode(PIN_13, OUTPUT);
  pinMode(PIN_2, INPUT_PULLUP);

  digitalWrite(PIN_14, HIGH);
  pinMode(PIN_14, OUTPUT);
  Serial.begin(115200);

  WiFi.begin(WIFI_NAME, WIFI_PASS);
  Serial.println("Connecting to Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("Wifi connected!");

  client.setServer(BROKER_HOST, BROKER_POST);
  while (!client.connected()) {
    String clientId = "broker-hivemq-";
    clientId += String(WiFi.macAddress());
    Serial.println(clientId);
    bool connectStatus = client.connect((const char*)clientId.c_str(), BROKER_USERNAME, BROKER_PASSWORD);
    if (!connectStatus) {
      Serial.println("Connect to broker fail");
      Serial.println(client.state());
      delay(2000);
    }
  }

  client.subscribe(TOPIC_SUB);
  client.subscribe(TOPIC_PUB_CONFIG);
  client.setCallback(callback);
}

void loop() {
  client.loop();

  unsigned long currentMillis = millis();
  realValue = 0;

  if (currentMillis - previousMillis >= INTERVAL) {
    for (int i = 0; i < LOOP_COUNT; i ++) {
      realValue += analogRead(A0);
    }

    value = realValue / LOOP_COUNT;
    int percent = map(value, 0, 1023, 100, 0);

    StaticJsonDocument<256> doc;
    
    doc["percent"] = percent;
    doc["sensorId"] = "630c7f1d6ae147067e678c42";

    if (percent < minThreshold) {
      digitalWrite(PIN_14, 1);
      if (!sendPumpData) {
        doc["pumpId"] = "630c7fdcec1f18d4de014de2";
        doc["pumpValue"] = 1;
      }
      sendPumpData = true;
    } else {
      if (sendPumpData) {
        doc["pumpId"] = "630c7fdcec1f18d4de014de2";
        doc["pumpValue"] = 0;
      }
      sendPumpData = false;
    }
    serializeJson(doc, jsonBuffer);
    client.publish(TOPIC_PUB, jsonBuffer);

    previousMillis = currentMillis;
  }

}
