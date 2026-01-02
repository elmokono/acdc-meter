#include "network.h"
#include "secrets.h"
#include "config.h"
#include "meter.h"
#include "storage.h"

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

static unsigned long lastSend = 0;

void setupNetwork()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
        delay(300);

    ArduinoOTA.setHostname("acdc-meter");
    ArduinoOTA.begin();

    httpUpdater.setup(&server, "/update");

    server.begin();
}

void networkHandle()
{
    ArduinoOTA.handle();
    server.handleClient();

    if (millis() - lastSend >= SEND_INTERVAL_MS)
    {
        lastSend = millis();
        sendThingSpeak();
        saveEEPROM();
    }
}

void sendThingSpeak()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    WiFiClient client;
    HTTPClient http;

    String url = "http://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_KEY) + "&field1=" + String(A.watts, 1) + "&field2=" + String(totalKwh(A), 3) + "&field3=" + String(A.pulses) + "&field4=" + String(B.watts, 1) + "&field5=" + String(totalKwh(B), 3) + "&field6=" + String(B.pulses);

    http.begin(client, url);
    http.GET();
    http.end();
}
