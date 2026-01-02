#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

extern ESP8266WebServer server;
extern ESP8266HTTPUpdateServer httpUpdater;

void setupNetwork();
void networkHandle();
void sendThingSpeak();
