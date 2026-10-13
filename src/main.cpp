#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ArduinoJson.h>

// =========================================================
// การตั้งค่าฮาร์ดแวร์
// =========================================================
#define DHTPIN 15          
#define DHTTYPE DHT22      
#define WIFI_RESET_BTN 4   // ปุ่มรีเซ็ต WiFi (Active LOW)

#define RELAY_PUMP 14      
#define RELAY_VALVE 27     

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =========================================================
// การตั้งค่า MQTT และ Topic
// =========================================================
const char* mqtt_server = "192.168.1.72"; // เปลี่ยนเป็น IP ของ MQTT Broker
const int mqtt_port = 11883; // Host port mapped to Mosquitto's container port 1883

WiFiClient espClient;
PubSubClient client(espClient);

String site_id = "site_01";
String zone_id = "zone_A";
String device_id = "node_01"; // ต้องไม่ซ้ำกัน
String base_topic = "smart_farm/" + site_id + "/" + zone_id + "/" + device_id + "/";

unsigned long lastMsg = 0; 

void reconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// =========================================================
// ฟังก์ชัน 1: setup()
// =========================================================
void setup() {
  Serial.begin(115200);
  Serial.println("Smart Farm starting...");
  
  pinMode(WIFI_RESET_BTN, INPUT_PULLUP);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_VALVE, OUTPUT);
  digitalWrite(RELAY_PUMP, 0); 
  digitalWrite(RELAY_VALVE, 0);
  Serial.printf("[RELAY] Initial GPIO levels: pump=%d, valve=%d\n", digitalRead(RELAY_PUMP), digitalRead(RELAY_VALVE));

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Farm Lab");

  dht.begin();
  Serial.printf("[DHT] DHT22 initialized on GPIO %d\n", DHTPIN);

  WiFiManager wm;
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");
  
  Serial.println("[WiFi] Connecting; setup AP: SmartFarm_Setup");
  if (!wm.autoConnect("SmartFarm_Setup")) {
    Serial.println("[WiFi] Connection failed. Restarting in 3 seconds.");
    delay(3000);
    ESP.restart(); 
  }
  
  Serial.printf("[WiFi] Connected. IP=%s, RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connected!   ");
  lcd.setCursor(0, 2);
  lcd.print(WiFi.localIP().toString());
  delay(2000);
  lcd.clear();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  Serial.printf("[MQTT] Broker=%s:%d, client=%s\n", mqtt_server, mqtt_port, device_id.c_str());
}

// =========================================================
// ฟังก์ชัน 2: loop()
// =========================================================
void loop() {
  // รีเซ็ต WiFi เมื่อกดปุ่มค้าง
  if (digitalRead(WIFI_RESET_BTN) == LOW) {
    delay(50); 
    if (digitalRead(WIFI_RESET_BTN) == LOW) {
      lcd.clear();
      lcd.print("WiFi Config Mode");
      WiFiManager wm;
      wm.setConfigPortalTimeout(180); 
      Serial.println("[WiFi] Button pressed. Opening SmartFarm_Setup portal (180 seconds).");
      if (!wm.startConfigPortal("SmartFarm_Setup")) {
        Serial.println("[WiFi] Portal failed or timed out. Restarting.");
        ESP.restart();
      }
      Serial.printf("[WiFi] Portal complete. Status=%d, IP=%s\n", WiFi.status(), WiFi.localIP().toString().c_str());
      lcd.clear();
    }
  }

  // รักษาสถานะเชื่อมต่อ MQTT
  if (!client.connected()) reconnect();
  client.loop(); 

  // ส่งข้อมูลทุก 5 วินาที
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("[DHT] Read failed. Check sensor wiring and power.");
      return;
    }
    Serial.printf("[DHT] Temperature=%.1f C, humidity=%.1f %%, pump GPIO=%d, valve GPIO=%d\n", t, h, digitalRead(RELAY_PUMP), digitalRead(RELAY_VALVE));

    lcd.setCursor(0, 0);
    lcd.print("T:" + String(t, 1) + "C P:" + String(digitalRead(RELAY_PUMP)));
    lcd.setCursor(0, 1);
    lcd.print("H:" + String(h, 1) + "% V:" + String(digitalRead(RELAY_VALVE)));

    StaticJsonDocument<256> doc;
    doc["temperature"] = t;
    doc["humidity"] = h;
    doc["pump_state"] = digitalRead(RELAY_PUMP);
    doc["valve_state"] = digitalRead(RELAY_VALVE);
    
    char msgBuffer[256];
    serializeJson(doc, msgBuffer);

    String topic_telemetry = base_topic + "telemetry";
    bool sent = client.publish(topic_telemetry.c_str(), msgBuffer);
    Serial.printf("[MQTT] Telemetry publish %s: topic=%s, payload=%s\n", sent ? "OK" : "FAILED", topic_telemetry.c_str(), msgBuffer);
  }
}

// =========================================================
// ฟังก์ชัน 3: reconnect()
// =========================================================
void reconnect() {
  while (!client.connected()) {
    Serial.printf("[MQTT] Connecting to %s:%d; WiFi status=%d\n", mqtt_server, mqtt_port, WiFi.status());
    String topic_status = base_topic + "status";
    String willMessage = "{\"status\":\"offline\"}"; 
    
    // ตั้งค่า LWT 
    if (client.connect(device_id.c_str(), NULL, NULL, topic_status.c_str(), 0, true, willMessage.c_str())) {
      String topic_cmd = base_topic + "cmd";
      Serial.println("[MQTT] Connected.");
      bool subscribed = client.subscribe(topic_cmd.c_str());
      Serial.printf("[MQTT] Subscribe %s: %s\n", subscribed ? "OK" : "FAILED", topic_cmd.c_str());
      
      // ทับ LWT ด้วยสถานะ online
      bool sent = client.publish(topic_status.c_str(), "{\"status\":\"online\"}", true);
      Serial.printf("[MQTT] Online status publish %s\n", sent ? "OK" : "FAILED");
    } else {
      Serial.printf("[MQTT] Connection failed, state=%d. Retry in 5 seconds.\n", client.state());
      delay(5000);
    }
  }
}

// =========================================================
// ฟังก์ชัน 4: mqttCallback()
// =========================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) messageTemp += (char)payload[i];
  Serial.printf("[MQTT] Received: topic=%s, payload=%s\n", topic, messageTemp.c_str());
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, messageTemp);
  if (error) {
    Serial.printf("[CMD] Invalid JSON: %s\n", error.c_str());
    return;
  }

  if (doc.containsKey("cmd") && doc.containsKey("state")) {
    String command = doc["cmd"].as<String>();
    int state = doc["state"].as<int>();

    if (command == "pump") digitalWrite(RELAY_PUMP, state);
    else if (command == "valve") digitalWrite(RELAY_VALVE, state);
    else Serial.printf("[CMD] Unknown command: %s\n", command.c_str());
    Serial.printf("[RELAY] Command=%s, requested=%d, pump GPIO=%d, valve GPIO=%d\n", command.c_str(), state, digitalRead(RELAY_PUMP), digitalRead(RELAY_VALVE));

    StaticJsonDocument<256> ackDoc;
    ackDoc["ack"] = command;          
    ackDoc["state_set"] = state;      
    
    char ackBuffer[256];
    serializeJson(ackDoc, ackBuffer); 
    
    String topic_ack = base_topic + "ack";
    bool sent = client.publish(topic_ack.c_str(), ackBuffer);
    Serial.printf("[MQTT] ACK publish %s: %s\n", sent ? "OK" : "FAILED", ackBuffer);
  } else {
    Serial.println("[CMD] Ignored: missing cmd or state field.");
  }
}
