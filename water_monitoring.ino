/*
 * ============================================================
 *  Smart Water Quality Monitoring System
 *  Sensors : TDS (GPIO 34) | Turbidity (GPIO 35) | DS18B20 (GPIO 32)
 *  Platform: ESP32 + Arduino IDE
 *  Note    : Serial Monitor output only (no Firebase yet)
 * ============================================================
 */

#include <OneWire.h>
#include <DallasTemperature.h>

// ─── PIN DEFINITIONS ─────────────────────────────────────────
#define TDS_PIN       34    // TDS sensor analog output  → GPIO 34
#define TURBIDITY_PIN 35    // Turbidity sensor analog   → GPIO 35
#define ONE_WIRE_BUS  32    // DS18B20 data line         → GPIO 32

// ─── CONSTANTS ───────────────────────────────────────────────
#define VREF           3.3      // ESP32 ADC reference voltage
#define ADC_RESOLUTION 4095.0   // 12-bit ADC (0–4095)
#define SAMPLE_COUNT   20       // Samples averaged per reading

// ─── SAFETY THRESHOLDS ───────────────────────────────────────
#define TDS_THRESHOLD        500.0   // ppm — WHO limit for drinking water
#define TURBIDITY_THRESHOLD  5.0     // NTU — WHO upper acceptable limit
#define TEMP_MIN             10.0    // °C
#define TEMP_MAX             25.0    // °C

// ─── SENSOR OBJECTS ──────────────────────────────────────────
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ─── FUNCTION DECLARATIONS ───────────────────────────────────
float readAverageVoltage(int pin);
float calculateTDS(float voltage, float temp);
float mapTurbidity(float voltage);
bool  isPolluted(float tds, float turbidity, float temp);

// ============================================================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);   // Set ESP32 ADC to 12-bit
  sensors.begin();

  Serial.println("\n=== Smart Water Quality Monitor ===");
  Serial.println("Initializing sensors...");
  delay(1000);
  Serial.println("Ready!\n");
}

// ============================================================
void loop() {

  // ── 1. TEMPERATURE ─────────────────────────────────────────
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("WARNING: DS18B20 not detected! Using 25°C default.");
    temperature = 25.0;
  }

  // ── 2. TDS ─────────────────────────────────────────────────
  float tdsVoltage = readAverageVoltage(TDS_PIN);
  float tdsValue   = calculateTDS(tdsVoltage, temperature);

  // ── 3. TURBIDITY ───────────────────────────────────────────
  float turbidityVoltage = readAverageVoltage(TURBIDITY_PIN);
  float turbidityNTU     = mapTurbidity(turbidityVoltage);

  // ── 4. POLLUTION STATUS ────────────────────────────────────
  bool polluted = isPolluted(tdsValue, turbidityNTU, temperature);

  // ── 5. PRINT TO SERIAL MONITOR ─────────────────────────────
  Serial.println("──── Water Quality Readings ────");
  Serial.print("Temperature : "); Serial.print(temperature, 1);    Serial.println(" °C");
  Serial.print("TDS         : "); Serial.print(tdsValue,   0);     Serial.println(" ppm");
  Serial.print("Turbidity   : "); Serial.print(turbidityNTU, 1);   Serial.println(" NTU");
  Serial.print("Turb Voltage: "); Serial.print(turbidityVoltage, 2); Serial.println(" V  (debug)");
  Serial.print("Status      : "); Serial.println(polluted ? "*** POLLUTED ***" : "SAFE");
  Serial.println("────────────────────────────────\n");

  delay(5000);   // Wait 5 seconds between readings
}

// ============================================================
//  HELPER FUNCTIONS
// ============================================================

// Takes SAMPLE_COUNT ADC readings from a pin,
// averages them to reduce electrical noise, then
// converts the result to a voltage value.
float readAverageVoltage(int pin) {
  float total = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    total += analogRead(pin);
    delay(5);
  }
  float averageRaw = total / (float)SAMPLE_COUNT;
  return (averageRaw * VREF) / ADC_RESOLUTION;
}

// Converts TDS sensor voltage to ppm (parts per million).
// Temperature compensation applied because water conducts
// electricity better at higher temperatures (~2% per °C).
float calculateTDS(float voltage, float temp) {
  float compensationCoefficient = 1.0 + 0.02 * (temp - 25.0);
  float compensatedVoltage      = voltage / compensationCoefficient;

  // Standard polynomial calibration curve for analog TDS probes
  float tds = (133.42 * pow(compensatedVoltage, 3)
              - 255.86 * pow(compensatedVoltage, 2)
              + 857.39 * compensatedVoltage) * 0.5;

  return (tds < 0) ? 0 : tds;
}

// Converts turbidity sensor voltage to NTU.
// Higher voltage = clearer water (inverse relationship).
float mapTurbidity(float voltage) {
  if (voltage > 2.5) return 0;   // Above 2.5V = clear water

  float ntu = -1120.4 * sq(voltage)
              + 5742.3 * voltage
              - 4352.9;

  return (ntu < 0) ? 0 : ntu;
}

// Returns true if ANY parameter is outside its safe range.
bool isPolluted(float tds, float turbidity, float temp) {
  return (tds       > TDS_THRESHOLD)       ||
         (turbidity > TURBIDITY_THRESHOLD) ||
         (temp      < TEMP_MIN)            ||
         (temp      > TEMP_MAX);
}
