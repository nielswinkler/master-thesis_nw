#include "CumulocityPlatform.h"
#include "SIM900.h"
#include <SoftwareSerial.h>
#include "DHT.h"

CumulocityPlatform cPlatform("developer.cumulocity.com", "x52n002", "NielsWinkler", "Niwicum2011", "<<applicationKey>>");
GSMModule* mod;

#define DHTPIN A3
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
int result;

/*
 * Replace DEVICE_NAME and all parameters in sendMeasurement methods with desired values
 */

void setup() {
  Serial.begin(19200);
  dht.begin();
  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if(mod->attachGPRS("internet.t-d1.de", "", "")) {
    Serial.println(F("GPRS attached."));
    cPlatform.setGSM(mod);

    char id[8];
    const char* supportedOperations[2];
    const char* supportedMeasurements[1];
    supportedOperations[0] = "c8y_Relay";
    supportedOperations[1] = "c8y_Message";
    supportedMeasurements[0] = "c8y_LightMeasurement";
    Serial.println(F("Registering a device..."));
    int result = cPlatform.registerDevice("POTWECH_TEST_DEVICE", 10800, 8, supportedOperations, 2, supportedMeasurements, 1);

    if(result<0) {
      Serial.println(F("Registration error."));
      while(true);
    } else {
      Serial.print(F("Arduino registered with id: "));
      Serial.println(id);
    }
  } else {
    Serial.println(F("Could not attach GPRS."));
    while(true);
  }
  
  sendMeasurements();
};

void loop() {
}

void sendMeasurements() {
  for(int i=0; i<5; i++) {
    float t = dht.readTemperature();
    Serial.println(F("Sending measurement"));
    result = cPlatform.sendMeasurement("Temperatur",  "e", t, "lx");
    if(result > 0) {
      Serial.println(F("Measurement sent successfully."));
    } else {
      Serial.println(F("Measurement sending failed."));
    }
    delay(300000UL); // 5 minutes
  }
}
