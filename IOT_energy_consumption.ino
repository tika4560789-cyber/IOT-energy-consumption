#include <WiFi.h>
#include <WiFiClientSecure.h>   
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "xxxx"; // wifi name 
const char* password = "xxxxxxxx"; // wifi password

// MQTT Topic 
const char* mqtt_topic = "energy_meter/data"; // MQTT topic

//  HiveMQTT credential
const char* mqtt_server = "xxxxxxxxxxxxxxxxxxxxxx.s1.eu.hivemq.cloud"; // MQTT server
const int mqtt_port = 8883; // Port number
const char* mqtt_user = "xxxxxx"; // MQTT username
const char* mqtt_password = "xxxxxxx"; // MQTT password

// Pin Definitions
#define VOLTAGE_PIN 34   // ZMPT101B output
#define CURRENT_PIN 35   // ACS712 output

// Your Calibration Constants
// ZMPT101B Voltage
#define VOLTAGE_CALIBRATION  500   // Adjust to match your multimeter

// ACS712 Current (20A version)
#define ACS712_VCC  5.0
#define CURRENT_ZERO_POINT  (ACS712_VCC / 2.0)   // 2.5V for 5V supply
#define CURRENT_SENSITIVITY  0.100              // 100 mV/A for 20A module

#define ADC_REFERENCE   3.3
#define ADC_RESOLUTION  4095.0   // 12‑bit ADC

// Global Variables
WiFiClientSecure espClient;   
PubSubClient client(espClient);

float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;               // Wh (published as kWh)
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 1000;    // 1 second
unsigned long lastPublishTime = 0;
const unsigned long PUBLISH_INTERVAL = 2000;   // 2 seconds

// Moving average filters (smoothing)
const int NUM_SAMPLES = 20;
float voltageSamples[NUM_SAMPLES];
float currentSamples[NUM_SAMPLES];
int sampleIndex = 0;

// Setup
void setup() {
  Serial.begin(115200);
  
  // ADC 12‑bit
  analogReadResolution(12);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);

  // Initialize sample arrays
  for (int i = 0; i < NUM_SAMPLES; i++) {
    voltageSamples[i] = 0;
    currentSamples[i] = 0;
  }

  connectToWiFi();

  // MQTT setup with TLS
  client.setServer(mqtt_server, mqtt_port);
  espClient.setInsecure();   
  client.setCallback(mqttCallback);

  Serial.println("\n========================================");
  Serial.println("Energy Meter Starting...");
  Serial.print("MQTT Broker: "); Serial.println(mqtt_server);
  Serial.print("MQTT Topic: "); Serial.println(mqtt_topic);
  Serial.println("Voltage: ZMPT101B, Current: ACS712 (20A)");
  Serial.println("========================================\n");
}

// Loop
void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  if (millis() - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = millis();

    // Read sensors
    readVoltage();
    readCurrent();

    // Calculate power and energy
    power = voltage * current;
    energy += (power * (SAMPLE_INTERVAL / 3600000.0));   // Wh

    printData();

    if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
      lastPublishTime = millis();
      publishMQTT();
    }
  }
}

// WiFi Connection
void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi connected, IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n WiFi failed – restarting...");
    ESP.restart();
  }
}

// MQTT
void reconnectMQTT() {
  int attempts = 0;
  while (!client.connected() && attempts < 10) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32_Energy_" + String(random(0xffff), HEX);

    // For HiveMQ Cloud with TLS (username/password required)
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println(" connected");
      client.subscribe("energy/command");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 5s");
      delay(5000);
      attempts++;
    }
  }

}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  if (String(topic) == "energy/command" && message == "RESET_ENERGY") {
    energy = 0;
    Serial.println("Energy reset");
  }
}

// Sensor Reading
void readVoltage() {
  float maxVal = 0, minVal = 4095;
  int valid = 0;
  for (int i = 0; i < 100; i++) {
    int raw = analogRead(VOLTAGE_PIN);
    if (raw > 50 && raw < 4045) {   // Ignore extreme noise
      if (raw > maxVal) maxVal = raw;
      if (raw < minVal) minVal = raw;
      valid++;
    }
    delayMicroseconds(200);
  }
  if (valid > 50) {
    // Peak‑to‑peak → RMS
    float peakVoltage = (maxVal - minVal) * (ADC_REFERENCE / ADC_RESOLUTION) / 2.0;
    float rms = peakVoltage / sqrt(2);
    // Apply your calibration factor
    voltage = rms * VOLTAGE_CALIBRATION;
    // Smooth
    voltageSamples[sampleIndex] = voltage;
    float sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) sum += voltageSamples[i];
    voltage = sum / NUM_SAMPLES;
    if (voltage < 2) voltage = 0;
  }
 
}

void readCurrent() {
  float sumSq = 0;
  int valid = 0;
  for (int i = 0; i < 100; i++) {
    int raw = analogRead(CURRENT_PIN);
    float vOut = raw * (ADC_REFERENCE / ADC_RESOLUTION);
    float currentVal = (vOut - CURRENT_ZERO_POINT) / CURRENT_SENSITIVITY;
    sumSq += currentVal * currentVal;
    valid++;
    delayMicroseconds(200);
  }
  if (valid > 50) {
    current = sqrt(sumSq / valid);   // RMS
    currentSamples[sampleIndex] = current;
    float sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) sum += currentSamples[i];
    current = sum / NUM_SAMPLES;
    if (current < 0.005) current = 0;
  }
  // Update the sample index once after both readings
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
}

// Publish
void publishMQTT() {
  StaticJsonDocument<256> doc;
  doc["voltage"] = round(voltage * 100) / 100.0;
  doc["current"] = round(current * 1000) / 1000.0;
  doc["power"] = round(power * 100) / 100.0;
  doc["energy"] = round(energy / 1000.0 * 1000) / 1000.0;  // Wh → kWh, 3 decimals
  doc["timestamp"] = millis();
  doc["device"] = "ESP32_EnergyMeter";

  char jsonBuffer[256];
  size_t n = serializeJson(doc, jsonBuffer);

  if (client.publish(mqtt_topic, jsonBuffer, n)) {
    Serial.println(" MQTT published");
  } else {
    Serial.println(" MQTT publish failed");
  }
}

// Serial Output
void printData() {
  Serial.print("V="); Serial.print(voltage, 1); Serial.print(" V, ");
  Serial.print("I="); Serial.print(current, 3); Serial.print(" A, ");
  Serial.print("P="); Serial.print(power, 1); Serial.print(" W, ");
  Serial.print("E="); Serial.print(energy / 1000.0, 3); Serial.println(" kWh");
}