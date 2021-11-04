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
#include <ArduinoJson.h>


//#ifdef DEBUG_ESP_PORT
//  #define DBG(_1, ...) DEBUG_ESP_PORT.printf_P(PSTR(_1), ##__VA_ARGS__)
//#else
//  #define DBG(...)
//#endif
#define DEBUG_ON 1
#define DEBUG_OFF 0
byte debugMode = DEBUG_ON;
#define DBG(...) debugMode == DEBUG_ON ? Serial.println(__VA_ARGS__) : NULL


bool apMode = false;

char userSSID[20];
char userPass[20];
char userIP[20] = "192.168.1.100";        //xxx.xxx.xxx.xxx\0
char userGateway[20] = "192.168.1.250";   //xxx.xxx.xxx.xxx\0
char userProxy[20] = "192.168.1.130";     //xxx.xxx.xxx.xxx\0
int userUTCOffset = 720;
int userElevation = 135;

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
int logWraps = 0;

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
    logWraps++;
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
  if (logWraps > 0)
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
  if (logPointer >= BUFF_SIZE)
  {
    logPointer = 0;
    logWraps = 1;
  }
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

  DBG("Starting program");
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
