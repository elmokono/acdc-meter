#include "api.h"
#include "meter.h"
#include "config.h"
#include "html.h"
#include "network.h" // extern server

void handleData()
{
    String json = "{";

    uint32_t pA = A.pulses; // quick read (incremented by ISRs)
    uint32_t pB = B.pulses;

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

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200, "application/json", json);
}

void handleReset()
{
    String uri = server.uri();
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

void setupApi()
{
    server.on("/", HTTP_GET, []()
              { server.send_P(200, "text/html", INDEX_HTML); });
    server.on("/data", handleData);

    server.onNotFound([]()
                      {
    if (server.method() == HTTP_OPTIONS) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
      server.send(204);
    } else {
      if (server.uri().startsWith("/reset/")) {
        handleReset();
      } else {
        server.send(404, "text/plain", "Not found");
      }
    } });
}
