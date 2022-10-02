// Khai báo các thư viện
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Khai báo các PIN sẽ dùng trên ESP8266
#define PIN_4 4         //D2 RED LIGHT 101
#define PIN_5 5         //D1 BLUE LIGHT 201
#define PIN_12 12       //D6 RED LIGHT 202
#define PIN_13 13       //D7 RED LIGHT 202
#define PIN_2 2         //D4 MOISTURE SENSOR
#define PIN_14 14       //D5 WATER PUMP

// Thông tin để kết nối Wifi
const char* WIFI_NAME = "Hai Ha";
const char* WIFI_PASS = "063053726";

// Thông tin kết nối MQTT Broker
const char* BROKER_HOST = "broker.hivemq.com";
const int BROKER_POST = 1883;
const char* BROKER_USERNAME = "";
const char* BROKER_PASSWORD = "";

/*
  Các topic để subscribe và publish
  Topic TOPIC_PUB để publish trạng thái thiết bị
  Topic TOPIC_SUB_CONFIG để subscribe config cho cảm biến độ ẩm
  Topic TOPIC_SUB để subscribe yêu cầu thay đổi trạng thái thiết bị
*/
const char* TOPIC_PUB = "haipham/devices/pub";
const char* TOPIC_SUB_CONFIG = "haipham/devices/sub/config";
const char* TOPIC_SUB = "haipham/devices/sub";

// Tạo PubSubClient: Làm việc với MQTT Broker
WiFiClient wifiClient;
PubSubClient client(wifiClient);

const long INTERVAL = 5000;   // Cứ 5 giây gửi dữ liệu cảm biến một lần
const int LOOP_COUNT = 10;    // Cứ đo 10 lần giá trị cảm biến, tính trung bình rồi mới gửi

// Khởi tạo thời điểm ban đầu chương trình chạy là 0
unsigned long previousMillis = 0;

// Một số biến khác
int realValue = 0;
int value = 0;
char jsonBuffer[256];
int minThreshold = 0;
int desireThreshold = 100;
bool sendPumpData = false;

// Hàm callback khi nhận được thông điệp trên các kênh đã subscribe
void callback(char* topic, byte* payload, unsigned int leng) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload);
  // Đọc port (PIN) cần điều khiển
  int port = doc["port"];

  // Nếu topic đó là TOPIC_SUB -> yêu cầu thay đổi trạng thái thiết bị
  if (strcmp(topic, TOPIC_SUB) == 0) {
    // Đọc value và ghi giá trị ra PIN tương ứng
    int value = doc["value"];
    digitalWrite(port, value);
  } else {
    // Trường hợp còn lại đó là thay đổi config cho cảm biến độ ẩm
    // Đọc ra minThreshold và desireThreshold và lưu vào các biến tương ứng
    int newMinThreshold = doc["value"]["minThreshold"];
    int newDesireThreshold = doc["value"]["desireThreshold"];
    minThreshold = newMinThreshold;
    desireThreshold = newDesireThreshold;
  }
}

// Hàm setup cho chương trình chạy
void setup() {
  // Thiết lập mode INPUT/OUTPUT cho các PIN
  pinMode(PIN_4, OUTPUT);
  pinMode(PIN_5, OUTPUT);
  pinMode(PIN_12, OUTPUT);
  pinMode(PIN_13, OUTPUT);
  pinMode(PIN_2, INPUT_PULLUP);
  pinMode(PIN_14, OUTPUT);
  // Cấu hình baudrate cho Serial là 115200
  Serial.begin(115200);

  //Kết nối wifi
  WiFi.begin(WIFI_NAME, WIFI_PASS);
  Serial.println("Connecting to Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    // Nếu không thành công, đợi 500ms rồi kết nối lại
    delay(500);
  }
  Serial.println("Wifi connected!");

  // Kết nối wifi thành công -> tiếp tục kết nối MQTT Broker
  client.setServer(BROKER_HOST, BROKER_POST);
  while (!client.connected()) {
    String clientId = "broker-hivemq-";
    clientId += String(WiFi.macAddress());
    Serial.println(clientId);
    bool connectStatus = client.connect((const char*)clientId.c_str(), BROKER_USERNAME, BROKER_PASSWORD);
    // Kiểm tra kết nối tới MQTT Broker, nếu thất bại thì kết nối lại
    if (!connectStatus) {
      Serial.println("Connect to broker fail");
      Serial.println(client.state());
      delay(2000);
    }
  }

  // Nếu kết nối thành công, subscribe các topic TOPIC_SUB và TOPIC_SUB_CONFIG
  client.subscribe(TOPIC_SUB);
  client.subscribe(TOPIC_SUB_CONFIG);

  // set hàm callback khi nhận thông điệp trên các topic đã subscribe
  client.setCallback(callback);
}

// Hàm giữ cho ứng dụng luôn chạy
void loop() {
  // Duy trì kết nối tới MQTT Broker
  client.loop();

  unsigned long currentMillis = millis();
  realValue = 0;

  // Mỗi khoảng thời gian INTERVAL lại đo lại giá trị của độ ẩm đất và tính trung bình
  if (currentMillis - previousMillis >= INTERVAL) {
    for (int i = 0; i < LOOP_COUNT; i ++) {
      realValue += analogRead(A0);
    }

    value = realValue / LOOP_COUNT;
    int percent = map(value, 0, 1023, 100, 0);

    // Tạo dữ liệu dạng Json để gửi qua topic TOPIC_PUB
    StaticJsonDocument<256> doc;

    doc["percent"] = percent;
    doc["sensorId"] = "630c7f1d6ae147067e678c42";

    // Nếu độ ẩm hiện tại đang nhỏ hơn minThreshold thì bật máy bơm nước, ngược lại tắt máy bơm nước
    // Và thêm trường pumId, pumValue vào doc để báo hiệu trạng thái máy bơm thay đổi
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

    // Publish dữ liệu qua topic TOPIC_PUB
    client.publish(TOPIC_PUB, jsonBuffer);

    previousMillis = currentMillis;
  }
}
