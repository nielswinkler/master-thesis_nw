/*
 * CumulocityPlatform.cpp
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

#include "CumulocityPlatform.h"

static const char* CUMULOCITY_ARDUINO_TYPE = "Arduino";

#define RX_FINISHED_STR F("RX FINISHED STR")
#define HTTP_HEADER_FINISH F(" HTTP/1.1\nHost: ")

// content types
#define CUMULOCITY_CT_PREFIX F("application/vnd.com.nsn.cumulocity.")
#define CUMULOCITY_CT_SUFFIX F("+json;ver=0.9;charset=UTF-8")
#define CUMULOCITY_CT_MANAGED_OBJECT_09 F("managedObject")
#define CUMULOCITY_CT_MANAGED_OBJECT_REF_09 F("managedObjectReference")
#define CUMULOCITY_CT_MEASUREMENT_09 F("measurement")
#define CUMULOCITY_CT_OPERATION_COLLECTION_09 F("operationCollection")
#define CUMULOCITY_CT_OPERATION_09 F("operation")
#define CUMULOCITY_CT_ALARM_09 F("alarm")
#define CUMULOCITY_CT_EVENT_09 F("event")

// http paths
#define MANAGED_OBJECT_PATH F("/inventory/managedObjects")
#define CHILD_DEVICES_PATH_SUFFIX F("/childDevices")
#define IDENTITY_PATH F("/identity")
#define OPERATION_PATH_PREFIX F("/devicecontrol/operations")
#define GET_OPERATION_PATH_CRITERIA F("?pageSize=1&deviceId=")
#define GET_OPERATION_PATH_SUFFIX F("&status=PENDING")

#define DEBUG_ENABLED

/* ############################## GSMModule ############################## */

GSMModule::GSMModule() {
    pinMode(7, OUTPUT);
    pinMode(6, OUTPUT);
    gsm.begin(4800);
    gsm.forceON();
}

GSMModule::~GSMModule() {}

int GSMModule::attachGPRS(char* domain, char* dom1, char* dom2) {
    return inet.attachGPRS(domain, dom1, dom2);
}
int GSMModule::atWrite(char* atcommand) {
    return inet.atWrite(atcommand);
}
	
int GSMModule::dettachGPRS() {
    return inet.dettachGPRS();
}

int GSMModule::connectTCP(const char* host, int port) {
    return inet.connectTCP(host, port);
}

inline void GSMModule::disconnectTCP() {
    gsm.SimpleWriteln(F("AT+CIPCLOSE"));
    gsm.WaitResp(5000, 400);
}

/* ############################## HttpResponse ############################## */

HttpResponse::HttpResponse(GSMModule* mod, char* jsonElement, int elementBufferLength, char* jsonValue, int valueBufferLength) {
    readHeader(NULL);

    if(findJsonElementContaining("control", jsonElement, elementBufferLength)) {
#ifdef DEBUG_ENABLED
        Serial.print(".");
#endif
        readJSONValue(jsonValue, valueBufferLength);
    } else {
        jsonValue[0] = '\0';
        jsonElement[0] = '\0';
    }
#ifdef DEBUG_ENABLED
    Serial.print(".");
#endif

    finish(mod);
}

HttpResponse::HttpResponse(GSMModule* mod, const char* jsonPath, char* buffer, const int bufferLength, char* httpDateBuffer) {
    readHeader(httpDateBuffer);

    if(jsonPath != NULL && buffer != NULL && bufferLength > 0) {
        if(locateJsonValue(jsonPath)) {
            readJSONValue(buffer, bufferLength);
        } else {
            buffer[0] = '\0';
        }
    }
    finish(mod);
}

bool HttpResponse::findJsonElementContaining(const char* substring, char* buffer, int bufferLength) {
    int matchIndex = 0;
    int elementIndex = 0;
    char c;
    unsigned long prevTime = millis();
    if (!readToElementStart(c, prevTime)) {
        return false;
    }

    while (true) {
        c = gsm.read();
        if (c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            prevTime = millis();

            if (strlen(substring) > matchIndex) {
                if (substring[matchIndex] == c) {
                    matchIndex++;
                }
            }
            if(c == '"') {
                // end of element
                if (strlen(substring) == matchIndex) {
                    // found it!
                    if(elementIndex < bufferLength) {
                        buffer[elementIndex++] = '\0';
                    } else {
                        buffer[bufferLength - 1] = '\0';
                    }
                    return true;
                } else if(readToElementStart(c, prevTime)) {
                    // start next element
                    matchIndex = 0;
                    elementIndex = 0;
                    continue;
                } else {
                    // end of response, or response not received
                    buffer[0] = '\0';
                    return false;
                }

            }
            if(elementIndex < bufferLength) {
                buffer[elementIndex++] = c;
            }
        } else {
            if ((millis() - prevTime) >= 2000) {
                return false;
            }
        }
    }
}

bool HttpResponse::readToElementStart(char &c, unsigned long &prevTime) {
    do {
        tryagain:
        c = gsm.read();
        if (c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            prevTime = millis();
        } else {
            if((millis() - prevTime) >= 2000) {
                return false;
            }
        }
    } while(c != '{' && c != ',');

    // one more char (must be quote - ")
    while(true) {
        c = gsm.read();
        if (c > 0) {
            prevTime = millis();
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            if(c != '"') {
                goto tryagain;
            }
            return true;
        } else {
            if((millis() - prevTime) >= 2000) {
                return false;
            }
        }
    }
}

void HttpResponse::finish(GSMModule* mod) {
#ifdef DEBUG_ENABLED
    Serial.print(".");
#endif
    waitForEndOfResponse();
#ifdef DEBUG_ENABLED
    Serial.println(".");
#endif
    delay(1000);
    mod->disconnectTCP();
}

void HttpResponse::readHeader(char* httpDateBuffer) {
#ifdef DEBUG_ENABLED
    Serial.println(F("Response (some dots are additional for debug):")); // debug dots
#endif
    readHttpCode();
#ifdef DEBUG_ENABLED
    Serial.print(".");
#endif
    if(httpDateBuffer != NULL) {
        readDate(httpDateBuffer);
    }
#ifdef DEBUG_ENABLED
    Serial.print(".");
#endif
}

void HttpResponse::readHttpCode() {
    char buff[20];
    buff[19] = '\0';
    // read HTTP code
    char c;
    int i=0;
    do {
        c = gsm.read();
        if(c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            buff[i++] = c;
        }
    } while(i < 19 && buff[i-1] != '\n');

    // rfc2616:
    // Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    char* code = strstr(buff, " ");
    code++;
    httpCode = atoi(code);
}

int HttpResponse::readDate(char* httpDate) {
    char* dateHeader = "Date:";
    int i = 0; // character index that matched
    char c;
    char prev;
    unsigned long prevTime = millis();
    int exitcode = 0;
    while(i < 5) {
        c = gsm.read();
        if(prev == '\n' && c == prev) {
            exitcode = -1; // not found
            break; // body started
        }
        if (c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            prevTime = millis();
            if (c != dateHeader[i++]) {
                // not matching any letter
                i=0;
            }
        } else {
            if((millis() - prevTime) >= 2000) {
                exitcode = -3; // timeout
            }
        }
    }
    if(i == strlen(dateHeader)) { // found date header
        // skip day name, e.g. " Tue, " - precisely we wait for second space
        for(int j = 0; j < 2;) {
            c = gsm.read();
#ifdef DEBUG_ENABLED
            if(c > 0) {
                Serial.print(c);
            }
#endif
            if(c == ' ') {
                j++;
            }
        }

        // read the date
        // header: 12 Feb 2013 08:01:26 GMT
        // target: 2013-02-12T08:01:26.000Z
        i = 0; // reusing
        while(c != '\n') {
            c = gsm.read();
            if(c > 0) {
#ifdef DEBUG_ENABLED
                Serial.print(c);
#endif
                prevTime = millis();
                switch(i) {
                    // day1
                case 0:
                    httpDate[8] = c;
                    break;
                    // day2
                case 1:
                    httpDate[9] = c;
                    break;
                    // month starts
                case 3:
                    // set helper (temp) chars
                    if(c == 'J')
                        httpDate[5] = httpDate[6] = '1'; // Jan or Jun
                    else if(c == 'M')
                        httpDate[5] = httpDate[6] = '2'; // Mar
                    else
                        httpDate[5] = httpDate[6] = '0'; // other
                    break;
                case 4:
                    if(httpDate[5] == '1') { // Jan or Jun
                        if(c == 'a') { // Jan
                            httpDate[5] = '0';
                            httpDate[6] = '1';
                        } else { // Jun
                            httpDate[5] = '0';
                            httpDate[6] = '6';
                        }
                    }
                    break;
                    // month ends
                case 5:
                    chooseMonth(c, httpDate);
                    break;
                    // year1
                case 7:
                    httpDate[0] = c;
                    break;
                    // year2
                case 8:
                    httpDate[1] = c;
                    break;
                    // year3
                case 9:
                    httpDate[2] = c;
                    break;
                    // year4
                case 10:
                    httpDate[3] = c;
                    break;
                    // hour1
                case 12:
                    httpDate[11] = c;
                    break;
                    // hour2
                case 13:
                    httpDate[12] = c;
                    break;
                    // min1
                case 15:
                    httpDate[14] = c;
                    break;
                    // min2
                case 16:
                    httpDate[15] = c;
                    break;
                    // sec1
                case 18:
                    httpDate[17] = c;
                    break;
                    // sec2
                case 19:
                    httpDate[18] = c;
                    break;
                }
                i++;
            } else {
                if((millis() - prevTime) >= 2000) {
                    return -3; // timeout
                }
            }
        }
        httpDate[4] = httpDate[7] = '-';
        httpDate[13] = httpDate[16] = ':';
        httpDate[10] = 'T';
        httpDate[19] = '.';
        httpDate[20] = httpDate[21] = httpDate[22] = '0';
        httpDate[23] = 'Z';
        httpDate[24] = '\0';

        return 1;
    } else {
        return exitcode;
    }
}

void HttpResponse::chooseMonth(char c, char* httpDate) {
    /* 3rd letter (month):
        n - Jan, Jun (done)
        b - Feb
        r - Mar (2), Apr (0)
        y - May
        l - Jul
        g - Aug
        p - Sep
        t - Oct
        v - Nov
        c - Dec
    */
    if(httpDate[5] != httpDate[6])
        return; // already done (Jan or Jun)

    switch(c) {
    case 'b' :
        httpDate[5] = '0';
        httpDate[6] = '2';
        break;
    case 'r' :
        if(httpDate[5] == '2')  { // Mar
            httpDate[5] = '0';
            httpDate[6] = '3';
        } else {
            httpDate[5] = '0';
            httpDate[6] = '4';
        }
        break;
    case 'y' :
        httpDate[5] = '0';
        httpDate[6] = '5';
        break;
    case 'l' :
        httpDate[5] = '0';
        httpDate[6] = '7';
        break;
    case 'g' :
        httpDate[5] = '0';
        httpDate[6] = '8';
        break;
    case 'p' :
        httpDate[5] = '0';
        httpDate[6] = '9';
        break;
    case 't' :
        httpDate[5] = '1';
        httpDate[6] = '0';
        break;
    case 'v' :
        httpDate[5] = '1';
        httpDate[6] = '1';
        break;
    case 'c' :
        httpDate[5] = '1';
        httpDate[6] = '2';
        break;
    }
}

void HttpResponse::skipInnerJsonObject() {
    int depth = 1;
    char c;
    while (depth > 0) {
        c = gsm.read();
#ifdef DEBUG_ENABLED
        if (c > 0) {
            Serial.print(c);
        }
#endif
        if (c == '{')
            depth++;
        else if (c == '}')
            depth--;
    }
}

void HttpResponse::readJSONValue(char* buffer, const int bufferLength) {
    bool json = isJSONObject(buffer);
    // read to buffer until " char
    int i = json ? 1 : 0;
    int jsonDepth = 1;
    char c;
    while (i < bufferLength) { // max i on exit = bufferLength
        c = gsm.read();
        if (c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            if(c != '\\') { // TODO handle double backlash also (ignore once)
                if (!json && c == '"') {
                    break;
                }
                buffer[i++] = c;
                if (json) {
                    if(c == '{')
                        jsonDepth++;
                    else if (c == '}' && --jsonDepth == 0) {
                        break;
                    }
                }
            }
        }
    }
    if ( i == bufferLength) {
        Serial.println(F("OVERFLOW"));
        buffer[0] = '\0';
    } else {
        buffer[i++] = '\0'; // max i after instruction = bufferLength
    }
}

bool HttpResponse::isJSONObject(char* buffer) {
    char c;
    while(true) {
        c = gsm.read();
#ifdef DEBUG_ENABLED
        if (c > 0) {
            Serial.print(c);
        }
#endif
        if (c == ':') {
            break;
        }
    }
    // next char is { or "
    while (true) {
        c = gsm.read();
#ifdef DEBUG_ENABLED
        if (c > 0) {
            Serial.print(c);
        }
#endif
        if (c == '{') {
            buffer[0] = c;
            return true; // JSON object
        } else if (c == '"') {
            return false; // string object
        }
    }
}

bool HttpResponse::locateJsonValue(const char* jsonPath) {
    int i = 0; // character index that matched
    int pieceBeginning=0;
    char c;
    unsigned long prevTime = millis();
    while(i < strlen(jsonPath)) {
        c = gsm.read();
        if (c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            prevTime = millis();
            if (c == jsonPath[i]) {
                if (jsonPath[++i] == '/') {
                    i++;
                    pieceBeginning = i;

                    // read to open inner object
                    while (c != '{') {
                        c = gsm.read();
                        if (c > 0) {
#ifdef DEBUG_ENABLED
                            Serial.print(c);
#endif
                            prevTime = millis();
                        } else {
                            if((millis() - prevTime) >= 2000)
                                goto timeout;
                        }
                    }
                }
            } else {
                // not matching any letter
                i = pieceBeginning;
                if (i != 0) {
                    // at least one outer object already found
                    // but inner object char not yet matching
                    if (c == '{') { // entering inner object, we want to skip it entirely...
                        skipInnerJsonObject();
                    }
                }
            }
        } else {
            if((millis() - prevTime) >= 2000)
                goto timeout;
        }
    }
    return i == strlen(jsonPath);
timeout:
    return false;
}

void HttpResponse::waitForEndOfResponse() {
    char c;

    unsigned long prevTime = millis();
    while (true) {
        c = gsm.read();
        if (c > 0) {
#ifdef DEBUG_ENABLED
            Serial.print(c);
#endif
            prevTime = millis();
        } else {
            if((millis() - prevTime) >= 2000)
                break;
        }
    }
}

int HttpResponse::getHttpCode() {
    return httpCode;
}

/* ############################## HttpRequest ############################## */

HttpRequest::HttpRequest(GSMModule* mod, const char* host, const int port) {
    bool connected = false;
    int attemptsCounter = 0;
    while(attemptsCounter < 3) {
        if(!mod->connectTCP(host, port)) {
            attemptsCounter++;
        } else {
            connected = true;
            attemptsCounter = 3;
        }
    }
    if(!connected) {
        Serial.println(F("TCP Failed"));
    }
}

HttpRequest::HttpRequest(GSMModule* mod, const char* host, const char* path, const int port, const __FlashStringHelper* type) : mod(mod) {
    bool connected = false;
    int attemptsCounter = 0;
    while(attemptsCounter < 3) {
        if(!mod->connectTCP(host, port)) {
            attemptsCounter++;
        } else {
            connected = true;
            attemptsCounter = 3;
        }
    }
    if(!connected) {
        Serial.println(F("TCP Failed"));
    }

    write(type);
    write(F(" "));
    write(path);
    write(HTTP_HEADER_FINISH);
    write(host);
}

HttpRequest::HttpRequest(GSMModule* mod, const char* host, const __FlashStringHelper* path, const int port, const __FlashStringHelper* type) : mod(mod) {
    bool connected = false;
    int attemptsCounter = 0;
    while(attemptsCounter < 3) {
        if(!mod->connectTCP(host, port)) {
            attemptsCounter++;
        } else {
            connected = true;
            attemptsCounter = 3;
        }
    }
    if(!connected) {
        Serial.println(F("TCP Failed"));
    }

    write(type);
    write(F(" "));
    write(path);
    write(HTTP_HEADER_FINISH);
    write(host);
}

void HttpRequest::authorization(const char* authorization) {
    write(F("\nAuthorization: Basic "));
    write(authorization);
}

void HttpRequest::applicationKey(const char* applicationKey) {
    write(F("\nX-Cumulocity-Application-Key: "));
    write(applicationKey);
}

void HttpRequest::writeMeasurementContentType() {
    write(F("\nContent-Type: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_MEASUREMENT_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeManagedObjectContentType() {
    write(F("\nContent-Type: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_MANAGED_OBJECT_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeManagedObjectReferenceContentType() {
    write(F("\nContent-Type: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_MANAGED_OBJECT_REF_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeAlarmContentType() {
    write(F("\nContent-Type: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_ALARM_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeEventContentType() {
    write(F("\nContent-Type: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_EVENT_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeOperationContentType() {
    write(F("\nContent-Type: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_OPERATION_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeManagedObjectAccept() {
    write(F("\nAccept: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_MANAGED_OBJECT_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::writeOperationCollectionAccept() {
    write(F("\nAccept: "));
    write(CUMULOCITY_CT_PREFIX);
    write(CUMULOCITY_CT_OPERATION_COLLECTION_09);
    write(CUMULOCITY_CT_SUFFIX);
}

void HttpRequest::startContent(int contentLength) {
    write(F("\nContent-Length: "));
    write(contentLength);
    write(F("\n\n"));
}

void HttpRequest::write(const char* content) {
#ifdef DEBUG_ENABLED
    Serial.print(content);
#endif
    gsm.SimpleWrite(content);
}
void HttpRequest::write(char*& content) {
#ifdef DEBUG_ENABLED
    Serial.print(content);
#endif
    gsm.SimpleWrite(content);
}

void HttpRequest::write(const __FlashStringHelper* content) {
#ifdef DEBUG_ENABLED
    Serial.print(content);
#endif
    gsm.SimpleWrite(content);
}

void HttpRequest::write(const int content) {
#ifdef DEBUG_ENABLED
    Serial.print(content);
#endif
    gsm.SimpleWrite(content);
}

HttpResponse HttpRequest::execute(char* jsonElement, int elementBufferLength, char* jsonValue, int valueBufferLength) {
    write(F("\n\n"));
    char endChar[2];
    endChar[0] = 0x1a;
    endChar[1] = '\0';
    write(endChar);

    switch(gsm.WaitResp(2000, 50, "SEND OK")) {
    case RX_TMOUT_ERR:
        Serial.println(F("RX TMOUT ERR"));
        break;
    case RX_FINISHED_STR_NOT_RECV:
        Serial.println(RX_FINISHED_STR);
        break;
    }
#ifdef DEBUG_ENABLED
    Serial.println(F("REQ SENT"));
#endif

    return HttpResponse(mod, jsonElement, elementBufferLength, jsonValue, valueBufferLength);
}

HttpResponse HttpRequest::execute(const char* jsonPath, char* buffer, int bufferLength, char* timeBuffer) {
    write(F("\n\n"));
    char endChar[2];
    endChar[0] = 0x1a;
    endChar[1] = '\0';
    write(endChar);

    switch(gsm.WaitResp(2000, 50, "SEND OK")) {
    case RX_TMOUT_ERR:
        Serial.println(F("RX TMOUT ERR"));
        break;
    case RX_FINISHED_STR_NOT_RECV:
        Serial.println(RX_FINISHED_STR);
        break;
    }
#ifdef DEBUG_ENABLED
    Serial.println(F("REQ SENT"));
#endif

    return HttpResponse(mod, jsonPath, buffer, bufferLength, timeBuffer);
}

HttpResponse HttpRequest::execute(char* timeBuffer) {
    return execute(NULL, NULL, 0, timeBuffer);
}

/* ############################## HttpClient ############################## */

HttpClient::HttpClient(GSMModule* mod) : mod(mod)
{}

HttpRequest* HttpClient::createRequest(const char* url, const __FlashStringHelper* type) {

    // skip "http://" if exists
    if(strstr(url, "http://") == url) {
        url += 7;
    }

    // find PATH after domain
    char* path = strstr(url, "/");

    // copy DOMAIN to new string array
    char host[path-url+1];
    strncpy(host, url, path-url);
    host[path-url] = '\0';

    // get port if exists
    char* portStr = strstr(url, ":");
    int port = 80;
    if(portStr) {
        port = atoi(++portStr);
    }

    return new HttpRequest(mod, host, path, port, type);
}

HttpRequest* HttpClient::createRequest(const char* host, const char* path, const __FlashStringHelper* type) {
    // skip "http://" if exists
    if(strstr(host, "http://") == host) {
        host += 7;
    }

    // get port if exists
    char* portStr = strstr(host, ":");
    int port = 80;
    if(portStr) {
        port = atoi(++portStr);
    }

    return new HttpRequest(mod, host, path, port, type);
}

HttpRequest* HttpClient::createRequest(const char* host, const __FlashStringHelper* path, const __FlashStringHelper* type) {
    // skip "http://" if exists
    if(strstr(host, "http://") == host) {
        host += 7;
    }

    // get port if exists
    char* portStr = strstr(host, ":");
    int port = 80;
    if(portStr) {
        port = atoi(++portStr);
    }

    return new HttpRequest(mod, host, path, port, type);
}

HttpRequest* HttpClient::get(const char* host, char* path) {
    return createRequest(host, path, F("GET"));
}

HttpRequest* HttpClient::get(const char* host, const __FlashStringHelper* path) {
    return createRequest(host, path, F("GET"));
}

HttpRequest* HttpClient::get(const char* url) {
    return createRequest(url, F("GET"));
}

HttpRequest* HttpClient::post(const char* host, char* path) {
    return createRequest(host, path, F("POST"));
}

HttpRequest* HttpClient::post(const char* host, const __FlashStringHelper* path) {
    return createRequest(host, path, F("POST"));
}

HttpRequest* HttpClient::post(const char* url) {
    return createRequest(url, F("POST"));
}

/* ############################## OperationRequester ############################## */

OperationRequester::OperationRequester(void (*functionPtr)(), char* operationNameBuffer, int nameBufferLength, char* operationBuffer, int operationBufferLength, const unsigned long interval) :
    handler(functionPtr),
    interval(interval),
    opBuffer(operationBuffer),
    opBufferLength(operationBufferLength),
    nameBuffer(operationNameBuffer),
    nameBufferLength(nameBufferLength),
    prevTime(0) {}

/* ############################## CumulocityPlatform ############################## */

void CumulocityPlatform::Construct(const char* _host, const int _port, const char* _tenantId, const char* _user, const char* _password, const char* _applicationKey) {
    host = _host;
    port = _port;
    tenantId = _tenantId;
    user = _user;
    password = _password;
    applicationKey = _applicationKey;
}

CumulocityPlatform::CumulocityPlatform(const char* host, const int port, const char* tenantId, const char* user, const char* password, const char* applicationKey) {
    Construct(host, port, tenantId, user, password, applicationKey);
}

CumulocityPlatform::CumulocityPlatform(const char* host, const char* tenantId, const char* user, const char* password, const char* applicationKey) {
    Construct(host, 80, tenantId, user, password, applicationKey);
}

CumulocityPlatform::~CumulocityPlatform() {}

// ------------------ private

void CumulocityPlatform::writeAuth(HttpRequest* request) {
    //<realm>/<user name>:<password>
    int input_size = strlen(tenantId) + strlen(user) + strlen(password) + 2;
    int output_size = ((input_size * 4) / 3) + (input_size / 96) + 6;
    char plain_auth[input_size];
    char encoded_auth[output_size];

    strcpy(plain_auth, tenantId);
    strcat(plain_auth, "/");
    strcat(plain_auth, user);
    strcat(plain_auth, ":");
    strcat(plain_auth, password);

    base64_encode(encoded_auth, plain_auth, input_size);

    request->authorization(encoded_auth);
}

char* CumulocityPlatform::readIdFromEEPROM() {
    int i = 0;
    char c;
    do {
        c = (char) eeprom_read_byte((unsigned char*) i++);
        if(c == -1)
            return NULL;
    } while(c != -1 && c != 0);

    if(i == 0)
        return NULL;

    char* buff = (char*) malloc( (i + 1) * sizeof( char ) );
    for (int k = 0; k <= i; k++) {
        buff[k] = (char) eeprom_read_byte((unsigned char*) k);
    }

    return buff;
}

void CumulocityPlatform::saveIdInEEPROM(char* id) {
    char* _id = id;
    for (int i = 0; i <= strlen(id); i++) {
        eeprom_write_byte((unsigned char*)i, *(_id++));
    }
}

char* CumulocityPlatform::prepareFragment(const char** fragmentValues, const int noValues){
    //"<fragmentName>":["<fragmentValue1>","<fragmentValue2>, ..."]
    int size = 2;//[]
    for (int i = 0; i < noValues; i++){
        size += strlen(fragmentValues[i]) + 2;//size + quation marks
    }
    if(noValues > 0){
        size += (noValues - 1);//colons
    }   
    size++;//one extra char at the end
    char* result = (char*) malloc(size * sizeof(char));
      
    strcpy(result, "[");
    for (int i = 0; i < noValues; i++){
        strcat(result, "\"");
        strcat(result, fragmentValues[i]);  
        strcat(result, "\"");
        if(i < noValues - 1){
            strcat(result,  ",");   
        }
    }
    strcat(result, "]"); 
    
    return result; 
}

HttpRequest* CumulocityPlatform::prepareRegistrationRequest(){

    HttpClient client(mod);
    HttpRequest* request = client.post(host, MANAGED_OBJECT_PATH);
    writeAuth(request);
    request->writeManagedObjectContentType();
    request->writeManagedObjectAccept();
    return request;
}

int CumulocityPlatform::registerInCumulocity(const char* type, const char* name, char* id, const int idLength, 
        const char** supportedOperations, const int nSupportedOperations, 
        const char** supportedMeasurements, const int nSupportedMeasurements) {

    HttpRequest* request = prepareRegistrationRequest();

    char* operationsFragment = prepareFragment(supportedOperations, nSupportedOperations);
    int operationsFragmentLength = strlen(operationsFragment);

    char* measurementFragment = prepareFragment(supportedMeasurements, nSupportedMeasurements);
    int measurementFragmentLength = strlen(measurementFragment);

    request->startContent(128 + strlen(name) + strlen(type) + operationsFragmentLength + measurementFragmentLength);

    request->write(F("{\"name\":\""));
    request->write(name);
    request->write(F("\",\"type\":\""));
    request->write(type);
    request->write(F("\",\"com_cumulocity_model_Agent\":{}"));
    request->write(F(",\"c8y_IsDevice\":{}"));
    request->write(F(",\"c8y_SupportedOperations\":"));
    
    request->write(operationsFragment);
    free(operationsFragment);
    request->write(F(",\"c8y_SupportedMeasurements\":"));
    
    request->write(measurementFragment);
    free(measurementFragment);
    request->write(F("}"));

 
 //   request->write(F("\",\"c8y_SupportedOperations\":{\"" + supportedOperation + "\"}}"));
//  request->write(F("\",\"c8y_SupportedMeasurements\":{\"" + supportedMeasurement + "\"}}"));

    HttpResponse response = request->execute("id", id, idLength);
    delete request;
    // not required, but without this println, we loose new id in case of reregistration.
    // FIXME needs to be investigated
    Serial.println(id);

    int exitcode = response.getHttpCode();
    if(exitcode == 201) {
        exitcode = 1;
    } else if (exitcode == 0) {
        exitcode = -2;
    } else {
        exitcode *= -1;
    }
    return exitcode;
}

int CumulocityPlatform::isDeviceRegistered(const char* id, const char* type, const char* name) {
    HttpRequest* request = new HttpRequest(mod, host, port);

    // custom path writing
    request->write(F("GET "));
    request->write(MANAGED_OBJECT_PATH);
    request->write(F("/"));
    request->write(id);
    request->write(HTTP_HEADER_FINISH);
    request->write(host);

    writeAuth(request);
    request->writeManagedObjectAccept();

    char nameInCumulocity[strlen(name) + 1];
    HttpResponse response = request->execute("name", nameInCumulocity, strlen(name) + 1);
    delete request;

    int exitcode = response.getHttpCode();
    if (exitcode == 200) {
        if (strcmp(nameInCumulocity, name) == 0) {
            exitcode = 1;
        } else {
            exitcode = -1;
        }
    } else if (exitcode == 0) {
        exitcode = -2;
    } else if (exitcode == 404) {
        exitcode = -1;
    } else {
        exitcode *= -1;
    }
    return exitcode;
}

HttpRequest* CumulocityPlatform::startAlarmRequest() {
    HttpClient client(mod);
    HttpRequest* request = client.post(host, F("/alarm/alarms"));

    writeAuth(request);
    request->writeAlarmContentType();

    return request;
}

int CumulocityPlatform::raiseAlarm(const char* type, const char* status, const char* severity, const char* text) {
    const char* time = getTime();
    return raiseAlarm(type, status, severity, time, text);
}

int CumulocityPlatform::raiseAlarm(const char* type, const char* status, const char* severity, const char* time, const char* text) {
    HttpRequest* request = startAlarmRequest();

    char* sourceId = readIdFromEEPROM();
    int contentLength = 76 + strlen(sourceId) + strlen(type) + strlen(status) + strlen(severity) + strlen(time) + strlen(text);
    request->startContent(contentLength);

    request->write(F("{\"source\":{\"id\":\""));
    request->write(sourceId);
    free(sourceId);
    request->write(F("\"},\"type\":\""));
    request->write(type);
    request->write(F("\",\"status\":\""));
    request->write(status);
    request->write(F("\",\"severity\":\""));
    request->write(severity);
    request->write(F("\",\"time\":\""));
    request->write(time);
    request->write(F("\",\"text\":\""));
    request->write(text);
    request->write(F("\"}"));

    HttpResponse response =request->execute();
    delete request;
    int exitcode = response.getHttpCode();
    if (exitcode == 0) {
        exitcode = -2;
    } else if(exitcode == 201) {
        exitcode = 1;
    } else if (exitcode > 0) {
        exitcode *= -1;
    }
    return exitcode;
}

HttpRequest* CumulocityPlatform::startEventRequest() {
    HttpClient client(mod);
    HttpRequest* request = client.post(host, F("/event/events"));

    writeAuth(request);
    request->writeEventContentType();

    return request;
}

int CumulocityPlatform::sendEvent(const char* type, const char* fragmentName, const char* altValue,const char* lngValue,const char* latValue, const char* text,const char* time) {
    HttpRequest* request = startEventRequest();

		
	char* sourceId = readIdFromEEPROM();
    int contentLength = 76 + strlen(sourceId) + strlen(type) + strlen(fragmentName) + strlen(latValue)+ strlen(lngValue)+ strlen(altValue) + strlen(time) + strlen(text);
    request->startContent(contentLength);

	/*
	{
		"source": {
			"id":"11700" },
		"type": "c8y_LocationUpdate",
		"time":"2013-06-22T17:07:14.000+02:00",
		"text": "LocUpdate",
		"c8y_Position": {
			"alt": 67,
			"lng": 6.55173,
			"lat": 51.551977 }
	}
	*/
	
	
    request->write(F("{\"source\":{\"id\":\""));
    request->write(sourceId);
    free(sourceId);
    request->write(F("\"},\"type\":\""));
    request->write(type);
    request->write(F("\",\"time\":\""));
    request->write(time);
    request->write(F("\",\"text\":\""));
    request->write(text);
	request->write("\",\"");
    request->write(fragmentName);
    request->write("\":{");
    request->write(F("\"alt\":"));
    request->write(altValue);
	request->write(F(",\"lng\":"));
    request->write(lngValue);
	request->write(F(",\"lat\":"));
    request->write(latValue);
    request->write(F("}}"));

    HttpResponse response =request->execute();
    delete request;
    int exitcode = response.getHttpCode();
    if (exitcode == 0) {
        exitcode = -2;
    } else if(exitcode == 201) {
        exitcode = 1;
    } else if (exitcode > 0) {
        exitcode *= -1;
    }
    return exitcode;
}

HttpRequest* CumulocityPlatform::startMeasurementRequest(const char* type, const char* time, const char* measurementName, const long &mValue, const char* mUnit) {
    HttpClient client(mod);
    HttpRequest* request = client.post(host, F("/measurement/measurements"));

    writeAuth(request);
    request->writeMeasurementContentType();

    return request;
}

int CumulocityPlatform::sendMeasurement(const char* type, const char* time, const char* fragmentName, const char* measurementName, const long &mValue, const char* mUnit) {
    HttpRequest* request = startMeasurementRequest(type, time, measurementName, mValue, mUnit);

    char* mValueStr = (char*) malloc(12*sizeof(char));
    ltoa(mValue, mValueStr, 10);

    char* sourceId = readIdFromEEPROM();
    int contentLength = 69 + strlen(sourceId) + strlen(time) + strlen(type) + strlen(fragmentName) + strlen(measurementName) + strlen(mValueStr) + strlen(mUnit);

    /*
     * Write content:
     * {
     * "time" : "2012-12-31T23:59:59.999Z",
     * "type" : "ArduinoMeasurement",
     * "source" : { "id": "XXX" },
     * "measurementName": { "value": YYY, "unit": "ZZZ" }
     * }
     */
    request->startContent(contentLength);
    request->write(F("{\"source\":{\"id\":\""));
    request->write(sourceId);
    free(sourceId);
    request->write(F("\"},\"time\":\""));
    request->write(time);
    request->write(F("\",\"type\":\""));
    request->write(type);
    request->write("\",\"");
    request->write(fragmentName);
    request->write(F("\":{\""));
    request->write(measurementName);
    request->write(F("\":{\"value\":"));
    request->write(mValueStr);
    free(mValueStr);
    request->write(F(",\"unit\":\""));
    request->write(mUnit);
    request->write(F("\"}}}"));

    HttpResponse response = request->execute();
    delete request;
    int exitcode = response.getHttpCode();
    if (exitcode == 0) {
        exitcode = -2;
    } else if(exitcode == 201) {
        exitcode = 1;
    } else if (exitcode > 0) {
        exitcode *= -1;
    }
    return exitcode;
}

int CumulocityPlatform::registerDevice(const char* name, char* buffer, int bufferSize) {
    const char* supportedOperations[0];
    const char*  supportedMeasurements[0];
    return registerDevice(name, buffer, bufferSize, supportedOperations, 0, supportedMeasurements, 0);
}

int CumulocityPlatform::registerDevice(const char* name, char* buffer, int bufferSize,
        const char** supportedOperations, const int nSupportedOperations, 
        const char** supportedMeasurements, const int nSupportedMeasurements) {
    char* savedId = readIdFromEEPROM();
    Serial.println();
    int exitcode;

    if(savedId != NULL) {
#ifdef DEBUG_ENABLED
        Serial.print(F("Device id in EEPROM:"));
        Serial.println(savedId);
#endif

        if(bufferSize < (strlen(savedId)+1)) {
            free(savedId);
            return -4;
        }
        buffer = strcpy(buffer, savedId);
        free(savedId);

        exitcode = isDeviceRegistered(buffer, CUMULOCITY_ARDUINO_TYPE, name);
        if(exitcode > 0) {
            return 0;
        } else if (exitcode != -1) {
            return exitcode;
        }
    }

    // either not in EEPROM, or not in Cumulocity
    exitcode = registerInCumulocity(CUMULOCITY_ARDUINO_TYPE, name, buffer, bufferSize,
        supportedOperations, nSupportedOperations, supportedMeasurements, nSupportedMeasurements);
    if(exitcode > 0) {
        Serial.print(F("Saving in EEPROM:"));
        Serial.println(buffer);
        saveIdInEEPROM(buffer);
        exitcode = 1;
    }
    return exitcode;
}

int CumulocityPlatform::getPendingOperationFromServer(char* operationName, int nameBuffLength, char* operationBuffer, int opBuffLength) {
    HttpRequest* request = new HttpRequest(mod, host, port);

    // custom path writing
    request->write(F("GET "));
    request->write(OPERATION_PATH_PREFIX);
    request->write(GET_OPERATION_PATH_CRITERIA);
    char* id = readIdFromEEPROM();
    request->write(id);
    free(id);
    request->write(GET_OPERATION_PATH_SUFFIX);
    request->write(HTTP_HEADER_FINISH);
    request->write(host);

    writeAuth(request);
    request->writeOperationCollectionAccept();

    HttpResponse response = request->execute(operationName, nameBuffLength, operationBuffer, opBuffLength);
    delete request;

    int exitcode = response.getHttpCode();
    if (exitcode == 0) {
        exitcode = -2; // error reading response
    } else if(exitcode == 200) {
        if (operationBuffer == NULL || strlen(operationBuffer) == 0) {
            exitcode = 0; // no operation awaiting
        } else {
            exitcode = 1; // OK
        }
    } else if (exitcode > 0) {
        exitcode *= -1; // http error code
    }

    return exitcode;
}

int CumulocityPlatform::getOperationId(char* buffer, int bufferLength) {
    HttpRequest* request = new HttpRequest(mod, host, port);

    // custom path writing
    request->write(F("GET "));
    request->write(OPERATION_PATH_PREFIX);
    request->write(GET_OPERATION_PATH_CRITERIA);
    char* id = readIdFromEEPROM();
    request->write(id);
    free(id);
    request->write(GET_OPERATION_PATH_SUFFIX);
    request->write(HTTP_HEADER_FINISH);
    request->write(host);

    writeAuth(request);
    request->writeOperationCollectionAccept();

    HttpResponse response = request->execute("id", buffer, bufferLength);
    delete request;

    int exitcode = response.getHttpCode();
    if (exitcode == 0) {
        exitcode = -2; // error reading response
    } else if(exitcode == 200) {
        if (buffer == NULL || strlen(buffer) == 0) {
            exitcode = 0; // no operation awaiting
        } else {
            exitcode = 1; // OK
        }
    } else if (exitcode > 0) {
        exitcode *= -1; // http error code
    }

}

int CumulocityPlatform::markOperationCompleted() {

    char idBuffer[8];

    int exitcode = getOperationId(idBuffer, 8);

    if(exitcode > 0 && strlen(idBuffer) > 0) {
        exitcode = updateOperation(idBuffer, F("SUCCESSFUL"));
    }

    return exitcode;
}

int CumulocityPlatform::updateOperation(const char* id, const __FlashStringHelper* newStatus) {

    Serial.print(F("Updating operation "));
    Serial.println(id);

    HttpRequest* request = startOperationUpdateRequest(id);

    request->startContent(23);
    request->write(F("{\"status\":\""));
    request->write(newStatus);
    request->write(F("\"}"));

    HttpResponse response = request->execute();
    delete request;

    int exitcode = response.getHttpCode();
    if (exitcode == 0) {
        exitcode = -2;
    } else if(exitcode == 200) {
        exitcode = 1;
    } else if (exitcode > 0) {
        exitcode *= -1;
    }
    return exitcode;
}

HttpRequest* CumulocityPlatform::startOperationUpdateRequest(const char* id) {
    HttpRequest* request = new HttpRequest(mod, host, port);

    request->write(F("PUT "));
    request->write(OPERATION_PATH_PREFIX);
    request->write(F("/"));
    request->write(id);
    request->write(HTTP_HEADER_FINISH);
    request->write(host);

    writeAuth(request);
    request->writeOperationContentType();
    return request;
}

void CumulocityPlatform::triggerHandlersIfCommandsAwaiting() {
    if(millis() - requester->prevTime > (requester->interval * 1000)) {
        unsigned long time = millis();
        int result = getPendingOperationFromServer(requester->nameBuffer, requester->nameBufferLength, requester->opBuffer, requester->opBufferLength);

        if(result >= 0) {
            requester->prevTime = time;
            if(result > 0) {
                requester->handler();
            }
        } // if result<0 then try again next time (don't wait interval)
    }
}

void CumulocityPlatform::registerForServerOperation(void (*functionPtr)(), char* operationNameBuffer, int nameBufferLength, char* operationBuffer, int operationBufferLength, const unsigned long interval) {
    if(requester == NULL)
        free(requester);

    requester = new OperationRequester(functionPtr, operationNameBuffer, nameBufferLength, operationBuffer, operationBufferLength, interval);
}

const char* CumulocityPlatform::getTime() {
    if(lastTimeUpdate == 0) {
        HttpClient client(mod);
        HttpRequest* request = client.get(host, IDENTITY_PATH);

        writeAuth(request);

        lastTimeUpdate = millis();
        HttpResponse response = request->execute(time);
        delete request;
    } else {
        unsigned long diff = millis() - lastTimeUpdate;
        lastTimeUpdate = millis();
        addTime(time, diff);
    }

    return time;
}



void CumulocityPlatform::addTime(char* str, unsigned long diff) {
    // milliseconds
    if(diff > 0) {
        int num = atoi(str + 20);
        diff += num;
        sprintf(str + 20, "%03lu", diff % 1000);
        diff /= 1000;
        str[23] = 'Z';
    } else {
        return;
    }

    // seconds
    if(diff > 0) {
        int num = atoi(str + 17);
        diff += num;
        sprintf(str + 17, "%02lu", diff % 60);
        diff /= 60;
        str[19] = '.';
    } else {
        return;
    }

    // minutes
    if(diff > 0) {
        int num = atoi(str + 14);
        diff += num;
        sprintf(str + 14, "%02lu", diff % 60);
        diff /= 60;
        str[16] = ':';
    } else {
        return;
    }

    // hours
    if(diff > 0) {
        int num = atoi(str + 11);
        diff += num;
        sprintf(str + 11, "%02lu", diff % 24);
        diff /= 24;
        str[13] = ':';
    } else {
        return;
    }

    // date
    diff += atoi(str+8);
    str[8] = str[9] = '0';
    while(diff > 0) {
        int year = atoi(str);
        int month = atoi(str + 5);
        unsigned int monthdayslimit = daysInMonth(month, year);

        if(diff > monthdayslimit) {
            diff -= monthdayslimit;
            if(++month > 12) {
                sprintf(str, "%04d", year+1);
                str[4] = '-';
                month %= 12;
            }
            sprintf(str + 5, "%02d", month);
            str[7] = '-';
        } else {
            sprintf(str + 8, "%02lu", diff);
            str[10] = 'T';
            return;
        }
    }
}

unsigned int CumulocityPlatform::daysInMonth(int month, int year) {
    switch(month) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31;
    }

    if(month == 2) {
        if(isLeapYear(year)) {
            return 29;
        }
        return 28;
    } else {
        return 30;
    }
}

bool CumulocityPlatform::isLeapYear(int year) {
    return ( 0 == year % 4 && 0 != year % 100 || 0 == year % 400 );
}

// ------------------ GSMModule-related methods

void CumulocityPlatform::setGSM(GSMModule* mod) {
    this->mod = mod;
}

/* ############################## BASE64 ############################## */

void base64_encode(char* output, char const* input, unsigned int in_len) {
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int out_idx = 0;

    while (in_len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i < 4) ; i++)
                output[out_idx++] = base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            output[out_idx++] = base64_chars[char_array_4[j]];

        while((i++ < 3))
            output[out_idx++] = '=';
    }
    output[out_idx++] = '\0';
}
