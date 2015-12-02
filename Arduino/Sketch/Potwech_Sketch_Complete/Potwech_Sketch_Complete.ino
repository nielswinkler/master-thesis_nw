#include "CumulocityPlatform.h"
#include "SIM900.h"
#include "sms.h"
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>
#include "DHT.h"
#include "RTClib.h"
#include <Wire.h>
#include <TinyGPS++.h>
#include <SD.h>
#include <SPI.h>
#include <LSM303.h>


CumulocityPlatform cPlatform("developer.cumulocity.com", "xxxxxx", "xxxxxxxxxx", "xxxxxxxxxxxx", "xxxxxxxxxxxxxxxxxxxxx");
GSMModule* mod;
SMSGSM sms;

#define DHTPIN 22
#define DHTTYPE DHT22

RTC_DS1307 RTC;
LSM303 accelerometer;

String altValueString = "0";
String latValueString = "0";
String lngValueString = "0";

long sampleInterval = 10; // intervall for sampling raw data
long measureInterval = 60; // intervall for averaging the raw data
long uploadInterval = 300; // intervall for uploading saved averged data and saved events

int time; //unixtime
DateTime timeOld = 0; // last Sampling was made
DateTime lastMeasurement; // last Measurement was made
DateTime lastUpload; // last Upload was made
DateTime crashTime; 
DateTime measureTime;
DateTime openTime;
DateTime TimeBuffer;

//sum variables for averaging function:
float sumTemp = 0;
float sumHumi = 0;
float upavgTemp = 0;
float upavgHum = 0;
//avg variables for temperature and humidity
int avgTemp = 0;
int avgHum = 0;
int sendavgTemp = 0;
int sendavgHum = 0;
int loopCounter1 = 0; // number of measurements for computing the averages
int loopCounter2 = 0; // number of measurements for second time

String Stringbuffer = "0";
String buff2 = "";
String buff = "";

char altValue[12];
char latValue[12];
char lngValue[12];
char error[30];

  
boolean opened= false;
boolean crashed = false;

const int chipSelect = 53;
double accData[3];
double g; //Acceleration
double go=2000;

AltSoftSerial gpsSerial; // create gps sensor connection
TinyGPSPlus gps; // create gps object


DHT dht(DHTPIN, DHTTYPE);
File dataFile;
int result;
char id[8];

int Temp_Rsensor = 0;
int Hum_Rsensor = 0;
int Light_Rsensor = 0;
//RTC_Millis rtc;

DateTime now;

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  dht.begin();
  Wire.begin();
  RTC.begin(DateTime(F(__DATE__), F(__TIME__)));


  pinMode(A3, INPUT);


  Serial.println(F("Start"));
  mod = new GSMModule();
  Serial.println(F("Attaching GPRS..."));
  if (mod->attachGPRS("xxxxxxxxxxx", "", "")) {
    Serial.println(F("GPRS attached."));
    cPlatform.setGSM(mod);


    Serial.println(F("Registering a device..."));
    int result = cPlatform.registerDevice("POTWECHxxxxxxxxxxxxx", id , 8);
    Serial.println(result);
    if (result < 0) {
      Serial.println(F("Registration error."));
      while (true);
    } else {
      Serial.print(F("Arduino registered with id: "));
      Serial.println(id);
    }
  } else {
    Serial.println(F("Could not attach GPRS."));
    while (true);
  }
delay(500);
  if (!SD.begin(chipSelect)) {
    Serial.println("initialization SD CARD failed!");
  }
  delay(1000);
  Serial.println("Removing existing files.");
  if (SD.exists("LOGFILE.txt"))
    SD.remove("LOGFILE.txt");
  if (SD.exists("opened.txt"))
    SD.remove("opened.txt");
  if (SD.exists("crashed.txt"))
    SD.remove("crashed.txt");
  if (SD.exists("temperature.txt"))
    SD.remove("temperature.txt");
  if (SD.exists("humidity.txt"))
    SD.remove("humidity.txt");
  delay(1000);

  accelerometer.init();
  accelerometer.enableDefault();

  RTC.adjust(DateTime(__DATE__, __TIME__));
  TimeBuffer = RTC.now();
  while(TimeBuffer.unixtime() > 1500000000){
    TimeBuffer = RTC.now();
  }
  timeOld = TimeBuffer;
  lastMeasurement = TimeBuffer;
  lastUpload = TimeBuffer;

  buff = "";
  buff += TimeBuffer.year(), DEC;
  buff += '-';
  buff += TimeBuffer.month(), DEC;
  buff += '-';
  buff += TimeBuffer.day(), DEC;
  buff += 'T';
  buff += TimeBuffer.hour(), DEC;
  buff += ':';
  buff += TimeBuffer.minute(), DEC;
  buff += ':';
  buff += TimeBuffer.second(), DEC;
  char time[30];
  buff.toCharArray(time, 30);

  gsm.atWrite("AT+CIPGSMLOC=1,1");
  delay(2000);
  gsm.atWrite("AT+CIPGSMLOC=1,1");
  delay(2000);
  gsm.atWrite("AT+CIPGSMLOC=1,1");
  delay(2000);  
  
  
}

void loop() {

  now = RTC.now();
  Serial.println(now.unixtime());
  if((now.unixtime() - timeOld.unixtime()) < 3000){
  delay(100);
  Light_Rsensor = analogRead(3);

  
  if ((Light_Rsensor > 900) && (!opened)) {
  openTime = now;
  opened = true;
  Serial.println("EVENT OCCURED: Parcel opened!");
  smartDelay(1000);

  Stringbuffer = "Opened: ";

  buff="";
  buff += now.year(), DEC;
  buff += '-';
  buff += now.month(), DEC;
  buff += '-';
  buff += now.day(), DEC;
  buff += 'T';
  buff += now.hour(), DEC;
  buff += ':';
  buff += now.minute(), DEC;
  buff += ':';
  buff += now.second(), DEC;

  Stringbuffer += "Time = ";
  Stringbuffer += buff;
  Stringbuffer += " ; ";
  Stringbuffer += "Location: ";
  Stringbuffer += "lat: ";
  Stringbuffer += latValueString;
  Stringbuffer += " , ";
  Stringbuffer += "lng: ";
  Stringbuffer += lngValueString;

  Serial.println(Stringbuffer);
  printlnSD(Stringbuffer, 2);
  delay(300);
  //Save light event string to SD

  
  String buff2 = "Parcel opened ";
  buff2.toCharArray(error, 30);
  char time[30];
  buff.toCharArray(time, 30);
  Serial.println(F("Sending Alarm"));

  result = cPlatform.raiseAlarm("Parcel opened", "ACTIVE", "MAJOR", time, error);

  if (result > 0) {
    Serial.println(F("Alarm sent successfully."));
  } else {
    Serial.println(F("Alarm sending failed."));
  }
  //SendSMS();
  delay(500);
  Serial.print("hi");
  }
  
  
  accelerometer.read();
  accData[0] = (double) accelerometer.a.x;
  accData[1] = (double) accelerometer.a.y;
  accData[2] = (double) accelerometer.a.z;


  g = sqrt(accData[0]*accData[0]+accData[1]*accData[1]+accData[2]*accData[2])/10000;
  //Serial.println(g);
  

  

  if ((go!=g) && (g>3) && (!crashed))
  {
    crashTime = now;
    crashed = true;
    go = g;

    Serial.println("EVENT OCCURED: Parcel crashed!");
    smartDelay(1000);

    Stringbuffer = "Chrached: ";

    buff="";
    buff += now.year(), DEC;
    buff += '-';
    buff += now.month(), DEC;
    buff += '-';
    buff += now.day(), DEC;
    buff += 'T';
    buff += now.hour(), DEC;
    buff += ':';
    buff += now.minute(), DEC;
    buff += ':';
    buff += now.second(), DEC;
  
    Stringbuffer += "Time = ";
    Stringbuffer += buff;
    Stringbuffer += " ; ";
    Stringbuffer += "Location: ";
    Stringbuffer += "lat: ";
    Stringbuffer += latValueString;
    Stringbuffer += " , ";
    Stringbuffer += "lng: ";
    Stringbuffer += lngValueString;
  
    Serial.println(Stringbuffer);
    printlnSD(Stringbuffer, 3);
    delay(300);

    String buff2 = "Parcel crashed ";
    buff2.toCharArray(error, 30);
    char time[30];
    buff.toCharArray(time, 30);
    Serial.println(F("Sending Alarm"));

    result = cPlatform.raiseAlarm("Parcel crashed", "ACTIVE", "MAJOR", time, error);

    if (result > 0) {
      Serial.println(F("Alarm sent successfully."));
    } else {
      Serial.println(F("Alarm sending failed."));
    }
    delay(500);
  }
  
  //Serial.print(".");
  
  if (DiffBiggerOrEqual(now,timeOld,sampleInterval)) //Sampling intervall (according to intervall variable)
  {
    loopCounter1++;
    Serial.println("S A M P L E  N R. " + String(loopCounter1) + ",  T I M E: " + String(millis()/1000) + " S E C");
    //time = RTC.now();
  
    Temp_Rsensor = dht.readTemperature();
    Hum_Rsensor = dht.readHumidity();

    if(Temp_Rsensor <10){
      Serial.println("EVENT OCCURED: Temperature under 10C°");
      smartDelay(1000);
  
      Stringbuffer = "Temperature under 10C°: ";
  
      buff="";
      buff += now.year(), DEC;
      buff += '-';
      buff += now.month(), DEC;
      buff += '-';
      buff += now.day(), DEC;
      buff += 'T';
      buff += now.hour(), DEC;
      buff += ':';
      buff += now.minute(), DEC;
      buff += ':';
      buff += now.second(), DEC;
    
      Stringbuffer += "Time = ";
      Stringbuffer += buff;
      Stringbuffer += " ; ";
      Stringbuffer += "Location: ";
      Stringbuffer += "lat: ";
      Stringbuffer += latValueString;
      Stringbuffer += " , ";
      Stringbuffer += "lng: ";
      Stringbuffer += lngValueString;
    
      Serial.println(Stringbuffer);
      printlnSD(Stringbuffer, 3);
      delay(300);
  
      String buff2 = "Temperature under 10C° ";
      buff2.toCharArray(error, 30);
      char time[30];
      buff.toCharArray(time, 30);
      Serial.println(F("Sending Alarm"));
  
      result = cPlatform.raiseAlarm("Temperature under 10C°", "ACTIVE", "MAJOR", time, error);
  
      if (result > 0) {
        Serial.println(F("Alarm sent successfully."));
      } else {
        Serial.println(F("Alarm sending failed."));
      }
      delay(500);
    }

    if(Hum_Rsensor <20){
      Serial.println("EVENT OCCURED: Humidity under 20%");
      smartDelay(1000);
  
      Stringbuffer = "Humidity under 20%: ";
  
      buff="";
      buff += now.year(), DEC;
      buff += '-';
      buff += now.month(), DEC;
      buff += '-';
      buff += now.day(), DEC;
      buff += 'T';
      buff += now.hour(), DEC;
      buff += ':';
      buff += now.minute(), DEC;
      buff += ':';
      buff += now.second(), DEC;
    
      Stringbuffer += "Time = ";
      Stringbuffer += buff;
      Stringbuffer += " ; ";
      Stringbuffer += "Location: ";
      Stringbuffer += "lat: ";
      Stringbuffer += latValueString;
      Stringbuffer += " , ";
      Stringbuffer += "lng: ";
      Stringbuffer += lngValueString;
    
      Serial.println(Stringbuffer);
      printlnSD(Stringbuffer, 5);
      delay(300);
  
      String buff2 = "Humidity under 20% ";
      buff2.toCharArray(error, 30);
      char time[30];
      buff.toCharArray(time, 30);
      Serial.println(F("Sending Alarm"));
  
      result = cPlatform.raiseAlarm("Humidity under 20%", "ACTIVE", "MAJOR", time, error);
  
      if (result > 0) {
        Serial.println(F("Alarm sent successfully."));
      } else {
        Serial.println(F("Alarm sending failed."));
      }
      delay(500);
    }
     
     if(Temp_Rsensor >50){
      Serial.println("EVENT OCCURED: Temperature over 50C°");
      smartDelay(1000);
  
      Stringbuffer = "Temperature over 50C°: ";
  
      buff="";
      buff += now.year(), DEC;
      buff += '-';
      buff += now.month(), DEC;
      buff += '-';
      buff += now.day(), DEC;
      buff += 'T';
      buff += now.hour(), DEC;
      buff += ':';
      buff += now.minute(), DEC;
      buff += ':';
      buff += now.second(), DEC;
    
      Stringbuffer += "Time = ";
      Stringbuffer += buff;
      Stringbuffer += " ; ";
      Stringbuffer += "Location: ";
      Stringbuffer += "lat: ";
      Stringbuffer += latValueString;
      Stringbuffer += " , ";
      Stringbuffer += "lng: ";
      Stringbuffer += lngValueString;
    
      Serial.println(Stringbuffer);
      printlnSD(Stringbuffer, 3);
      delay(300);
  
      String buff2 = "Temperature over 50C° ";
      buff2.toCharArray(error, 30);
      char time[30];
      buff.toCharArray(time, 30);
      Serial.println(F("Sending Alarm"));
  
      result = cPlatform.raiseAlarm("Temperature over 50C°", "ACTIVE", "MAJOR", time, error);
  
      if (result > 0) {
        Serial.println(F("Alarm sent successfully."));
      } else {
        Serial.println(F("Alarm sending failed."));
      }
      delay(500);
    }

    if(Hum_Rsensor >90){
      Serial.println("EVENT OCCURED: Humidity over 90%");
      smartDelay(1000);
  
      Stringbuffer = "Humidity over 90%: ";
  
      buff="";
      buff += now.year(), DEC;
      buff += '-';
      buff += now.month(), DEC;
      buff += '-';
      buff += now.day(), DEC;
      buff += 'T';
      buff += now.hour(), DEC;
      buff += ':';
      buff += now.minute(), DEC;
      buff += ':';
      buff += now.second(), DEC;
    
      Stringbuffer += "Time = ";
      Stringbuffer += buff;
      Stringbuffer += " ; ";
      Stringbuffer += "Location: ";
      Stringbuffer += "lat: ";
      Stringbuffer += latValueString;
      Stringbuffer += " , ";
      Stringbuffer += "lng: ";
      Stringbuffer += lngValueString;
    
      Serial.println(Stringbuffer);
      printlnSD(Stringbuffer, 5);
      delay(300);
  
      String buff2 = "Humidity over 90% ";
      buff2.toCharArray(error, 30);
      char time[30];
      buff.toCharArray(time, 30);
      Serial.println(F("Sending Alarm"));
  
      result = cPlatform.raiseAlarm("Humidity over 90%", "ACTIVE", "MAJOR", time, error);
  
      if (result > 0) {
        Serial.println(F("Alarm sent successfully."));
      } else {
        Serial.println(F("Alarm sending failed."));
      }
      delay(500);
    }
    
    sumTemp += Temp_Rsensor;
    sumHumi += Hum_Rsensor;
    //Serial.println(lastMeasurement.unixtime());


//      Serial.println("hahaha");
  if (DiffBiggerOrEqual(now, lastMeasurement, measureInterval))
    {
      loopCounter2++;
      Serial.println("C O M P U T I N G  A V E R A G E S ");
      //compute the average
      avgTemp = (int)(sumTemp / loopCounter1);
      avgHum = (int)(sumHumi / loopCounter1);

      TimeBuffer = RTC.now();
      while(TimeBuffer.unixtime() > 1500000000){
        TimeBuffer = RTC.now();
      }
      lastMeasurement = TimeBuffer;
      
      loopCounter1 = 0; // setting number of measurements to zero
      
      // setting the sum's to zero
      sumTemp = 0;
      sumHumi = 0;

      // second avg
      upavgTemp += avgTemp;
      upavgHum += avgHum;

      Stringbuffer = "Save Data: ";
      Stringbuffer += "Temperatur = ";
      Stringbuffer += avgTemp;
      Stringbuffer += " ; ";
      Stringbuffer += "Humidity = ";
      Stringbuffer += avgHum;
      Stringbuffer += " ; ";
    
      buff="";
      buff += now.year(), DEC;
      buff += '-';
      buff += now.month(), DEC;
      buff += '-';
      buff += now.day(), DEC;
      buff += 'T';
      buff += now.hour(), DEC;
      buff += ':';
      buff += now.minute(), DEC;
      buff += ':';
      buff += now.second(), DEC;
    
      Stringbuffer += "Time = ";
      Stringbuffer += buff;

      Serial.println(Stringbuffer);
      printlnSD(Stringbuffer, 1);
      delay(300);
      
      
      if (DiffBiggerOrEqual(now, lastUpload, uploadInterval))
      {
        Serial.println("U P L O A D  D A T A");
        sendavgTemp = (int)(upavgTemp / loopCounter2);
        sendavgHum = (int)(upavgHum / loopCounter2);
        loopCounter2 = 0; // setting number of measurements to zero
        upavgTemp = 0;
        upavgHum = 0;
  
        smartDelay(1000);

        Stringbuffer = "Sended Data: ";
        Stringbuffer += "Temperatur = ";
        Stringbuffer += sendavgTemp;
        Stringbuffer += " ; ";
        Stringbuffer += "Humidity = ";
        Stringbuffer += sendavgHum;
        Stringbuffer += " ; ";
      
        buff="";
        buff += now.year(), DEC;
        buff += '-';
        buff += now.month(), DEC;
        buff += '-';
        buff += now.day(), DEC;
        buff += 'T';
        buff += now.hour(), DEC;
        buff += ':';
        buff += now.minute(), DEC;
        buff += ':';
        buff += now.second(), DEC;
      
        Stringbuffer += "Time = ";
        Stringbuffer += buff;
        Stringbuffer += " ; ";
        Stringbuffer += "Location: ";
        Stringbuffer += "lat: ";
        Stringbuffer += latValueString;
        Stringbuffer += " , ";
        Stringbuffer += "lng: ";
        Stringbuffer += lngValueString;
      
        Serial.println(Stringbuffer);
        printlnSD(Stringbuffer, 1);
        delay(300);

        sendMeasurements();
        delay(500);
        TimeBuffer = RTC.now();
        while(TimeBuffer.unixtime() > 1500000000){
          TimeBuffer = RTC.now();
        }
          lastUpload = TimeBuffer; // update time for last Upload
          lastMeasurement = TimeBuffer;
        
        
        
        opened = false;
        crashed = false;
      }
    }
    timeOld = now;
    delay(100); 
  }
 

  }

  
}

void sendMeasurements() {
   
  buff = "";
  buff += now.year(), DEC;
  buff += '-';
  buff += now.month(), DEC;
  buff += '-';
  buff += now.day(), DEC;
  buff += 'T';
  buff += now.hour(), DEC;
  buff += ':';
  buff += now.minute(), DEC;
  buff += ':';
  buff += now.second(), DEC;
  char time[30];
  buff.toCharArray(time, 30);

  delay(1000);

  if (Temp_Rsensor <= 2) {

    //Serial.println(error);
    String buff2 = "Temperature at ";
    buff2 += Temp_Rsensor;
    buff2 += "C!";
    buff2.toCharArray(error, 30);

    Serial.println(buff2);

    Serial.println(F("Sending Alarm"));

    result = cPlatform.raiseAlarm("Temperature Alarm", "CLEARED", "MAJOR", time, error);

    if (result > 0) {
      Serial.println(F("Alarm sent successfully."));
    } else {
      Serial.println(F("Alarm sending failed."));
    }
    for ( int i = 0; i < sizeof(error);  ++i )
      error[i] = (char)0;
  }


  Serial.println(F("Sending measurements"));

  delay(500);
  result = cPlatform.sendMeasurement("c8y_TemperatureMeasurement", time, "Temperature", "TEMP", sendavgTemp, "C");
  if (result > 0) {
    Serial.println(F("Measurement sent successfully."));
  } else {
    Serial.println(F("Measurement sending failed."));
  }
  delay(500);
  
  result = cPlatform.sendMeasurement("c8y_HumidityMeasurement", time, "Humidity", "Hum", sendavgHum, "%");
  if (result > 0) {
    Serial.println(F("Measurement sent successfully."));
  } else {
    Serial.println(F("Measurement sending failed."));
  }
  delay(500);

  if((latValueString != "0.000000")&&(lngValueString != "0.000000")){
  altValueString.toCharArray(altValue,11);
  lngValueString.toCharArray(lngValue,11);
  latValueString.toCharArray(latValue,11);
  delay(1000);
  result = cPlatform.sendEvent("c8y_LocationUpdate", "c8y_Position", altValue, lngValue, latValue, "LocUpdate", time);

  if (result > 0) {
    Serial.println(F("Event sent successfully."));
  } else {
    Serial.println(F("Event sending failed."));
  }
  }

    

}
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (gpsSerial.available())
      gps.encode(gpsSerial.read());
  } while (millis() - start < ms);

  latValueString = float2str(gps.location.lat(), 6);
  lngValueString = float2str(gps.location.lng(), 6);
  if{(latValueString = " ")&& (lngValueString = " "){
      delay(1000);
      gsm.atWrite("AT+CIPGSMLOC=1,1");
      delay(1000)
      c=gsm.SimpleRead();
      delay(1000);
      latValueString= float2str(c.location.lat(), 6)
      lngValueString= float2str(gps.location.lng(), 6);
      delay(500);
}

String float2str(float f, int n) {

  String s = "12345.67890";
  int i, k, st = 0;
  float teiler;
  double d, e;
  char c;

  s = "";
  d = f;
  for (i = 10000; i > 0; i /= 10) {
    // 10000er 1000er 100er 10er 1er
    k = (int)d / i;
    if ((k > 0) | st) {
      st = 1;
      c = k + 48; // ASCII
      s.concat(c);
    }
    d = d - (k * i);
  }
  if (st == 0) s.concat("0"); // wenigstens 0 ausgeben
  if (n > 0) s.concat("."); // Dezimalpunkt
  teiler = 0.1;
  for (i = 0; i < n; i++) {
    e = d / teiler; // 0.1er 0.01er 0.001er 0.0001er
    k = (int)e;
    c = k + 48; // ASCII
    s.concat(c);
    d = d - (k * teiler);
    teiler = teiler / 10.0;
  }
  return s;
}

void printlnSD(String str, int fileName)
{
  if (fileName == 1) {
    dataFile = SD.open("logfile.txt", FILE_WRITE);
  }
  else if (fileName == 2) {
    dataFile = SD.open("opened.txt", FILE_WRITE);
  }
  else if (fileName == 3) {
    dataFile = SD.open("crashed.txt", FILE_WRITE);
  }
  else if (fileName == 4) {
    dataFile = SD.open("temperature.txt", FILE_WRITE);
  }
  else if (fileName == 5) {
    dataFile = SD.open("humidity.txt", FILE_WRITE);
  }
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(str);
    delay(100);
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}
void ShowSerialData()
{
  while(gpsSerial.available()!=0)
    Serial.write(gpsSerial.read());
    delay(500);
    Serial.println("------");
}
boolean DiffBiggerOrEqual(DateTime a, DateTime b, long timeDiff){
  long c = a.unixtime() - b.unixtime();
  return (c >= timeDiff);
}
void SendSMS(){
      if (sms.SendSMS("xxxxxxxxxx", "Parcel opened"))
      Serial.println("\nSMS sent OK");
}

