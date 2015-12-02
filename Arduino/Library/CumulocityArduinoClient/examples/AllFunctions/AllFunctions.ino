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

int result;
char operation[50];
char operationName[15];

/*
 * Replace DEVICE_NAME and all parameters in sendMeasurement methods with desired values
 */

void setup() {
  Serial.begin(19200);
  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if(mod->attachGPRS("internet.t-mobile", "t-mobile", "tm")) {
    Serial.println(F("GPRS attached."));
    cPlatform.setGSM(mod);
    
    registerDevice();
    
  } else {
    Serial.println(F("Could not attach GPRS."));
    while(true);
  }
};

void loop() {
  sendMeasurement();
  raiseAlarm();
  checkOperation();
}

void registerDevice() {
  Serial.println(F("Registering a device..."));
  char id[8];
  const char* supportedOperations[1];
  supportedOperations[0] = "LED_control";    
  const char*  supportedMeasurements[1];
  supportedMeasurements[0] = "c8y_LightMeasurement";
  result = cPlatform.registerDevice(DEVICE_NAME, id, 8, supportedOperations, 1, supportedMeasurements, 1);
  
  if(result<0) {
    Serial.println(F("Registration error."));
    while(true);
  } else {
    Serial.print(F("Arduino registered with id: "));
    Serial.println(id);
  }
}

void sendMeasurement() {
  Serial.println(F("Sending measurement"));
  result = cPlatform.sendMeasurement("Light", "c8y_LightMeasurement", "e", 50, "lx");
  if(result > 0) {
    Serial.println(F("Measurement sent successfully."));
  } else {
    Serial.println(F("Measurement sending failed."));
  }
}

void raiseAlarm() {
  Serial.println(F("Raising alarm..."));
  int result = cPlatform.raiseAlarm("TYPE_1", "active", "major", "Something happened");
  if(result < 0) {
    Serial.println(F("Alarm failed."));
  } else {
    Serial.println(F("Alarm sent."));
  }
}

void checkOperation() {
  Serial.println(F("\nRetrieving operation from server..."));
  if((result = cPlatform.getPendingOperationFromServer(operationName, 15, operation, 50)) > 0) {
    executeOperation();

    if((result = cPlatform.markOperationCompleted()) > 0) {
      Serial.println(F("Operation marked as completed"));
    } else {
      Serial.println(F("Operation update failed."));
    }
  } else if(result < 0) {
    Serial.println(F("Operation retrieval failed."));
  } else {
    Serial.println(F("No operation awaiting."));
  }
}

void executeOperation() {
  Serial.println(F("Executing operation:"));
  Serial.println(operationName);
  Serial.println(operation);
  // parse the operation JSON
  // and turn on the LED or print character on display...
}
