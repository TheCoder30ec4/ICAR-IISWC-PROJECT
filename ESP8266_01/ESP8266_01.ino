#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// Configuration Constants
namespace Config {
  const int TCP_PORT = 12345;
  const char* SSID = "Varun";
  const char* PASSWORD = "Energy$2003";
  const unsigned long WIFI_TIMEOUT_MS = 10000;
  const unsigned long SENSOR_INTERVAL_MS = 1000;
  const int TRIGGER_DISTANCE_CM = -5;
  const int RELAY_PIN = 0;
  const bool RELAY_ACTIVE_STATE = LOW;
  const unsigned long RELAY_ACTIVE_DURATION_MS = 2000;
  const char* THINGSPEAK_API_KEY = "6H2HRD5JFWAACIPC";
  const char* THINGSPEAK_SERVER = "api.thingspeak.com";
  const unsigned long THINGSPEAK_MIN_INTERVAL_MS = 15000;
}

// State Management
struct SystemState {
  bool relayActive = false;
  unsigned long lastSensorCheck = 0;
  unsigned long lastThingSpeakUpdate = 0;
  unsigned long relayActivationTime = 0;
  bool ipReceived = false;
  String esp32Host;
};

SystemState state;
WiFiClient wifiClient;
WiFiServer tcpServer(Config::TCP_PORT);

void setup() {
  Serial.begin(115200);
  initializeRelay();
  initializeWiFi();
  tcpServer.begin();
  Serial.println("TCP server started");
}

void loop() {
  if (!state.ipReceived) {
    handleTCPUpdate();
  } else {
    maintainWiFiConnection();
    handleSensorCheck();
    handleRelayAutoTurnOff();
  }
}

void initializeRelay() {
  pinMode(Config::RELAY_PIN, OUTPUT);
  digitalWrite(Config::RELAY_PIN, !Config::RELAY_ACTIVE_STATE);
  Serial.println("Relay initialized");
}

void initializeWiFi() {
  WiFi.begin(Config::SSID, Config::PASSWORD);
  Serial.print("\nConnecting to WiFi");
  const unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < Config::WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void handleTCPUpdate() {
  WiFiClient client = tcpServer.available();
  if (client) {
    Serial.println("Client connected");
    String newIP = client.readStringUntil('\n');
    newIP.trim();
    state.esp32Host = "http://" + newIP + "/distance1";
    client.stop();
    state.ipReceived = true;
    Serial.print("Updated ESP32 host: ");
    Serial.println(state.esp32Host);
  }
}

void maintainWiFiConnection() {
  static unsigned long lastCheck = 0;
  const unsigned long checkInterval = 10000;
  if (millis() - lastCheck >= checkInterval) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      initializeWiFi();
    }
    lastCheck = millis();
  }
}

void handleSensorCheck() {
  if (millis() - state.lastSensorCheck >= Config::SENSOR_INTERVAL_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      checkDistance();
    }
    state.lastSensorCheck = millis();
  }
}

void checkDistance() {
  HTTPClient http;
  if (http.begin(wifiClient, state.esp32Host)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      processSensorResponse(http.getString());
    } else {
      Serial.printf("HTTP error: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Failed to connect to ESP32 sensor");
  }
}

void processSensorResponse(const String& payload) {
  StaticJsonDocument<64> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (!error) {
    int distance = doc["distance1"];
    Serial.printf("Received distance: %d cm\n", distance);
    if (distance <= Config::TRIGGER_DISTANCE_CM && !state.relayActive) {
      activateRelay();
    }
  } else {
    Serial.printf("JSON parsing failed: %s\n", error.c_str());
  }
}

void activateRelay() {
  digitalWrite(Config::RELAY_PIN, Config::RELAY_ACTIVE_STATE);
  state.relayActive = true;
  state.relayActivationTime = millis();
  Serial.println("Relay activated!");
  if (canUpdateThingSpeak()) {
    sendToThingSpeak(1);
  }
}

void handleRelayAutoTurnOff() {
  if (state.relayActive && millis() - state.relayActivationTime >= Config::RELAY_ACTIVE_DURATION_MS) {
    deactivateRelay();
  }
}

void deactivateRelay() {
  digitalWrite(Config::RELAY_PIN, !Config::RELAY_ACTIVE_STATE);
  state.relayActive = false;
  Serial.println("Relay deactivated");
  if (canUpdateThingSpeak()) {
    sendToThingSpeak(0);
  }
}

bool canUpdateThingSpeak() {
  return millis() - state.lastThingSpeakUpdate >= Config::THINGSPEAK_MIN_INTERVAL_MS;
}

void sendToThingSpeak(int status) {
  HTTPClient http;
  String url = String("http://") + Config::THINGSPEAK_SERVER + "/update?api_key=" + Config::THINGSPEAK_API_KEY + "&field4=" + String(status);
  if (http.begin(wifiClient, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      Serial.printf("ThingSpeak updated: %d\n", status);
      state.lastThingSpeakUpdate = millis();
    } else {
      Serial.printf("ThingSpeak error: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Failed to connect to ThingSpeak");
  }
}
