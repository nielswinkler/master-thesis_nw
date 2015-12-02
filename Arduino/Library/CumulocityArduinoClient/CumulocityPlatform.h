/*
 * CumulocityPlatform.h
 *
 * Created on: Jan 2, 2013
 * Authors: Adam Pierzchała
 *          Oliver Stache
 *
 * Copyright (c) 2013 Cumulocity GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef CUMULOCITYPLATFORM_H
#define CUMULOCITYPLATFORM_H

#define CUMULOCITY_CLIENT_VERSION 590

#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "inetGSM.h"

class GSMModule {

    // TODO keep state of connection
    // attach/detach -> connect/disconnect

protected:
    InetGSM inet;

public:
    GSMModule();
    virtual ~GSMModule();

    /**
      * Initiates the connection using the GPRS APN passed as the first parameter.
      * The second and third arguments are two strings that contain the username and password.
      * If no authentication is required, just pass the last two strings empty.
      */
    int attachGPRS(char*, char*, char*);
	int atWrite(char* atcommand);
    /**
      * Disconnects the module from the GPRS network.
      */
    int dettachGPRS();

    int connectTCP(const char* host, int port);
    void disconnectTCP();

    InetGSM getInet();
};

class HttpResponse {

private:
    int httpCode;

    void readHeader(char* httpDateBuffer);
    void readHttpCode();
    int readDate(char* httpDateBuffer);
    void chooseMonth(char c, char* httpDate);
    void skipInnerJsonObject();
    bool findJsonElementContaining(const char* substring, char* buffer, int bufferLength);
    bool readToElementStart(char &c, unsigned long &prevTime);
    void readJSONValue(char* buffer, const int bufferLength);
    /**
      * if JSON object, the function writes '{' char to first element of the buffer.
      * Returns true in such case.
      */
    bool isJSONObject(char* buffer);
    bool locateJsonValue(const char* jsonPath);
    void waitForEndOfResponse();
    void finish(GSMModule* mod);
public:
    /**
      * Creates HttpResponse. Remember to use #cleanMemory() method
      * when you are done with value of requested JSON element (see #getRequestedValue() ).
      * Parameters:
      *     jsonPath - path to the JSON element which value needs to be saved
      *     buffer - buffer to save JSON value to
      *     bufferLength - length of the buffer
      *     [httpDateBuffer] - buffer for date (optional)
      * Parameter httpDateBuffer must be at least 23 long! (no check performed)
      */
    HttpResponse(GSMModule* mod, const char* jsonPath, char* buffer, const int bufferLength, char* httpDateBuffer = NULL);
    HttpResponse(GSMModule* mod, char* jsonElement, int elementBufferLength, char* jsonValue, int valueBufferLength);

    /**
      * Returns the HTTP code of the response.
      */
    int getHttpCode();
};

class HttpRequest {

private:
    GSMModule* mod;

public:
    /**
      * Creates HttpRequest with basic headers.
      * Parameters:
      *     mod - pointer to GSMModule. Remember to attach GPRS before creating HttpRequest (see GSMModule#attachGPRS(char*,char*,char*) )
      *     host - hostname to connect to (without path, only [http://]domain.com )
      *     path - path for GET/POST/etc headers
      *     port - port to connect to
      *     type - type of the request (GET/POST/PUT/etc)
      */
    HttpRequest(GSMModule* mod, const char* host, const char* path, const int port, const __FlashStringHelper* type);
    HttpRequest(GSMModule* mod, const char* host, const __FlashStringHelper* path, const int port, const __FlashStringHelper* type);
    HttpRequest(GSMModule* mod, const char* host, const int port);

    /**
      * Adds Authorization header
      */
    void authorization(const char* authorization);
    /**
      * Adds X-Cumulocity-Application-Key header
      */
    void applicationKey(const char* applicationKey);

    void writeMeasurementContentType();

    void writeManagedObjectContentType();

    void writeManagedObjectReferenceContentType();

    void writeOperationContentType();

    void writeAlarmContentType();
	
	void writeEventContentType();

    void writeManagedObjectAccept();

    void writeOperationCollectionAccept();

    void startContent(int contentLength);

    void write(const char* content);

    void write(char*& mUnit);

    void write(const __FlashStringHelper* content);

    void write(const int content);

    /**
      * Executes request.
      * Parameters:
      *     jsonPath - path to the JSON element which value needs to be saved
      *     buffer - buffer to copy the value of JSON element to
      *     bufferLength - length of the buffer
      * Returns HttpResponse object with saved JSON element requested in second parameter.
      */
    HttpResponse execute(const char* jsonPath, char* buffer, int bufferLength, char* timeBuffer = NULL);

    HttpResponse execute(char* jsonElement, int elementBufferLength, char* jsonValue, int valueBufferLength);

    HttpResponse execute(char* timeBuffer = NULL);
};

class HttpClient {

protected:
    GSMModule* mod;
    HttpRequest* createRequest(const char* host, const __FlashStringHelper* type);
    HttpRequest* createRequest(const char* host, const char* path, const __FlashStringHelper* type);
    HttpRequest* createRequest(const char* host, const __FlashStringHelper* path, const __FlashStringHelper* type);


public:
    /**
      * Creates HttpClient.
      * Parameters:
      *     mod - pointer to GSMModule. Remember to attach GPRS before creating HttpClient (see GSMModule#attachGPRS(char*,char*,char*) )
      */
    HttpClient(GSMModule* mod);

    /**
      * Creates HTTP GET request.
      * Parameters:
      *     url - URL to send request to, in format: [http://]domain.com[:port]/[path]
      */
    HttpRequest* get(const char* url);
    HttpRequest* get(const char* host, char* path);
    HttpRequest* get(const char* host, const __FlashStringHelper* path);

    /**
      * Creates HTTP POST request.
      * Parameters:
      *     url - URL to send request to, in format: [http://]domain.com[:port]/[path]
      */
    HttpRequest* post(const char* url);
    HttpRequest* post(const char* host, char* path);
    HttpRequest* post(const char* host, const __FlashStringHelper* path);
};

class OperationRequester {
    friend class CumulocityPlatform;
private:
    unsigned long prevTime;
    void (*handler)();
    const unsigned long interval;
    char* opBuffer;
    int opBufferLength;
    char* nameBuffer;
    int nameBufferLength;
public:
    OperationRequester(void (*functionPtr)(), char* operationNameBuffer, int nameBufferLength, char* operationBuffer, int operationBufferLength, const unsigned long interval);
};

/** CumulocityPlatform class is the main access point to Cumulocity API.
  * All methods that return int have standardized exit codes:\n
  * Exit code | Description
  * -----------|------------
  * 1          | OK
  * 0	       | Object already exists (OK) / operation not found (OK)
  * -1	       | Not found
  * -2	       | Response not received
  * -3	       | Timeout reading response
  * < -200	   | Http error code multiplied by -1
  */
class CumulocityPlatform {

private:
    const char* host;
    const char* tenantId;
    const char* user;
    const char* password;
    const char* applicationKey;
    int port;

    unsigned long lastTimeUpdate;
    char time[25];

    OperationRequester* requester;

    GSMModule* mod;

    void writeAuth(HttpRequest* request);

    char* readIdFromEEPROM();
    void saveIdInEEPROM(char* id);
    int isDeviceRegistered(const char* ,const char*,const char*);
    int registerInCumulocity(const char*,const char*, char* id, const int, const char**, const int, const char**, const int);
    HttpRequest* prepareRegistrationRequest();
    char* prepareFragment(const char** fragmentValues, const int noValues);
    void Construct(const char* _host, const int _port, const char* _tenantId, const char* _user, const char* _password, const char* _applicationKey);

    HttpRequest* startAlarmRequest();
	HttpRequest* startEventRequest();
    HttpRequest* startMeasurementRequest(const char* type, const char* time, const char* measurementName, const long &mValue, const char* mUnit);
    HttpRequest* startOperationUpdateRequest(const char* id);
    int getOperationId(char* buffer, int bufferLength);
    int updateOperation(const char* id, const __FlashStringHelper* newStatus);
    void addTime(char* str, unsigned long diff);
    unsigned int daysInMonth(int month, int year);
    bool isLeapYear(int year);

public:

    /**
      * Creates CumulocityPlatform.
      *     @param host - host of the cumulocity platform in format: [http://]domain.com (without /platform !)
      *     @param port - port of the cumulocity platform API
      *     @param tenantId - for authorization
      *     @param user - for authorization
      *     @param password - for authorization
      *     @param applicationKey - for authorization
      */
    CumulocityPlatform(const char* host, const int port, const char* tenantId, const char* user, const char* password, const char* applicationKey);

    /**
      * Creates CumulocityPlatform using default port (80).
      *     @param host - host of the cumulocity platform in format: [http://]domain.com (without /platform !)
      *     @param tenantId - for authorization
      *     @param user - for authorization
      *     @param password - for authorization
      *     @param applicationKey - for authorization
      */
    CumulocityPlatform(const char* host, const char* tenantId, const char* user, const char* password, const char* applicationKey);
    virtual ~CumulocityPlatform();

    /**
      * Sends alarm information to the server.
      *     @param type - type of the alarm
      *     @param status - status of the alarm (active, acknowledged, cleared)
      *     @param severity - severity of the alarm (critical, major, minor, warning)
      *     @param time - time of alarm
      *     @param text - information about the alarm
      *     @return 1 in case of success
      *     @return -2 error reading response (impossible to determine success)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int raiseAlarm(const char* type, const char* status, const char* severity, const char* time, const char* text);

    /**
      * Sends alarm information to the server. Makes additional request to obtain current time.
      *     @param type - type of the alarm
      *     @param status - status of the alarm (active, acknowledged, cleared)
      *     @param severity - severity of the alarm (critical, major, minor, warning)
      *     @param text - information about the alarm
      *     @return 1 in case of success
      *     @return -2 error reading response (impossible to determine success)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int raiseAlarm(const char* type, const char* status, const char* severity, const char* text);

    /**
      * Sends measurement to the server.
      *     @param type - type of measurement
      *     @param time - time of measurement
      *     @param measurementName - name of measurment
      *     @param mValue - value
      *     @param mUnit - unit (V, mA, etc.)
      *     @return 1 in case of success
      *     @return -2 error reading response (impossible to determine success)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int sendMeasurement(const char* type, const char* time, const char* fragmentName, const char* measurementName, const long &mValue, const char* mUnit);

    /**
      * Sends measurement to the server. Makes additional request to obtain current time.
      *     @param type - type of measurement
      *     @param measurementName - name of measurment
      *     @param mValue - value
      *     @param mUnit - unit (V, mA, etc.)
      *     @return 1 in case of success
      *     @return -2 error reading response (impossible to determine success)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int sendMeasurement(const char* type, const char* fragmentName, const char* measurementName, const long &mValue, const char* mUnit);
	
	
	/**
	
	**/
	int sendEvent(const char* type, const char* fragmentName, const char* altValue,const char* lngValue,const char* latValue, const char* text,const char* time);
	
	
	
    /**
      * Registers the arduino device in Cumulocity (using "Arduino" as type).
      * If already registered, skips registration and only copies id.
      *     @param name - name of the device
      *     @param[out] buffer - buffer for ID of the device
      *     @param bufferSize - size of the buffer
      *     @param supportedOperations - supported operations
      *     @param nSupportedOperations - size of supported operations
      *     @param supportedMeasurements - supported measurements
      *     @param nSupportedMeasurements - size of supported measurements
      *     @return 1 in case of success
      *     @return 0 in case device already existed on server
      *     @return -2 error reading response (impossible to determine success)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int registerDevice(const char* name, char* buffer, const int bufferSize, 
        const char** supportedOperations, const int nSupportedOperations, 
        const char** supportedMeasurements, const int nSupportedMeasurements);

    /**
      * Registers the arduino device in Cumulocity (using "Arduino" as type).
      * If already registered, skips registration and only copies id.
      *     @param name - name of the device
      *     @param[out] buffer - buffer for ID of the device
      *     @param bufferSize - size of the buffer
      *     @return 1 in case of success
      *     @return 0 in case device already existed on server
      *     @return -2 error reading response (impossible to determine success)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int registerDevice(const char* name, char* buffer, const int bufferSize);

    /**
      * Obtains oldest pending operation from server (if any). If the operation fragment is a string,
      * it will be copied copied to buffer, excluding quotes. If the operation fragment is an object
      * it will be copied to buffer including brackets.
      *     @param[out] operationName - buffer for name of the operation
      *     @param nameBuffLength - length of the operationName buffer
      *     @param[out] operationBuffer - buffer for value of the operation
      *     @param opBuffLength - length of the operationBuffer buffer
      *     @return 1 in case of success
      *     @return 0 in case of no pending operations
      *     @return -2 error reading response (could not read operation)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int getPendingOperationFromServer(char* operationName, int nameBuffLength, char* operationBuffer, int opBuffLength);

    /**
      * Marks the oldest available pending operation as completed.
      *     @return 1 in case of success
      *     @return 0 in case of no pending operations to update
      *     @return -2 error reading response (could not read operation)
      *     @return -3 in case of timeout reading response
      *     @return other negative value as http error multiplied by -1
      */
    int markOperationCompleted();

    /**
      * If required interval passed and there is a pending operation on server,
      * handler for this command will be invoked.
      * @see registerForServerOperation
      */
    void triggerHandlersIfCommandsAwaiting();

    /**
      * Register a handler for server operation. Requires triggerHandlersIfCommandsAwaiting method to be called
      * in the loop method
      *     @param handler - handler for operation
      *     @param operationFragmentName - name of the fragment containing operation
      *     @param operationBuffer - buffer for operation
      *     @param bufferLength - length of the buffer
      *     @param interval - how often should server be checked for new commands
      * @see triggerHandlersIfCommandsAwaiting
      * @see getPendingOperationFromServer
      */
    void registerForServerOperation(void (*functionPtr)(), char* operationNameBuffer, int nameBufferLength, char* operationBuffer, int operationBufferLength, const unsigned long interval);

    /**
      * GSMModule is required for HTTP connections and for time support.
      *     @param mod - pointer to GSMModule object. It should have GPRS attached before any usage.
      */
    void setGSM(GSMModule* mod);

    /**
      * Copies pointer to time string in format yy-MM-ddThh:mm:ss.SSSZ. The pointer will always point to the same
      * char array, stored in this object. Method getTime will update this char array, returning it in the end.
      *     @return pointer to char array containing time
      */
    const char* getTime();
};

/* -------------------- BASE64 -------------------- */

static const char* base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
void base64_encode(char* output, char const* input, unsigned int in_len);

#endif /* CUMULOCITYPLATFORM_H_ */
