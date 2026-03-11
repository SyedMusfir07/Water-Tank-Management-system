#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "secrets.h"

// Pin definitions
#define PIN_TDS        34
#define PIN_TURBIDITY  35
#define PIN_DS18B20     4

// DS18B20 setup
OneWire oneWire(PIN_DS18B20);
DallasTemperature tempSensor(&oneWire);

// Settings
const int ADC_SAMPLES = 20;
const float ADC_MAX = 4095.0;
const float VREF = 3.3;

float TDS_LIMIT_PPM = 500.0;
float TURB_LIMIT_RAW = 2500.0;
float TEMP_LIMIT_C = 40.0;

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL_MS = 10000;

// WiFi
void connectWiFi() {
  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    if (millis() - start > 20000) break;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED");
  }
}

float readAdcAverage(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return (float)sum / (float)samples;
}

float adcToVoltage(float adcVal) {
  return (adcVal / ADC_MAX) * VREF;
}

float calcTdsPpm(float voltage, float tempC) {
  float compensation = 1.0 + 0.02 * (tempC - 25.0);
  float vComp = voltage / compensation;

  float tds = (133.42 * vComp * vComp * vComp
              - 255.86 * vComp * vComp
              + 857.39 * vComp) * 0.5;

  if (tds < 0) tds = 0;
  return tds;
}

String firebaseUrl(const String &path) {
  String url = "https://" + String(FIREBASE_HOST) + "/" + path + ".json";
  if (String(FIREBASE_AUTH).length() > 0) {
    url += "?auth=" + String(FIREBASE_AUTH);
  }
  return url;
}

bool firebasePut(const String &path, const String &json) {
  HTTPClient http;
  http.begin(firebaseUrl(path));
  http.addHeader("Content-Type", "application/json");

  int code = http.PUT(json);
  String resp = http.getString();
  http.end();

  Serial.print("Firebase PUT ");
  Serial.print(path);
  Serial.print(" -> ");
  Serial.println(code);

  if (code < 200 || code >= 300) {
    Serial.println("Firebase error response:");
    Serial.println(resp);
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  analogReadResolution(12);
  tempSensor.begin();
  connectWiFi();

  Serial.println("SWQMS Final Firmware Started");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();

    tempSensor.requestTemperatures();
    float tempC = tempSensor.getTempCByIndex(0);

    float tdsAdc = readAdcAverage(PIN_TDS, ADC_SAMPLES);
    float tdsV = adcToVoltage(tdsAdc);
    float tdsPPM = calcTdsPpm(tdsV, tempC);

    float turbAdc = readAdcAverage(PIN_TURBIDITY, ADC_SAMPLES);

    bool pollution = (tdsPPM > TDS_LIMIT_PPM) ||
                     (turbAdc > TURB_LIMIT_RAW) ||
                     (tempC > TEMP_LIMIT_C);

    Serial.println("----- SWQMS Reading -----");
    Serial.print("Temp (C): "); Serial.println(tempC, 2);
    Serial.print("TDS PPM: "); Serial.println(tdsPPM, 1);
    Serial.print("Turb Raw: "); Serial.println(turbAdc, 0);
    Serial.print("Pollution: "); Serial.println(pollution ? "TRUE" : "FALSE");

    unsigned long ts = millis();
    String json = "{";
    json += "\"temperature\":" + String(tempC, 2) + ",";
    json += "\"tds_ppm\":" + String(tdsPPM, 1) + ",";
    json += "\"turbidity_raw\":" + String(turbAdc, 0) + ",";
    json += "\"pollution\":" + String(pollution ? "true" : "false") + ",";
    json += "\"timestamp_ms\":" + String(ts);
    json += "}";

    firebasePut("latest", json);
    firebasePut("readings/" + String(ts), json);
  }
}
