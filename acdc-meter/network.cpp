#include "network.h"
#include "secrets.h"
#include "meter.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

void networkInit(ESP8266WebServer &server, ESP8266HTTPUpdateServer &updater)
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
        delay(300);

    ArduinoOTA.setHostname("acdc-meter");
    ArduinoOTA.begin();

    updater.setup(&server, "/update");

    server.begin();
}

void sendThingSpeak()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    WiFiClient client;
    HTTPClient http;

    String url =
        "http://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_KEY) + "&field1=" + String(A.watts, 1) + "&field2=" + String(totalKwh(A), 3) + "&field3=" + String(A.pulses) + "&field4=" + String(B.watts, 1) + "&field5=" + String(totalKwh(B), 3) + "&field6=" + String(B.pulses);

    http.begin(client, url);
    http.GET();
    http.end();
}
