#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// WiFi Credentials
const char* ssid = "LIVE";
const char* password = "123456789";

// Web Server
WebServer server(80);

// ThingSpeak Configuration
const char* thingspeakServer = "api.thingspeak.com";
const String apiKey = "6H2HRD5JFWAACIPC";
#define THINGSPEAK_INTERVAL pdMS_TO_TICKS(5000)

// Sensor Data (with mutex protection)
SemaphoreHandle_t dataMutex;
volatile float distance1_cm = 0.0;
volatile float distance2_cm = 0.0;
volatile int soilMoisture = 0;
volatile bool distance1Valid = false;
volatile bool distance2Valid = false;

// Hardware Serial for Sensors
HardwareSerial sensorSerial1(1);
HardwareSerial sensorSerial2(2);

// Pin Assignments
#define UART1_RXD 18
#define UART1_TXD 19
#define UART2_RXD 16
#define UART2_TXD 17
#define SOIL_MOISTURE_PIN 34

// Soil Sensor Calibration
#define SOIL_DRY 4095
#define SOIL_WET 1500

// Task Handles
TaskHandle_t sensorTaskHandle, serverTaskHandle, thingspeakTaskHandle;

void setup() {
  Serial.begin(115200);
  dataMutex = xSemaphoreCreateMutex();
  
  // Initialize UARTs
  sensorSerial1.begin(9600, SERIAL_8N1, UART1_RXD, UART1_TXD);
  sensorSerial2.begin(9600, SERIAL_8N1, UART2_RXD, UART2_TXD);
  sensorSerial1.setRxBufferSize(64);
  sensorSerial2.setRxBufferSize(64);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(250);
    Serial.print(".");
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed!");
  }

  // Create tasks
  xTaskCreatePinnedToCore(
    sensorTask,    // Task function
    "SensorTask",  // Task name
    4096,          // Stack size
    NULL,          // Parameters
    3,             // Priority (highest)
    &sensorTaskHandle, 
    0              // Core 0
  );

  xTaskCreatePinnedToCore(
    serverTask,
    "ServerTask",
    4096,
    NULL,
    1,             // Priority (lowest)
    &serverTaskHandle,
    0
  );

  xTaskCreatePinnedToCore(
    thingspeakTask,
    "ThingSpeakTask",
    4096,
    NULL,
    2,             // Priority (medium)
    &thingspeakTaskHandle,
    0
  );

  // Delete default Arduino loop task
  vTaskDelete(NULL);
}

void loop() {} // FreeRTOS scheduler takes over

void sensorTask(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);
  
  while(1) {
    Serial.println("[Sensor Task] Starting sensor read...");
    
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Original sensor reading logic
    static uint32_t lastSoilRead = 0;
    processSensorPacket(sensorSerial1, &distance1_cm, &distance1Valid);
    processSensorPacket(sensorSerial2, &distance2_cm, &distance2Valid);
    
    if(millis() - lastSoilRead >= 1000) {
      soilMoisture = analogRead(SOIL_MOISTURE_PIN);
      lastSoilRead = millis();
      
      int percentage = map(soilMoisture, SOIL_DRY, SOIL_WET, 0, 100);
      percentage = constrain(percentage, 0, 100);
      Serial.printf("[Sensor Task] Soil: %d%% | U1: %.1f | U2: %.1f\n", 
                   percentage, distance1_cm, distance2_cm);
    }
    
    xSemaphoreGive(dataMutex);
    
    Serial.println("[Sensor Task] Completed");
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void serverTask(void *pvParameters) {
  server.on("/distance1", []() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    String response = distance1Valid ? String(distance1_cm) : "Error";
    xSemaphoreGive(dataMutex);
    server.send(200, "text/plain", response);
    Serial.println("[Server Task] Handled request");
  });
  
  server.begin();
  
  while(1) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void thingspeakTask(void *pvParameters) {
  WiFiClient client;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  
  while(1) {
    Serial.println("[ThingSpeak Task] Starting update...");
    
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Original ThingSpeak logic
    int moisturePercentage = map(soilMoisture, SOIL_DRY, SOIL_WET, 0, 100);
    moisturePercentage = constrain(moisturePercentage, 0, 100);
    float validU1 = (distance1_cm < 0) ? 0.0 : distance1_cm;
    float validU2 = (distance2_cm < 0) ? 0.0 : distance2_cm;
    
    xSemaphoreGive(dataMutex);

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[ThingSpeak Task] WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    }

    if (client.connect(thingspeakServer, 80)) {
      String url = "/update?api_key=" + apiKey;
      url += "&field1=" + String(moisturePercentage);
      url += "&field2=" + String(validU1);
      url += "&field3=" + String(validU2);

      client.print(String("GET ") + url + " HTTP/1.1\r\n");
      client.print("Host: " + String(thingspeakServer) + "\r\n");
      client.print("Connection: close\r\n\r\n");

      Serial.println("[ThingSpeak Task] Sent: " + url);
      
      // Read response
      unsigned long timeout = millis();
      while (client.connected() && millis() - timeout < 5000) {
        if (client.available()) {
          String line = client.readStringUntil('\r');
          Serial.print("[ThingSpeak Response] " + line);
          break;
        }
      }
      client.stop();
    }
    
    Serial.println("[ThingSpeak Task] Completed");
    vTaskDelayUntil(&xLastWakeTime, THINGSPEAK_INTERVAL);
  }
}

void processSensorPacket(HardwareSerial &serial, volatile float *distance, volatile bool *valid) {
  uint8_t buffer[4];
  if(serial.available() >= 4) {
    serial.readBytes(buffer, 4);
    if(buffer[0] == 0xFF) {
      uint8_t checksum = buffer[0] + buffer[1] + buffer[2];
      if(buffer[3] == checksum) {
        float raw = ((buffer[1] << 8) | buffer[2]) / 10.0;
        
        if(distance == &distance2_cm) {
          *distance = (32.0 - raw) >= 0 ? (32.0 - raw) : 0.0;
        } else {
          *distance = raw <= 10 ? raw : -(raw - 10);
        }
        
        *valid = true;
        return;
      }
    }
    *valid = false;
  }
}
