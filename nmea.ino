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
      float last_wspd = wspd;
      wspd = newVal;
      wspdTotal += wspd;
      wspdCount++;
      // If the current wind speed is < 10 times the last wind speed, then assume it's OK. Otherwise
      // ignore as a false gust. 
      // TODO: false WS will still skew the mean
      if ((last_wspd * 10) < wspd)
      {    
        if (wspd > wspdGust)
        {
          wspdGust = wspd;
          wdirGust = wdir;
        }
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
