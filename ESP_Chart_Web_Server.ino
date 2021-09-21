// for ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>

#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <SoftwareSerial.h>
#include <NMEAParser.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include "LittleFS.h"

IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);  //DNS
IPAddress dns2(8, 8, 4, 4);  //DNS

const char* apSSID = "Weather Station";
IPAddress apIP(192, 168, 1, 100);
IPAddress apGateway(192, 168, 1, 100);
IPAddress apSubnet(255, 255, 255, 0);

// User settings
char userSSID[20];
char userPass[20];
char userIP[20] = "192.168.1.100";        //xxx.xxx.xxx.xxx\0
char userGateway[20] = "192.168.1.250";   //xxx.xxx.xxx.xxx\0
char userProxy[20] = "192.168.1.130";     //xxx.xxx.xxx.xxx\0
int userUTCOffset = 720;
int userElevation = 135;

bool apMode = false;
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

#define BAUD_RATE 4800
SoftwareSerial pb100Serial;

NMEAParser<2> parser;

// Output variables
float bp = 0;
float temp = 0;
float rh = 0;
float wdir = 0;
float wspd = 0;

// Averaging variables
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
byte timeMonth = 0;
byte timeDay = 0;
byte timeHour = 0;
byte timeMinute = 0;
byte timeSecond = 0;

int nextLog = -1;
const int logInt = 10;      // 10 minutes
byte restartFlag = 0;
byte lastLogDay = 0;

const byte BUFF_SIZE = 144;
const byte BP_COL = 0;
const byte TE_COL = 1;
const byte RH_COL = 2;
const byte WD_COL = 3;
const byte WS_COL = 4;
const byte GD_COL = 5;
const byte GS_COL = 6;

#define BP_ID "bp"
#define TE_ID "te"
#define RH_ID "rh"
#define WD_ID "wd"
#define WS_ID "ws"
#define GD_ID "gd"
#define GS_ID "gs"

// Optional A/D
Adafruit_ADS1015 ads1015;

// Store the log record as strings. This minimises processing when retrieving log data
// at the expense of RAM. We could possibly change to uint32 for time and float for data values.
// This would save about 40% RAM
#define LOGITEMS_COUNT 7
const byte LOGFILEDAYS = 7;
struct LogRecord {
  char TimeStamp[20];
  char Data[LOGITEMS_COUNT][7];    // 7 items of 7 characters each
};
LogRecord logBuffer[BUFF_SIZE];
byte logPointer = 0;
bool logBufferWrapped = false;

//================================================================
// NTP time synch
static const char ntpServerName[] = "nz.pool.ntp.org";
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
bool connectSTA(int timeout) {

  // Clear previous settings
  WiFi.disconnect(true);

  // Connect to Wifi.
  IPAddress ip;

  int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0 ;
  if (sscanf(userIP, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4) {
    ip = IPAddress(ip1, ip2, ip3, ip4);
  }

  IPAddress gateway;
  if (sscanf(userGateway, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4) {
    gateway = IPAddress(ip1, ip2, ip3, ip4);
  }

  WiFi.mode(WIFI_STA);
  // Configures static IP address
  WiFi.config(ip, gateway, subnet, dns1, dns2);
  WiFi.begin(userSSID, userPass);
  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED) {
    // Check to see if
    if (WiFi.status() == WL_CONNECT_FAILED) {
      return false;
    }

    delayWithYield(500);
    if (timeout > 0)
    {
      // Only try for 15 seconds.
      if (millis() - wifiConnectStart > timeout) {
        return false;
      }
    }
    yield();
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return true;
}

void connectAP()
{
  // Clear previous settings
  WiFi.disconnect(true);
  WiFi.softAP(apSSID, "pb100");    // just pass SSID for open network
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  delayWithYield(100);
  apMode = true;
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
      return secsSince1900 - 2208988800UL + userUTCOffset * 60;
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
  // Filter erroneous values otherwise we can get overflow of fixed width strings which corrupt log data
  float result;
  float newVal;
  if (parser.getArg(2, result))
  {
    newVal = result * 1000;
    // Correct to MSL
    newVal = newVal * pow((1 - ((0.0065 * userElevation) / (temp + 0.0065 * userElevation + 273.15))), -5.257);
    if (newVal < 1100 && newVal > 900)
    {
      bp = newVal;
      bpTotal += bp;
      bpCount++;
    }
  }
  // Don't use internal temperature sensor
  // TODO: use user configuration switch
  //if (parser.getArg(4, result))
  //{
  //  temp = result;
  //  tempTotal += temp;
  //  tempCount++;
  //}
  if (parser.getArg(8, result))
  {
    newVal = result;
    if (newVal >= 0 && newVal <= 100)
    {
      rh = newVal;
      rhTotal += rh;
      rhCount++;
    }
  }
  if (parser.getArg(12, result))
  {
    newVal = result;
    if (newVal >= 0 && newVal < 360)
    {
      wdir = newVal;
      float wdirRad = wdir * PI / 180;
      wdirSinTotal += sin(wdirRad);
      wdirCosTotal += cos(wdirRad);
      wdirCount++;
    }
  }
  if (parser.getArg(18, result))
  {
    newVal = result;
    if (newVal >= 0 && newVal < 100)
    {
      wspd = newVal;
      wspdTotal += wspd;
      wspdCount++;
      if (wspd > wspdGust)
      {
        wspdGust = wspd;
        wdirGust = wdir;
      }
    }
  }
}

//------------------------------------------------------------
// NMEA GPS date/time callback
//------------------------------------------------------------
void handleGPZDA(void)
{
  Serial.println("GPZDA received");
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
  }
  if (nextLog == timeMinute)
  {
    if (timeDay != lastLogDay && timeHour == 0 && timeMinute == 0)
    {
      // Shift before we log the midnight record so a day starts on midnight
      shiftFiles();
      lastLogDay = timeDay;
    }
    saveLog();
    saveDailyData();
    setNextLog();
  }
}

//------------------------------------------------------------
// On midnight, dump the data to
// To maintain a 7 day rolling buffer
// rename "temperature.6" to "temperature.7"
//        "temperature.5" to "temperature.6"
//        ...
// write  "temperature.0"
//------------------------------------------------------------
void saveDailyData()
{
  // Rename file day extentions e.g te.0 becomes te.1 etc.
  // latest data is then logged to te.0
  for (byte i = 0; i < LOGITEMS_COUNT; i++)
  {
    char* sensorCode = getSensorCode(i);
    saveToFile(i, sensorCode);
  }
}

void shiftFiles()
{
  for (byte i = 0; i < LOGITEMS_COUNT; i++)
  {
    char* sensorCode = getSensorCode(i);
    char srcfile[] = "/ID.X\0";   // e.g. /te.0
    char dstfile[] = "/ID.X\0";   // e.g. /te.0

    memcpy(&srcfile[1], sensorCode, 2);
    memcpy(&dstfile[1], sensorCode, 2);

    dstfile[4] = (char)(LOGFILEDAYS + 48);
    srcfile[4] = (char)(LOGFILEDAYS + 48 - 1);
    // Delete the oldest file e.g te.7
    if (LittleFS.exists(dstfile))
    {
      LittleFS.remove(dstfile);
    }

    for (int i = LOGFILEDAYS - 1; i >= 0; i--)
    {
      if (LittleFS.exists(srcfile))
      {
        LittleFS.rename(srcfile, dstfile);
      }
      dstfile[4] = (char)(i + 48);
      srcfile[4] = (char)(i + 48 - 1);
    }
  }
}

// Erase all log data files
void eraseLogFiles()
{
  for (byte logItem = 0; logItem < LOGITEMS_COUNT; logItem++)
  {
    char* sensorCode = getSensorCode(logItem);
    char srcfile[] = "/ID.X\0";   // e.g. /te.0

    memcpy(&srcfile[1], sensorCode, 2);

    for (int logDay = 0; logDay < LOGFILEDAYS; logDay++)
    {
      srcfile[4] = (char)(logDay + 48);
      Serial.print("Deleting file: ");
      Serial.print(srcfile);
      if (LittleFS.exists(srcfile))
      {
        LittleFS.remove(srcfile);
        Serial.println("...OK");
      }
    }
  }  
}

void saveToFile(byte sensorId, char* sensorCode)
{
  char srcfile[] = "/ID.0\0";
  memcpy(&srcfile[1], sensorCode, 2);
  File f = LittleFS.open(srcfile, "w");
  f.write(getCurrentData(sensorId));
  f.close();
}

//------------------------------------------------------------
// Set the next log interval (in minutes)
//------------------------------------------------------------
void setNextLog()
{
  nextLog = ((timeMinute / 10) * 10) + 10;
  //nextLog = timeMinute + 1;
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
  strcpy(logBuffer[logPointer].TimeStamp, getISO8601Time(true));
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
  dtostrf(mean, -6, 1, logBuffer[logPointer].Data[TE_COL]);

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
    float meanSin = wdirSinTotal / wdirCount;
    float meanCos = wdirCosTotal / wdirCount;
    mean = atan2(meanSin, meanCos);
    // Convert from radians to degrees
    mean = mean * 180 / PI;
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
  wdirCount = 0;
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
  if (sensor == TE_COL)
  {
    return String(temp, 1);
  }
  else if (sensor == RH_COL)
  {
    return String(rh, 1);
  }
  else if (sensor == BP_COL)
  {
    return String(bp, 1);
  }
  else if (sensor == WD_COL)
  {
    return String(wdir, 0);
  }
  else if (sensor == WS_COL)
  {
    return String(wspd, 1);
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
  if (sensor == TE_ID)
  {
    return TE_COL;
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
// Map column index to sensor name
//------------------------------------------------------------
char* getSensorCode(int column)
{
  if (column == TE_COL)
  {
    return TE_ID;
  }
  else if (column == RH_COL)
  {
    return RH_ID;
  }
  else if (column == BP_COL)
  {
    return BP_ID;
  }
  else if (column == WD_COL)
  {
    return WD_ID;
  }
  else if (column == WS_COL)
  {
    return WS_ID;
  }
  else if (column == GD_COL)
  {
    return GD_ID;
  }
  else if (column == GS_COL)
  {
    return GS_ID;
  }
  else
  {
    return TE_ID;
  }
}

//------------------------------------------------------------
// Return file list and used/free bytes
//------------------------------------------------------------
String listFiles()
{
  String str = "";
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    str += dir.fileName();
    str += " [";
    str += dir.fileSize();
    str += "]\r\n\r\n";
  }
  FSInfo fs_info;
  LittleFS.info(fs_info);
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
  //======================================================================
  // File handlers with compression. Must be an easier way of handling multiple files...
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  // If client not on a 192.168 address then return version with no Settings page (blocked in the API as well)
  server.on("/app.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    IPAddress source = request->client()->remoteIP();
    if (source[0] == 192 && source[1] == 168 && source[2] == 1 && source[3] != 130)
    {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/app.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }
    else
    {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/app-no-settings.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }
  });

  server.on("/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/bootstrap.bundle.min.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/feather.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/feather.min.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/highcharts.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/highcharts.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/highcharts-more.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/highcharts-more.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/jquery.min.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/logdata.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/logdata.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/logdatawind.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/logdatawind.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/PB100.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/pb100.png.gz", "text/plain");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/realtime.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/realtime.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/scripts.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/scripts.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  // Block anything not on a 192.168 address
  server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    IPAddress source = request->client()->remoteIP();
    if (source[0] == 192 && source[1] == 168 && source[2] == 1 && source[3] != 130)
    {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/settings.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }
    else
    {
      request->send(404);
    }
  });

  server.on("/solid-gauge.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/solid-gauge.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/styles.css.gz", "text/css");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/summary.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/summary.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/windbarb.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/windbarb.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  //======================================================================
  // Load / Save settings
  // Block anything not on a 192.168 address
  server.on("/update-settings", HTTP_POST, [](AsyncWebServerRequest * request) {}, NULL, [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    IPAddress source = request->client()->remoteIP();
    if (source[0] == 192 && source[1] == 168 && source[2] == 1 && source[3] != 130)
    {
      if (saveSettings((char*)data, len, index, total))
      {
        request->send(200);
      }
      else
      {
        request->send(500);
      }
    }
    else
    {
      request->send(404);
    }
  });

  // Block anything not on a 192.168 address
  server.on("/read-settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    IPAddress source = request->client()->remoteIP();
    if (source[0] == 192 && source[1] == 168 && source[2] == 1 && source[3] != 130)
    {
      if (LittleFS.exists("/api-settings.txt"))
      {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/api-settings.txt", "text/plain");
        request->send(response);
      }
      else
      {
        request->send(200);
      }
    }
    else
    {
      request->send(404);
    }
  });

  //======================================================================
  // Diagnostics API
  
  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest * request) {
    //request->send(200);
    request->send_P(200, "text/plain", "Rebooting module, wait for module to connect to WiFi before continuing");
    restartFlag = 1;
  });

  server.on("/heapfree", HTTP_GET, [](AsyncWebServerRequest * request) {
    uint32_t free;
    uint16_t max;
    uint8_t frag;
    ESP.getHeapStats(&free, &max, &frag);
    char mem[40];   
    int ptr=0;
    ptr += sprintf(&mem[ptr], "Free: %d\n", free);
    ptr += sprintf(&mem[ptr], "Max : %d\n", max);
    ptr += sprintf(&mem[ptr], "Frag: %d\n", frag);
    request->send_P(200, "text/plain", mem);
  });

  server.on("/files", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", listFiles().c_str());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", getStatus().c_str());
  });

  server.on("/rssi", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(WiFi.RSSI()).c_str());
  });

  server.on("/erase", HTTP_GET, [](AsyncWebServerRequest * request) {
    IPAddress source = request->client()->remoteIP();
    if (source[0] == 192 && source[1] == 168 && source[2] == 1 && source[3] != 130)
    {
      eraseLogFiles();
      request->send_P(200, "text/plain", "OK");
    }
    else
    {
      request->send(404);
    }
  });

  //======================================================================
  // Data API
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
      String filePath = "/" + id + "." + day;
      if (LittleFS.exists(filePath))
      {
        request->send(LittleFS, filePath, "text/plain");
      }
      else
      {
        request->send_P(400, "text/plain", "ERROR: no data available");
      }
    }
    else
    {
      request->send_P(200, "text/plain", getCurrentValue(sensor).c_str());
    }
  });
}

String getStatus()
{
  //{"dt": "2021-08-07T20:19:47", "te": "5.4", "ws": "99.5", "wd": "235", "rh": "19.0", "bp": "1052.2"}
  String response = "{\"dt\": \"";
  response += String(getISO8601Time(false));
  response += "\", \"te\": \"";
  response += getCurrentValue(TE_COL);
  response += "\", \"ws\": \"";
  response += getCurrentValue(WS_COL);
  response += "\", \"wd\": \"";
  response += getCurrentValue(WD_COL);
  response += "\", \"rh\": \"";
  response += getCurrentValue(RH_COL);
  response += "\", \"bp\": \"";
  response += getCurrentValue(BP_COL);
  response += "\"}";
  return response;
}

bool saveSettings(char *data, size_t len, size_t index, size_t total)
{
  // Convert \n to \0
  for (size_t i = 0; i < len; i++) {
    if (data[i] == 10)
    {
      data[i] = 0;
    }
  }

  int pos = 0;
  size_t retlen = strlcpy(userSSID, data, 20);
  pos = pos + retlen + 1;
  Serial.println(userSSID);

  retlen = strlcpy(userPass, &data[pos], 20);
  pos = pos + retlen + 1;
  Serial.println(userPass);

  retlen = strlcpy(userIP, &data[pos], 20);
  pos = pos + retlen + 1;
  Serial.println(userIP);

  retlen = strlcpy(userProxy, &data[pos], 20);
  pos = pos + retlen + 1;
  Serial.println(userProxy);

  retlen = strlcpy(userProxy, &data[pos], 20);
  pos = pos + retlen + 1;
  Serial.println(userProxy);

  userUTCOffset = atoi(&data[pos]);
  Serial.println(userUTCOffset);

  File file = LittleFS.open("/settings.txt", "w");
  // Save to LittleFS
  if (!file) {
    return false;
  }
  file.println(userSSID);
  file.println(userPass);
  file.println(userIP);
  file.println(userGateway);
  file.println(userProxy);
  file.println(userUTCOffset);
  file.close();

  file = LittleFS.open("/api-settings.txt", "w");
  // Save to LittleFS without password for sending via API
  if (!file) {
    return false;
  }
  file.println(userSSID);
  file.println(userIP);
  file.println(userGateway);
  file.println(userProxy);
  file.println(userUTCOffset);
  file.close();
  return true;
}

// Load settings from file
bool loadSettings()
{
  if (LittleFS.exists("/settings.txt"))
  {
    File file = LittleFS.open("/settings.txt", "r");
    if (!file) {
      return false;
    }
    char buffer[20];
    if (file.available())
    {
      int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l - 1] = 0;
      Serial.println(buffer);
      memcpy(userSSID, buffer, 20);
    }
    if (file.available())
    {
      int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l - 1] = 0;
      Serial.println(buffer);
      memcpy(userPass, buffer, 20);
    }
    if (file.available())
    {
      int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l - 1] = 0;
      Serial.println(buffer);
      memcpy(userIP, buffer, 20);
    }
    if (file.available())
    {
      int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l - 1] = 0;
      Serial.println(buffer);
      memcpy(userGateway, buffer, 20);
    }
    if (file.available())
    {
      int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l - 1] = 0;
      Serial.println(buffer);
      memcpy(userProxy, buffer, 20);
    }
    if (file.available())
    {
      int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l - 1] = 0;
      Serial.println(buffer);
      userUTCOffset = atoi(buffer);
    }
  }
}

// Data is saved to the *.0 files every log interval. Reload on startup
void loadCachedData()
{
  ESP.wdtDisable();

  char srcfile[] = "/ID.0\0";
  char buffer[20];
  for (int i = 0; i < 20; i++)
  {
    buffer[i] = 'X';
  }
  int row = 0;

  for (byte i = 0; i < LOGITEMS_COUNT; i++)
  {
    row = 0;
    char* sensorCode = getSensorCode(i);
    memcpy(&srcfile[1], sensorCode, 2);
    if (LittleFS.exists(srcfile))
    {
      File f = LittleFS.open(srcfile, "r");
      while (true)
      {
        // Timestamp
        if (f.available())
        {
          int l = f.readBytesUntil(',', buffer, sizeof(buffer));
          if (i == 0)
          {
            memcpy(logBuffer[row].TimeStamp, buffer, l);;
          }
        }
        // Data value
        if (f.available()) {
          int l = f.readBytesUntil('\n', buffer, sizeof(buffer));
          if (l == 0)
          {
            // End of file not terminated with \n
            f.readBytes(logBuffer[row].Data[i], 7);
          }
          else
          {
            //Copy into log buffer - do we need to null terminate it?
            memcpy(logBuffer[row].Data[i], buffer, l);
          }
          row++;
        }
        else
        {
          // EOF
          break;
        }

        ESP.wdtFeed();
      }
      f.close();
    }
  }
  logPointer = row;
  ESP.wdtEnable(1000);
}


void debugChar(char* buff, int len)
{
  for (int i = 0; i < len; i++)
  {
    Serial.print("[");
    Serial.print((byte)buff[i]);
    Serial.print("]");
  }
  Serial.println();
}

void restartWithDelay()
{
  delayWithYield(1000);
  ESP.restart();
}
//================================================================

void readLM34()
{
  float a0_mV = ads1015.readADC_SingleEnded(0) * 3;
  float newTemp = a0_mV * 0.05557 - 17.8;
  newTemp -= 0.1;   // Ice point cal 03/09/2021
  if (newTemp < 60)
  {
    temp = newTemp;
    tempTotal += temp;
    tempCount++;
  }
}

//================================================================
void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  ads1015.begin();  // Initialize ads1015

  // RS485 serial port
  pb100Serial.begin(BAUD_RATE, SWSERIAL_8N1, D7, D8, false, 95, 11);

  // Send config data
  pb100Serial.print("$PAMTC,EN,ALL,0*1D");       // Disable all
  pb100Serial.print("$PAMTC,EN,ZDA,1,10*2F");   // Add GPS time, 1 second update
  pb100Serial.print("$PAMTC,EN,MDA,1,30*3A");   // Add weather composite, 3 second update

  parser.addHandler("WIMDA", handleWIMDA);
  parser.addHandler("GPZDA", handleGPZDA);
  
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  loadSettings();
  loadCachedData();

  pinMode(LED_BUILTIN, OUTPUT);
  if (strlen(userSSID) == 0)
  {
    connectAP();
  }
  else if (!connectSTA(15000))
  {
    connectAP();
  }
  initServerRoutes();

  // Start server
  server.begin();

  //================================================
  // Init NTP and time
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
}

void loop() {

  if (restartFlag != 0)
  {
    restartWithDelay();
  }

  while (pb100Serial.available()) {
    byte b = pb100Serial.read();
    //Serial.print((char)b);
    parser << b;
    //parser << pb100Serial.read();
    yield();
  }

  timeSecond = second();
  if (timeSecond != lastSecond)
  {
    readLM34();

    // Capture time now incase processing takes longer than 1 second
    lastSecond = timeSecond;
    timeYear = year();
    timeMonth = month();
    timeDay = day();
    timeHour = hour();
    timeMinute = minute();
    checkLog();
    digitalWrite(LED_BUILTIN, LOW);
    delayWithYield(20);
    digitalWrite(LED_BUILTIN, HIGH);
    if (apMode)
    {
      delayWithYield(100);
      digitalWrite(LED_BUILTIN, LOW);
      delayWithYield(20);
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

void delayWithYield(int delayTime)
{
  for (int i = 0; i < delayTime / 5; i++)
  {
    delay(5);
  }
}
