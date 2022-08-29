#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define PIN_4 4         //D2 RED LIGHT
#define PIN_5 5         //D1 BLUE LIGHT
#define PIN_2 2         //D4 MOISTURE SENSOR
#define PIN_13 13       //D7 WATER PUMP

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
char buffer[256];
int minThreshold = 0;
int desireThreshold = 100;

void callback(char* topic, byte* payload, unsigned int leng) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload);
  int port = doc["port"];
  int value = doc["value"];

  if (strcmp(topic, TOPIC_SUB)) {
    digitalWrite(port, value);
  } else {
    minThreshold = value["minThreshold"];
    desireThreshold = value["desireThreshold"];

    Serial.println(minThreshold);
    Serial.println(desireThreshold);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(PIN_4, OUTPUT);
  pinMode(PIN_5, OUTPUT);
  pinMode(PIN_2, INPUT_PULLUP);
  pinMode(PIN_13, OUTPUT);
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
  // put your main code here, to run repeatedly:
  client.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= INTERVAL) {
    //    for (int i = 0; i < LOOP_COUNT; i ++) {
    //      real_value += analogRead(A0);
    //    }

    //    value = real_value / LOOP_COUNT;
    //    int percent = map(value, 0, 1023, 100, 0);

    StaticJsonDocument<2> doc;

    //    doc["percent"] = analogRead(A0);
    //    doc["digitalValue"] = digitalRead(PIN_2);

    doc["percent"] = 33;
    doc["digitalValue"] = 1;

    serializeJson(doc, buffer);

    client.publish(TOPIC_PUB, buffer);

    previousMillis = currentMillis;
  }

}
