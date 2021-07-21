/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#ifdef ESP32
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#else
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#endif
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SoftwareSerial.h>
#include <NMEAParser.h>

// Replace with your network credentials
#include "credentials.h"
//const char* ssid     = "xxxx";
//const char* password = "xxxx";
IPAddress staticIP(192, 168, 1, 80);
IPAddress gateway(192, 168, 1, 250);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);  //DNS
IPAddress dns2(8, 8, 4, 4);  //DNS

#define BAUD_RATE 4800
SoftwareSerial pb100Serial;

NMEAParser<2> parser;

float bp = 0;
float temp = 0;
float rh = 0;
float wdir = 0;
float wspd = 0;

float bpTotal = 0;
float bpCount = 0;
float tempTotal = 0;
float tempCount = 0;
float rhTotal = 0;
float rhCount = 0;
float wdirSinTotal = 0;
float wdirCosTotal = 0;
float wdirCount = 0;
float wspdTotal = 0;
float wspdCount = 0;
float wspdGust = 0;
float wdirGust = 0;

int timeYear = 0;
int timeMonth = 0;
int timeDay = 0;
int timeHour = 0;
int timeMinute = 0;
int timeSecond = 0;

int nextLog = -1;
const int logInt = 10;      // 10 minutes

const int BUFF_SIZE = 144;
//struct LogRecord {
//  char TimeStamp[20]; //dd-mm-yyyyThh:mm:ss
//  char BarometricPressure[7];  //xxxx.x
//  char Temperature[6];          //-xx.x
//  char RH[5];                  // xx.x
//  char WindDirection[4];         // xxx
//  char WindSpeed[5];            // xx.x
//  char WindGustDirection[4];     // xxx
//  char WindGustSpeed[5];         // xx.x
//};
const int BP_COL = 0;
const int TM_COL = 1;
const int RH_COL = 2;
const int WD_COL = 3;
const int WS_COL = 4;
const int GD_COL = 5;
const int GS_COL = 6;

#define BP_ID "bp"
#define TM_ID "tm"
#define RH_ID "rh"
#define WD_ID "wd"
#define WS_ID "ws"
#define GD_ID "gd"
#define GS_ID "gs"

#include "Wire.h"
#define TMP102_I2C_ADDRESS 72 /* This is the I2C address for our chip. This value is correct if you tie the ADD0 pin to ground. See the datasheet for some other values. */


// Store the log record as strings. This minimises processing when retrieving log data
// at the expense of RAM. We could possibly change to uint32 for time and float for data values.
// This would save about 40% RAM
struct LogRecord {
  char TimeStamp[20];
  char Data[7][7];    // 7 items of 7 characters each
};
LogRecord logBuffer[BUFF_SIZE];
int logPointer = 0;
bool logBufferWrapped = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//================================================================
// NTP time synch
#include <WiFiUdp.h>
#include <TimeLib.h>
static const char ntpServerName[] = "nz.pool.ntp.org";
const int timeZone = 0;     // UTC
const unsigned int localPort = 8888;  // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
time_t prevDisplay = 0; // when the digital clock was displayed
const uint32_t CLOCK_UPDATE_RATE = 1000;
uint32_t clockTimer = 0;       // time for next clock update
int lastSecond = 0;

//================================================================
// Connection methods
//================================================================
bool connect(int timeout) {

  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
   // Configures static IP address
  WiFi.config(staticIP, gateway, subnet, dns1, dns2);
  WiFi.begin(ssid, password);

  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED) {
    // Check to see if
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Failed to connect to WiFi. Please verify credentials: ");
      delay(10000);
    }

    delay(500);
    Serial.println("...");
    if (timeout > 0)
    {
      // Only try for 15 seconds.
      if (millis() - wifiConnectStart > 15000) {
        Serial.println("Failed to connect to WiFi");
        return false;
      }
    }
    yield();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.println("Connected!");
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  return true;
}
//==============================================================================
// NTP Methods
//==============================================================================
time_t getNtpTime()
{
  WiFiUDP Udp;
  Udp.begin(localPort);
  IPAddress ntpServerIP; // NTP server's ip address
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sendNTPpacket(ntpServerIP, Udp);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address, WiFiUDP &Udp )
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

char* getISO8601Time(boolean zeroSeconds)
{
  static char timeString[20];

  char buff[5];
  int yr = year();
  int mo = month();
  int da = day();
  int hr = hour();
  int mi = minute();
  int se;
  if (zeroSeconds)
  {
    se = 0;
  }
  else
  {
    se = second();
  }

  itoa(yr, buff, 10);
  strcpy(timeString, buff);
  strcat(timeString, "-");
  itoa(mo, buff, 10);
  if (mo < 10)
  {
    strcat(timeString, "0");
  }
  strcat(timeString, buff);
  strcat(timeString, "-");
  itoa(da, buff, 10);
  if (da < 10)
  {
    strcat(timeString, "0");
  }
  strcat(timeString, buff);
  strcat(timeString, "T");
  itoa(hr, buff, 10);
  if (hr < 10)
  {
    strcat(timeString, "0");
  }
  strcat(timeString, buff);
  strcat(timeString, ":");
  itoa(mi, buff, 10);
  if (mi < 10)
  {
    strcat(timeString, "0");
  }
  strcat(timeString, buff);
  strcat(timeString, ":");
  itoa(se, buff, 10);
  if (se < 10)
  {
    strcat(timeString, "0");
  }
  strcat(timeString, buff);
  return timeString;
}
//================================================================
//NMEA callbacks
//================================================================

//------------------------------------------------------------
// weather data callback
//------------------------------------------------------------
void handleWIMDA(void)
{
  float result;
  if (parser.getArg(3, result))
  {
    bp = result;
    bpTotal += bp;
    bpCount++;
  }
  if (parser.getArg(5, result))
  {
    temp = result;
    tempTotal += temp;
    tempCount++;
  }
  if (parser.getArg(9, result))
  {
    rh = result;
    rhTotal += rh;
    rhCount++;
  }
  if (parser.getArg(13, result))
  {
    wdir = result;
    float wdirRad = (wdir * 71) / 4068;
    wdirSinTotal += sin(wdirRad);
    wdirCosTotal += cos(wdirRad);
    wdirCount++;
  }
  if (parser.getArg(19, result))
  {
    wspd = result;
    wspdTotal += wspd;
    wspdCount++;
    if (wspd > wspdGust)
    {
      wspdGust = wspd;
      wdirGust = wdir;
    }
  }
}

//------------------------------------------------------------
// NMEA GPS date/time callback
//------------------------------------------------------------
void handleGPZDA(void)
{
  int result = 0;
  if (parser.getArg(1, result))
  {
    //hhmmss as UTC
    int utc = result;
    timeHour = utc / 10000;
    timeMinute = (utc - timeHour * 10000) / 100;
    timeSecond = (utc - timeHour * 10000 - timeMinute * 100);
  }
  if (parser.getArg(2, result))
  {
    //Day, 01 to 31
    timeDay = result;
  }
  if (parser.getArg(3, result))
  {
    //Month, 01 to 12
    timeMonth = result;
  }
  if (parser.getArg(3, result))
  {
    // Year, four digits (e.g. 2006)
    timeYear = result;
  }
  checkLog();
}

//================================================================
// Data logging methods
//================================================================

//------------------------------------------------------------
// Check to see if it's time to log data
//------------------------------------------------------------
void checkLog()
{
  if (nextLog == -1)
  {
    // Not initialised yet
    setNextLog();
    Serial.print("Next log initialised to: ");
    Serial.println(nextLog);
  }
  if (nextLog == timeMinute)
  {
    saveLog();
    setNextLog();
    Serial.print("Next log set to: ");
    Serial.println(nextLog);
  }
}

//------------------------------------------------------------
// On midnight, dump the data to SPIFFS
// To maintain a 7 day rolling buffer
// rename "temperature.6" to "temperature.7"
//        "temperature.5" to "temperature.6"
//        ...
// write  "temperature.0"
//------------------------------------------------------------
void checkMidnight()
{
  //TODO:
}

//------------------------------------------------------------
// Set the next log interval (in minutes)
//------------------------------------------------------------
void setNextLog()
{
  //nextLog = ((timeMinute / 10) * 10) + 10;
  nextLog = timeMinute + 1;
  if (nextLog >= 60)
  {
    nextLog -= 60;
  }
}

//------------------------------------------------------------
// Save data to log buffer
//------------------------------------------------------------
void saveLog()
{
  strcpy(logBuffer[logPointer].TimeStamp, getISO8601Time(false));
  float mean = 0;
  // Calculate mean and max and save
  if (bpCount > 0)
  {
    mean = bpTotal / bpCount;
  }
  else
  {
    mean = 0;
  }
  bpTotal = 0;
  bpCount = 0;
  dtostrf(mean, -7, 1, logBuffer[logPointer].Data[BP_COL]);

  if (tempCount > 0)
  {
    mean = tempTotal / tempCount;
  }
  else
  {
    mean = 0;
  }
  tempTotal = 0;
  tempCount = 0;
  dtostrf(mean, -6, 1, logBuffer[logPointer].Data[TM_COL]);

  if (rhCount > 0)
  {
    mean = rhTotal / rhCount;
  }
  else
  {
    mean = 0;
  }
  rhTotal = 0;
  rhCount = 0;
  dtostrf(mean, -5, 1, logBuffer[logPointer].Data[RH_COL]);

  if (wspdCount > 0)
  {
    mean = wspdTotal / wspdCount;
  }
  else
  {
    mean = 0;
  }
  wspdTotal = 0;
  wspdCount = 0;
  dtostrf(mean, -5, 1, logBuffer[logPointer].Data[WS_COL]);

  if (wdirCount > 0)
  {
    float meanSin = wdirSinTotal / wspdCount;
    float meanCos = wdirCosTotal / wspdCount;
    mean = atan2(meanSin, meanCos);
    if (mean < 0)
    {
      mean = mean + 360;
    }
  }
  else
  {
    mean = 0;
  }
  wdirSinTotal = 0;
  wdirCosTotal = 0;
  wspdCount = 0;
  dtostrf(mean, -4, 0, logBuffer[logPointer].Data[WD_COL]);

  dtostrf(wspdGust, -5, 1, logBuffer[logPointer].Data[GS_COL]);
  dtostrf(wdirGust, -4, 0, logBuffer[logPointer].Data[GD_COL]);

  wspdGust = 0;
  wdirGust = 0;

  logPointer ++;
  if (logPointer >= BUFF_SIZE)
  {
    logPointer = 0;
    logBufferWrapped = true;
  }
}

//------------------------------------------------------------
// Map sensor ID to sensor value
//------------------------------------------------------------
String getCurrentValue(int sensor)
{
  if (sensor == TM_COL)
  {
    return String(temp);
  }
  else if (sensor == RH_COL)
  {
    return String(bp);
  }
  else if (sensor == BP_COL)
  {
    return String(rh);
  }
  else if (sensor == WD_COL)
  {
    return String(wdir);
  }
  else if (sensor == WS_COL)
  {
    return String(wspd);
  }
}

//------------------------------------------------------------
// Return the 24 rolling data buffer
//------------------------------------------------------------
char* getCurrentData(int sensor)
{
  static char buffer[28 * BUFF_SIZE];
  //dd-mm-yyyyThh:mm:ss,xxxx.x\n   - worst case
  buffer[0] = 0;
  if (logBufferWrapped)
  {
    for (int record = logPointer; record < BUFF_SIZE; record++)
    {
      strcat(buffer, logBuffer[record].TimeStamp); // timestamp 20 bytes including NULL
      strcat(buffer, ",");
      strcat(buffer, logBuffer[record].Data[sensor]);
      strcat(buffer, "\n");
    }
    for (int record = 0; record < logPointer; record++)
    {
      strcat(buffer, logBuffer[record].TimeStamp); // timestamp 20 bytes including NULL
      strcat(buffer, ",");
      strcat(buffer, logBuffer[record].Data[sensor]);
      strcat(buffer, "\n");
    }
  }
  else
  {
    if (logPointer > 0)
    {
      for (int record = 0; record < logPointer; record++)
      {
        strcat(buffer, logBuffer[record].TimeStamp); // timestamp 20 bytes including NULL
        strcat(buffer, ",");
        strcat(buffer, logBuffer[record].Data[sensor]);
        strcat(buffer, "\n");
      }
    }
  }
  return buffer;
}

//------------------------------------------------------------
// Map sensor name to column index
//------------------------------------------------------------
int getSensorIndex(String sensor)
{
  if (sensor == TM_ID)
  {
    return TM_COL;
  }
  else if (sensor == RH_ID)
  {
    return RH_COL;
  }
  else if (sensor == BP_ID)
  {
    return BP_COL;
  }
  else if (sensor == WD_ID)
  {
    return WD_COL;
  }
  else if (sensor == WS_ID)
  {
    return WS_COL;
  }
  else if (sensor == GD_ID)
  {
    return GD_COL;
  }
  else if (sensor == GS_ID)
  {
    return GS_COL;
  }
  else
  {
    return -1;
  }
}

//------------------------------------------------------------
// Return file list and used/free bytes
//------------------------------------------------------------
String listFiles()
{
  String str = "";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    str += dir.fileName();
    str += " [";
    str += dir.fileSize();
    str += "]\r\n\r\n";
  }
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  str += "Bytes Used : " + String(fs_info.usedBytes) + "\r\n";
  str += "Bytes Total: " + String(fs_info.totalBytes) + "\r\n";
  str += "Bytes Free : " + String(fs_info.totalBytes - fs_info.usedBytes) + "\r\n";
  return str;
}

//------------------------------------------------------------
// Handlers for HTTP requests
//------------------------------------------------------------
void initServerRoutes()
{
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
  });

  // Test web page
  server.on("/test.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/test.html");
  });

  server.on("/highcharts.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/highcharts.js.gz", "text/plain");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    //request->send(SPIFFS, "/highcharts.js");
  });

  server.on("/sensor", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->hasParam("id"))
    {
      // Send error
      request->send_P(400, "text/plain", "ERROR: No sensor id query ?id=xx");
      return;
    }
    String id = request->getParam("id")->value();
    int sensor = getSensorIndex(id);
    if (sensor < 0)
    {
      // Send error
      request->send_P(400, "text/plain", "ERROR: Invalid sensor id query ?id=??");
      return;
    }
    if (request->hasParam("day"))
    {
      String day = request->getParam("day")->value();
      if (day == "0")
      {
        request->send_P(200, "text/plain", getCurrentData(sensor));
      }
      else
      {
        String filePath = "/" + id + "." + day;
        if (SPIFFS.exists(filePath))
        {
          request->send(SPIFFS, filePath, "text/plain");
        }
        else
        {
          request->send_P(400, "text/plain", "ERROR: no data available");
        }
      }
    }
    else
    {
      request->send_P(200, "text/plain", getCurrentValue(sensor).c_str());
    }
  });

  server.on("/heapfree", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(ESP.getFreeHeap()).c_str());
  });
  server.on("/time", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getISO8601Time(false));
  });
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", listFiles().c_str());
  });
}
//================================================================

void getTemp102()
{
  /* Reset the register pointer (by default it is ready to read temperatures)
    You can alter it to a writeable register and alter some of the configuration -
    the sensor is capable of alerting you if the temperature is above or below a specfied threshold. */

  Wire.beginTransmission(TMP102_I2C_ADDRESS); //Say hi to the sensor.
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(TMP102_I2C_ADDRESS, 2);
  Wire.endTransmission();

  byte b1 = Wire.read();
  byte b2 = Wire.read();

  int temp16 = (b1 << 4) | (b2 >> 4);    // builds 12-bit value
  float deg_c = temp16 * 0.0625;
  temp = deg_c;
  tempTotal += temp;
  tempCount++;
}

//================================================================
void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  Wire.begin(); // start the I2C library

  // RS485 serial port
  pb100Serial.begin(BAUD_RATE, SWSERIAL_8N1, D7, D8, false, 95, 11);

  // Send config data
  pb100Serial.print("$PAMTC,EN,ALL,0*1D");       // Disable all
  pb100Serial.print("$PAMTC,EN,ZDA,1,10*2F");   // Add GPS time, 1 second update
  pb100Serial.print("$PAMTC,EN,MDA,1,30*3A");   // Add weather composite, 3 second update

  parser.addHandler("WIMDA", handleWIMDA);
  parser.addHandler("GPZDA", handleGPZDA);

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  if (!connect(-1))
  {
    // Switch to STA mode

  }

  // Print Local IP Address
  Serial.println(WiFi.localIP());

  initServerRoutes();

  // Start server
  server.begin();

  //================================================
  // Init NTP and time
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
}

void loop() {

  while (pb100Serial.available()) {
    parser << pb100Serial.read();
    yield();
  }

  timeSecond = second();
  if (timeSecond != lastSecond)
  {
    getTemp102();
    lastSecond = timeSecond;
    timeYear = year();
    timeMonth = month();
    timeDay = day();
    timeHour = hour();
    timeMinute = minute();
    checkLog();
    digitalWrite(LED_BUILTIN, LOW);
    delay(20);
    digitalWrite(LED_BUILTIN, HIGH);
  }
}
