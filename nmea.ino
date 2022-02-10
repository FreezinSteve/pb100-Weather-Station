//------------------------------------------------------------
// weather data callback
//------------------------------------------------------------
void handleWIMDA(void)
{
  // Filter erroneous values otherwise we can get overflow of fixed width strings which corrupt log data
  float result;
  float newVal;
  static float lastWspd;

// Don't use internal BP sensor
//  if (parser.getArg(2, result))
//  {
//    newVal = result * 1000;
//    // Apply (preliminary) correction BP = 1.0063x - 9.3203
//    newVal = 1.0063 * newVal - 9.3203;    
//    
//    // Correct to MSL
//    //newVal = newVal * pow((1 - ((0.0065 * userElevation) / (temp + 0.0065 * userElevation + 273.15))), -5.257);    
//    if (newVal < 1100 && newVal > 900)
//    {
//      bp = newVal;
//      bpTotal += bp;
//      bpCount++;
//    }
//  }

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
      // Rough cal, maximum recorded value = 85 even though it should have been ~ 100
      rh = rh * 1.163; 
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
    if (newVal >= 0 && newVal < 40)
    {
      // If the new wind speed is < 10 times the last wind speed, then assume it's OK. Otherwise
      // ignore as a false gust. 
      if (lastWspd == 0 || newVal < (lastWspd * 10))
      {
        wspd = newVal;
        wspdTotal += wspd;
        wspdCount++;
        if (wspd > wspdGust)
        {
          wspdGust = wspd;
          wdirGust = wdir;
        }
        lastWspd = wspd;
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
