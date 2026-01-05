#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

void networkInit(ESP8266WebServer &server, ESP8266HTTPUpdateServer &updater);
void sendThingSpeak();

// helper para exponer objetos si se desea
// (notar que preferimos pasar referencias en lugar de usar globals)
