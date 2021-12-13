
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

  // If client not on a local address then return version with no Settings page (blocked in the API as well)
  server.on("/app.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (isLocalRequest(request, false))
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

  server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (isLocalRequest(request, true))
    {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/settings.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
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

  server.on("/download.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/download.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });


  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/favicon.ico.gz", "image/x-icon");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  //======================================================================
  // Load / Save settings
  // Block anything not on a 192.168 address
  server.on("/update-settings", HTTP_POST, [](AsyncWebServerRequest * request) {}, NULL, handleUpdateSettings);

  server.on("/read-settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (isLocalRequest(request, true))
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
  });

  //======================================================================
  // Diagnostics API

  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "Rebooting module, wait for module to connect to WiFi before continuing");
    restartFlag = 1;
  });

  server.on("/heapfree", HTTP_GET, [](AsyncWebServerRequest * request) {
    uint32_t free;
    uint16_t max;
    uint8_t frag;
    ESP.getHeapStats(&free, &max, &frag);
    char mem[40];
    int ptr = 0;
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

  //TODO: Should this be POST? GET lets us use a browser though
  server.on("/erase", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (isLocalRequest(request, true))
    {
      eraseLogFiles();
      request->send_P(200, "text/plain", "OK");
      restartFlag = 1;
    }
  });

  server.on("/pointer", HTTP_GET, [](AsyncWebServerRequest * request) {
    char mem[60];
    int ptr = 0;
    ptr += sprintf(&mem[ptr], "Curr: %d\n", logPointer);
    ptr += sprintf(&mem[ptr], "Size: %d\n", BUFF_SIZE);
    ptr += sprintf(&mem[ptr], "Wrap: %d\n", logWraps);
    ptr += sprintf(&mem[ptr], "Next: %d\n", nextLog);
    request->send_P(200, "text/plain", mem);
  });

  server.on("/resets", HTTP_GET, []
  (AsyncWebServerRequest * request) {
    if (LittleFS.exists("/resets.txt"))
    {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/resets.txt", "text/plain");
      request->send(response);
    }
    else
    {
      request->send(200);
    }
  });

  //======================================================================
  // Data API
  server.on("/sensor", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->hasParam("id"))
    {
      // Send error
      request->send_P(400, "text/plain", "ERROR: No sensor id query ?id=xx&day=y");
      return;
    }
    String id = request->getParam("id")->value();
    int sensor = getSensorIndex(id);
    if (sensor < 0)
    {
      // Send error
      request->send_P(400, "text/plain", "ERROR: Invalid sensor id query ?id=??&day=y");
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
        // File doesn't exist yet, return empty response
        request->send(200);
      }
    }
    else
    {
      request->send_P(400, "text/plain", "ERROR: Invalid sensor day query ?id=xx&day=y");
      return;
    }
  });
}


// Return TRUE if request is from a local address. If false AND handleResponse==TRUE then error 404 is returned
bool isLocalRequest(AsyncWebServerRequest * request, bool handleResponse)
{
  IPAddress source = request->client()->remoteIP();
  //TODO: Check the proxy address in settings
  if (source[0] == 192 && source[1] == 168 && source[2] == 1 && source[3] != 130)
  {
    return true;
  }
  else
  {
    if (handleResponse)
    {
      request->send(404);
    }
    return false;
  }
}

// Request is chunked, store data in temp object until we have the complete response
void handleUpdateSettings(AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (isLocalRequest(request, true))
  {
    if (request->_tempObject == NULL)
    {
      request->_tempObject = new char[total + 1];
    }
    char *tempObject = (char *)request->_tempObject;
    for (int i = 0; i < len; i++)
    {
      tempObject[index + i] = (char)data[i];
    }
    if (index + len >= total)
    {
      tempObject[total] = '\0';
      if (parseSettings(tempObject))
      {
        // Save to LittleFS as JSON
        saveSettings();
        request->send(200);
      }
      else
      {
        request->send(500);
      }
    }
  }
}
