#pragma once

#include <ESP8266WebServer.h>

// Rutas y handlers de API
void apiSetupRoutes(ESP8266WebServer &server);
void handleData(ESP8266WebServer &server);
void handleReset(ESP8266WebServer &server);
