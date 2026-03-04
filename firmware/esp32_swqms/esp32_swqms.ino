#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "secrets.h"   // <-- create secrets.h locally (from secrets.example.h)

// ---------------- PINS ----------------
#define PIN_TDS        34   // ADC input
#define PIN_TURBIDITY  35   // ADC input
#define PIN_DS18B20     4   // Digital

// ---------------- DS18B20 ----------------
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

// ---------------- SETTINGS ----------------
const int   ADC_SAMPLES = 20;
const float ESP32_ADC_MAX = 4095.0;
const float ESP32_VREF    = 3.3;     // ESP32 ADC reference (0-3.3V)

// Turbidity voltage divider (as you planned)
// If you used: AO -> 20k -> ADC -> 10k -> GND
// Then divider ratio = (Rtop + Rbottom) / Rbottom = 30k/10k = 3.0
const float TURB_DIV_RATIO = 3.0;

// Thresholds (you can later move these to Firebase settings)
float TDS_LIMIT_PPM = 500.0;
float TURB_LIMIT_NTU = 5.0;
float TEMP_LIMIT_C = 40.0;

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL_MS = 10000; // 10 seconds

// ---------------- HELPERS ----------------
float readAdcAverage(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(15);
  }
  return (float)sum / (float)samples;
}

float adcToVoltage(float adcValue) {
  return (adcValue / ESP32_ADC_MAX) * ESP32_VREF;
}

// TDS calculation (common polynomial used in many projects)
// NOTE: Needs calibration for best accuracy.
float calculateTDSppm(float voltage, float temperatureC) {
  // Temperature compensation (approx 2% per °C from 25°C)
  float compensationCoefficient = 1.0 + 0.02 * (temperatureC - 25.0);
  float compensatedVoltage = voltage / compensationCoefficient;

  // Polynomial conversion
  float tds = (133.42 * compensatedVoltage * compensatedVoltage * compensatedVoltage
              - 255.86 * compensatedVoltage * compensatedVoltage
              + 857.39 * compensatedVoltage) * 0.5;

  if (tds < 0) tds = 0;
  return tds;
}

// Turbidity NTU formula (commonly used for SEN0189-style modules)
// Uses sensor output voltage (usually 0~4.5V range).
float calculateNTU(float sensorVoltage) {
  // Clamp range
  if (sensorVoltage < 0) sensorVoltage = 0;
  if (sensorVoltage > 4.5) sensorVoltage = 4.5;

  // Standard curve
  float ntu = -1120.4 * sensorVoltage * sensorVoltage
              + 5742.3 * sensorVoltage
              - 4352.9;

  if (ntu < 0) ntu = 0;
  return ntu;
}

// Build Firebase REST URL
String firebaseUrl(const String &path) {
  // Example: https://<host>/<path>.json?auth=<token>
  String url = "https://" + String(FIREBASE_HOST) + "/" + path + ".json";
  if (String(FIREBASE_AUTH).length() > 0) {
    url += "?auth=" + String(FIREBASE_AUTH);
  }
  return url;
}

// PUT JSON to Firebase
bool firebasePutJson(const String &path, const String &json) {
  HTTPClient http;
  String url = firebaseUrl(path);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(json);
  String resp = http.getString();
  http.end();

  Serial.print("Firebase PUT ");
  Serial.print(path);
  Serial.print(" -> HTTP ");
  Serial.println(code);

  if (code < 200 || code >= 300) {
    Serial.println("Response: " + resp);
    return false;
  }
  return true;
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 20000) break;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED. Check SSID/PASS.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // ESP32 ADC config
  analogReadResolution(12);   // 0-4095
  // Optional: analogSetAttenuation(ADC_11db); // if your board supports it

  sensors.begin();

  connectWiFi();
}

void loop() {
  // Keep WiFi alive
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();

    // 1) Temperature
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    // 2) Read ADC average
    float tdsAdc = readAdcAverage(PIN_TDS, ADC_SAMPLES);
    float turbAdc = readAdcAverage(PIN_TURBIDITY, ADC_SAMPLES);

    // 3) Convert to voltages
    float tdsVoltage = adcToVoltage(tdsAdc);        // 0-3.3V
    float turbAdcVoltage = adcToVoltage(turbAdc);   // 0-3.3V at ESP32 pin

    // Convert ADC voltage back to sensor output voltage (due to divider)
    float turbSensorVoltage = turbAdcVoltage * TURB_DIV_RATIO;

    // 4) Calculate values
    float tdsPPM = calculateTDSppm(tdsVoltage, tempC);
    float turbNTU = calculateNTU(turbSensorVoltage);

    // 5) Pollution flag
    bool pollution = (tdsPPM > TDS_LIMIT_PPM) ||
                     (turbNTU > TURB_LIMIT_NTU) ||
                     (tempC > TEMP_LIMIT_C);

    // 6) Print for debugging (VERY important for viva)
    Serial.println("----- SWQMS Reading -----");
    Serial.print("Temp (C): "); Serial.println(tempC, 2);
    Serial.print("TDS ADC: "); Serial.print(tdsAdc, 0);
    Serial.print(" | V: "); Serial.print(tdsVoltage, 3);
    Serial.print(" | PPM: "); Serial.println(tdsPPM, 1);

    Serial.print("Turb ADC: "); Serial.print(turbAdc, 0);
    Serial.print(" | ADC_V: "); Serial.print(turbAdcVoltage, 3);
    Serial.print(" | Sensor_V: "); Serial.print(turbSensorVoltage, 3);
    Serial.print(" | NTU: "); Serial.println(turbNTU, 2);

    Serial.print("Pollution: "); Serial.println(pollution ? "TRUE" : "FALSE");

    // 7) Build JSON payload
    // (Simple JSON string, no extra library needed)
    unsigned long ts = millis();
    String json = "{";
    json += "\"temperature\":" + String(tempC, 2) + ",";
    json += "\"tds_ppm\":" + String(tdsPPM, 1) + ",";
    json += "\"turbidity_ntu\":" + String(turbNTU, 2) + ",";
    json += "\"pollution\":" + String(pollution ? "true" : "false") + ",";
    json += "\"timestamp_ms\":" + String(ts);
    json += "}";

    // 8) Upload to Firebase
    // A) latest (for dashboard live view)
    firebasePutJson("latest", json);

    // B) readings with a unique key (history)
    // Use millis as key (simple). You can use real time later.
    firebasePutJson("readings/" + String(ts), json);
  }
}
