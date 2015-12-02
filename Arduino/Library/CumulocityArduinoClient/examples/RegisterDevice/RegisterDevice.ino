#include "CumulocityPlatform.h"
#include "SIM900.h"
#include <SoftwareSerial.h>

CumulocityPlatform cPlatform("developer.cumulocity.com", "x52n002", "NielsWinkler", "Niwicum2011", "<<applicationKey>>");
GSMModule* mod;


/*
 * Replace DEVICE_NAME with desired value
 */

void setup() {
  Serial.begin(19200);
  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if(mod->attachGPRS("internet.t-mobile", "t-mobile", "tm")) {
    cPlatform.setGSM(mod);

    char id[8];
    const char* supportedOperations[2];
    const char* supportedMeasurements[1];
    supportedOperations[0] = "c8y_Relay";
    supportedOperations[1] = "c8y_Message";
    supportedMeasurements[0] = "c8y_LightMeasurement";

    Serial.println(F("Registering a device..."));
    int result = cPlatform.registerDevice(DEVICE_NAME_Test, id, 8, supportedOperations, 2, supportedMeasurements, 1);

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
}

void loop() {
}
