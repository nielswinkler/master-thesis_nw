#include "CumulocityPlatform.h"
#include "SIM900.h"
#include <SoftwareSerial.h>

CumulocityPlatform cPlatform("<<host>>", "<<tenantId>>", "<<user>>", "<<password>>", "<<applicationKey>>");
GSMModule* mod;

int result;

/*
 * Replace <<deviceName>> and all parameters in sendMeasurement methods with desired values
 */

void setup() {
  Serial.begin(19200);
  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if(mod->attachGPRS("internet.t-mobile", "t-mobile", "tm")) {
    Serial.println(F("GPRS attached."));
    cPlatform.setGSM(mod);
    Serial.println(F("Registering a device..."));

  } else {
    Serial.println(F("Could not attach GPRS."));
    while(true);
  }
}

void loop() {
  Serial.println(F("Sending measurement"));
  result = cPlatform.sendMeasurement("LightMeasurement", "c8y_LightMeasurement","e", 50, "lux");
  if(result > 0) {
    Serial.println(F("Measurement sent successfully."));
  } else {
    Serial.println(F("Measurement sending failed."));
  }
  
  delay(2000);
}
