#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <MQUnifiedsensor.h>
#include <DHT.h>
#include "DSM501.h"

#define Board "ESP-32"

#define MQ7_Pin 34
#define MQ135_Pin 35
#define DSM501_PM1_0  32
#define DSM501_PM2_5  33
#define SAMPLE_TIME   30  // seconds

#define Voltage_Resolution 3.3 
#define ADC_Bit_Resolution 12 
#define RatioMQ7CleanAir 26.0
#define RatioMQ135CleanAir 3.6
#define DHTPIN 4
#define DHTTYPE DHT11

const char* ssid = "root";
const char* password = "12345678";

MQUnifiedsensor MQ7(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ7_Pin, "MQ-7");
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ135_Pin, "MQ-135");
DHT dht(DHTPIN, DHTTYPE);
DSM501 dsm501;

float calculateIndoorAQI(float co, float co2, float pm25, float temperature, float humidity) {
  float normCO = constrain(map(co, 0, 9, 10, 0), 0, 10);
  float normCO2 = constrain(map(co2, 400, 2000, 10, 0), 0, 10); 
  float normPM25 = constrain(map(pm25, 0, 35.5, 10, 0), 0, 10); 
  float normTemp = constrain(map(temperature, 18, 24, 10, 0), 0, 10); 
  float normHumidity = constrain(map(humidity, 30, 60, 10, 0), 0, 10); 
  
  // Weights 
  float weightCO = 0.3;
  float weightCO2 = 0.3;
  float weightPM25 = 0.2;
  float weightTemp = 0.1;
  float weightHumidity = 0.1;

  // Weighted sum of normalized values
  float aqi = (normCO * weightCO) + 
              (normCO2 * weightCO2) +
              (normPM25 * weightPM25) + 
              (normTemp * weightTemp) + 
              (normHumidity * weightHumidity);
              
  return aqi;
}

void setup() {
  Serial.begin(9600); 
  delay(10);

  MQ7.setRegressionMethod(1); 
  MQ7.setA(99.042); 
  MQ7.setB(-1.518); 

  MQ135.setRegressionMethod(1); 
  MQ135.setA(110.47);
  MQ135.setB(-2.862); 
  
  MQ7.init();
  MQ135.init();

  Serial.print("Calibrating please wait.");
  float calcR0_MQ7 = 0;
  float calcR0_MQ135 = 0;

  for(int i = 1; i<=10; i ++)
  {
    MQ7.update();
    MQ135.update();

    calcR0_MQ7 += MQ7.calibrate(RatioMQ7CleanAir);
    calcR0_MQ135 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ7.setR0(calcR0_MQ7/10);
  MQ135.setR0(calcR0_MQ135/10);
  Serial.println("  done!.");
  
  if(isinf(calcR0_MQ7) || isinf(calcR0_MQ135)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected) please check your wiring and supply");
    while(1);
  }
  if(calcR0_MQ7 == 0 || calcR0_MQ135 == 0) {
    Serial.println("Warning: Connection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply");
    while(1);
  }

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  dht.begin();

  // Initialize DSM501
  dsm501.begin(DSM501_PM1_0, DSM501_PM2_5, SAMPLE_TIME);
  
  Serial.println("Wait 60s for DSM501 to warm up"); 
  for (int i = 1; i <= 60; i++) {
    delay(1000);
    Serial.print(i);
    Serial.println(" s (wait 60s for DSM501 to warm up)");
  }

  Serial.println("DSM501 is ready!");
  Serial.println();
}

void loop() {
  if (dsm501.update()) {
    MQ7.update();
    MQ135.update();

    float co = MQ7.readSensor();
    float co2 = MQ135.readSensor();

    Serial.print("MQ-7 CO Reading: ");
    Serial.print(co);
    Serial.println(" PPM");

    Serial.print("MQ-135 CO2 Reading: ");
    Serial.print(co2);
    Serial.println(" PPM");

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.println(" *C ");
    }


    Serial.print("PM1.0 particle count: ");
    Serial.print(dsm501.getParticleCount(0));
    Serial.println(" parts/283mL");
    Serial.print("PM2.5 particle count: ");
    Serial.print(dsm501.getParticleCount(1));
    Serial.println(" parts/283mL");
    float concentration = dsm501.getConcentration();
    Serial.print("PM1.0 ~ PM2.5 concentration: ");
    Serial.print(concentration);
    Serial.println(" ug/m3");
    Serial.println();
    Serial.println();

    // Calculate and print AQI
    float aqi = calculateIndoorAQI(co, co2, concentration, t, h);
    Serial.print("Indoor Air Quality Index: ");
    Serial.println(aqi);
  }
  
  delay(1000);
}