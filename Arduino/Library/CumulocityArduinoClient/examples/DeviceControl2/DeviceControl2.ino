#include "CumulocityPlatform.h"
#include "SIM900.h"
#include <SoftwareSerial.h>

/*
 *
 * !!! Device control feature is not working with avr-gcc 4.3.2
 * !!! Confirmed to be working with avr-gcc 4.7.2
 *
 */

CumulocityPlatform cPlatform("<<host>>", "<<tenantId>>", "<<user>>", "<<password>>", "<<applicationKey>>");
GSMModule* mod;

char operation[50];
char operationName[15];
int result;

/*
 * Replace DEVICE_NAME with desired value
 */

void setup() {
  Serial.begin(19200);
  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if(mod->attachGPRS("internet.t-mobile", "t-mobile", "tm")) {
    Serial.println(F("GPRS attached."));
    cPlatform.setGSM(mod);
    
    char id[8];
    Serial.println(F("Registering a device..."));
    result = cPlatform.registerDevice(DEVICE_NAME, id, 8);
    
    if(result<0) {
      Serial.println(F("Registration error."));
      while(true);
    } else {
      Serial.print(F("Arduino registered with id: "));
      Serial.println(id);
    }

    cPlatform.registerForServerOperation(executeOperation, operationName, 15, operation, 50, 300); // interval 5 minutes
  }
  else {
    Serial.println(F("status=IDLE"));
    while(true);
  }
}

void loop() {
  cPlatform.triggerHandlersIfCommandsAwaiting();
}

void executeOperation() {
  Serial.println(F("Operation received:"));
  Serial.println(operationName);
  Serial.println(operation);
  
  if((result = cPlatform.markOperationCompleted()) > 0) {
    Serial.println(F("Operation marked as completed"));
  } else {
    Serial.println(F("Operation update failed."));
  }
}
