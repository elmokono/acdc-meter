#include "api.h"
#include "meter.h"
#include "html.h"

static void sendCORS(ESP8266WebServer &server)
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleData(ESP8266WebServer &server)
{
    String json = "{";

    uint32_t pA = getPulsesSafe(A);
    uint32_t pB = getPulsesSafe(B);

    unsigned long ts = millis() / 1000;

    json += "\"ts\":" + String(ts) + ",";
    json += "\"A\":{";
    json += "\"w_inst\":" + String(A.watts, 1) + ",";
    json += "\"kwh\":" + String(totalKwh(A), 3) + ",";
    json += "\"alpha\":" + String(A.emaAlpha, 3) + ",";
    json += "\"pulses\":" + String(pA);
    json += "},";

    json += "\"B\":{";
    json += "\"w_inst\":" + String(B.watts, 1) + ",";
    json += "\"kwh\":" + String(totalKwh(B), 3) + ",";
    json += "\"alpha\":" + String(B.emaAlpha, 3) + ",";
    json += "\"pulses\":" + String(pB);
    json += "}";

    json += "}";

    sendCORS(server);
    server.send(200, "application/json", json);
}

void handleReset(ESP8266WebServer &server)
{
    String uri = server.uri(); // /reset/A/12.3
    uri.replace("/reset/", "");
    int s = uri.indexOf('/');
    if (s < 0)
        return server.send(400, "text/plain", "Bad request");

    String id = uri.substring(0, s);
    float val = uri.substring(s + 1).toFloat();

    if (id == "A")
    {
        resetMeter(A, val);
    }
    else if (id == "B")
    {
        resetMeter(B, val);
    }
    else if (id == "PA")
    {
        resetPulses(A, (uint32_t)val);
    }
    else if (id == "PB")
    {
        resetPulses(B, (uint32_t)val);
    }
    else
    {
        return server.send(404, "text/plain", "Meter not found");
    }

    server.send(200, "text/plain", "OK");
}

void apiSetupRoutes(ESP8266WebServer &server)
{
    server.on("/", HTTP_GET, [&server]()
              { server.send_P(200, "text/html", INDEX_HTML); });

    server.on("/data", HTTP_GET, [&server]()
              { handleData(server); });

    server.onNotFound([&server]()
                      {
    if (server.method() == HTTP_OPTIONS) {
      sendCORS(server);
      server.send(204);
    } else {
      if (server.uri().startsWith("/reset/")) {
        handleReset(server);
      } else {
        server.send(404, "text/plain", "Not found");
      }
    } });
}
