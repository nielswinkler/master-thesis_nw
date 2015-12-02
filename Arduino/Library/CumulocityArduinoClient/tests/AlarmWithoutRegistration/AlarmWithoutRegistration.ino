#include "CumulocityPlatform.h"
#include "SIM900.h"
#include <SoftwareSerial.h>

CumulocityPlatform cPlatform("<<host>>", "<<tenantId>>", "<<user>>", "<<password>>", "<<applicationKey>>");
GSMModule* mod;

int result;

/*
 * Replace <<deviceName>> and parameters in raiseAlarm method with desired values
 */

void setup() {
  Serial.begin(19200);
  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if(mod->attachGPRS("internet.t-mobile", "t-mobile", "tm")) {
    Serial.println(F("GPRS attached."));
    cPlatform.setGSM(mod);
    
  } else {
    Serial.println(F("Could not attach GPRS."));
    while(true);
  }
}

void loop() {
  Serial.println(F("Raising alarm..."));
  int result = cPlatform.raiseAlarm("TYPE_1", "active", "major", "Something happened");
  if(result < 0) {
    Serial.println(F("Alarm failed."));
  } else {
    Serial.println(F("Alarm sent."));
  }
  
  delay(2000);
}
