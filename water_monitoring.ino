
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin Definitions
#define TDS_PIN 34
#define TURBIDITY_PIN 35
#define ONE_WIRE_BUS 4

// Temperature Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Constants
#define VREF 3.3          // ESP32 ADC reference voltage
#define ADC_RESOLUTION 4095.0  // 12-bit ADC

void setup() {
  Serial.begin(115200);
  sensors.begin();
}

void loop() {

  // ---------- TEMPERATURE ----------
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  // ---------- TDS SENSOR ----------
  float tdsVoltage = readAverageVoltage(TDS_PIN);
  float tdsValue = calculateTDS(tdsVoltage, temperature);

  // ---------- TURBIDITY SENSOR ----------
  float turbidityVoltage = readAverageVoltage(TURBIDITY_PIN);
  float turbidityNTU = mapTurbidity(turbidityVoltage);

  // ---------- PRINT RESULTS ----------
  Serial.println("----- Water Quality Data -----");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("TDS: ");
  Serial.print(tdsValue);
  Serial.println(" ppm");

  Serial.print("Turbidity: ");
  Serial.print(turbidityNTU);
  Serial.println(" NTU");

  Serial.println("-------------------------------\n");

  delay(5000);
}


// Function to read averaged voltage
float readAverageVoltage(int pin) {
  float total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(pin);
    delay(10);
  }
  float average = total / 10.0;
  return (average * VREF) / ADC_RESOLUTION;
}


// TDS Calculation (with temperature compensation)
float calculateTDS(float voltage, float temperature) {

  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensatedVoltage = voltage / compensationCoefficient;

  float tdsValue = (133.42 * compensatedVoltage * compensatedVoltage * compensatedVoltage
                    - 255.86 * compensatedVoltage * compensatedVoltage
                    + 857.39 * compensatedVoltage) * 0.5;

  return tdsValue;
}


// Basic Turbidity Mapping
float mapTurbidity(float voltage) {
  float ntu = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
  if (ntu < 0) ntu = 0;
  return ntu;
}