#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Tcl.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 🔹 WiFi credentials
const char* ssid = "POCO M3";
const char* password = "Palem2f8771";

// 🔹 Ubidots MQTT credentials
const char* mqtt_server = "industrial.api.ubidots.com";
const int mqtt_port = 1883;
const char* mqtt_token = "BBUS-4p3JS6lOPE6pouh8uHFWg3CjWkBm7B";
const char* device_label = "esp32";

// 🔹 MQTT topics
const char* mqtt_topic_publish = "/v1.6/devices/esp32";
String mqtt_topic_subscribe = String("/v1.6/devices/") + device_label + "/+/lv";

// 🔹 IR LED pin
const uint16_t kIrLed = 23;

// 🔹 Sensor DHT
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// 🔹 OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 🔹 Global variables
IRTcl112Ac ac(kIrLed);
WiFiClient espClient;
PubSubClient client(espClient);

bool powerState = false;
uint8_t temperature = 24;
bool stateChanged = false;

float dhtTemperature = 0;

// 🔹 Koneksi WiFi
void setup_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected. IP: " + WiFi.localIP().toString());
}

// 🔹 Publish ke Ubidots
void publishState() {
  DynamicJsonDocument doc(256);
  doc["power"] = powerState;
  doc["temperature"] = temperature;
  doc["suhu"] = dhtTemperature;

  String payload;
  serializeJson(doc, payload);

  if (client.publish(mqtt_topic_publish, payload.c_str())) {
    Serial.println("Published: " + payload);
  } else {
    Serial.println("Failed to publish.");
  }
}

// 🔹 Kirim IR
void sendIRCommand() {
  ac.setPower(powerState);
  ac.setMode(kTcl112AcCool);
  ac.setTemp(temperature);
  ac.send();
  Serial.println("IR sent: Power=" + String(powerState) + ", Temp=" + String(temperature));
}

// 🔹 MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String topicStr = String(topic);
  int lastSlash = topicStr.lastIndexOf('/');
  int secondLastSlash = topicStr.lastIndexOf('/', lastSlash - 1);
  String variable = topicStr.substring(secondLastSlash + 1, lastSlash);

  float value = message.toFloat();

  if (variable == "power") {
    bool val = (value >= 0.5);
    if (powerState != val) {
      powerState = val;
      stateChanged = true;
    }
  } else if (variable == "temperature") {
    int val = int(value);
    if (val >= 16 && val <= 30 && temperature != val) {
      temperature = val;
      stateChanged = true;
    }
  }
}

// 🔹 MQTT reconnect
void reconnect() {
  while (!client.connected()) {
    String clientId = "esp32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_token, mqtt_token)) {
      client.subscribe(mqtt_topic_subscribe.c_str());
      Serial.println("MQTT connected & subscribed to: " + mqtt_topic_subscribe);
    } else {
      Serial.print("MQTT connect failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

// 🔹 OLED update
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  String acText = "AC: " + String(powerState ? "Nyala" : "Mati");
  String suhuText = String(dhtTemperature) + " C";

  int16_t x1, y1;
  uint16_t tw, th;

  display.getTextBounds(acText, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 10);
  display.print(acText);

  display.getTextBounds("Suhu:", 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 30);
  display.print("Suhu:");

  display.getTextBounds(suhuText, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 50);
  display.print(suhuText);

  

  display.display();
}

// 🔹 Setup
void setup() {
  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  ac.begin();
  ac.setModel(tcl_ac_remote_model_t::GZ055BE1);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("⚠️ OLED initialization failed!");
    while (true);
  }
  display.clearDisplay();
  display.display();

  dhtTemperature = dht.readTemperature();
  if (isnan(dhtTemperature)) {
    Serial.println("⚠️ Failed to read from DHT sensor (initial)!");
    dhtTemperature = 0; // fallback untuk mencegah tampilan kosong
  }

  updateDisplay();
}

// 🔹 Loop
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (stateChanged) {
    sendIRCommand();
    publishState();
    updateDisplay();
    stateChanged = false;
  }

  static unsigned long lastDHTRead = 0;
  if (millis() - lastDHTRead > 5000) {
    lastDHTRead = millis();
    dhtTemperature = dht.readTemperature();

    if (isnan(dhtTemperature)) {
      Serial.println("⚠️ Failed to read from DHT sensor!");
    } else {
      Serial.println("🌡️ DHT Temperature: " + String(dhtTemperature) + "°C");
      updateDisplay();

    }
  }
}
