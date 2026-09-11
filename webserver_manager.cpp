#include "webserver_manager.h"
#include "telemetria.h"
#include "obd2_manager.h" 
#include <WiFi.h>
#include <SD.h> 
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>

extern Preferences prefs; 

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static String gerarSnapshotJSON() {
  String json = "{";
  json += "\"rpm\":" + String(telemetria.rpm) + ",";
  json += "\"vel\":" + String(telemetria.velocidade) + ",";
  json += "\"boost\":" + String(telemetria.boost, 2) + ",";
  json += "\"fuel\":" + String(telemetria.consumo_ml_min, 0) + ",";
  json += "\"tps\":" + String(telemetria.tps, 0) + ",";
  json += "\"engine_size\":" + String(telemetria.motor_litros, 1) + ",";
  json += "\"sim_mode\":" + String(telemetria.modoSimulador ? "1" : "0");
  json += "}";
  return json;
}

void inicializarWebServer() {
  // Entrega assíncrona dos arquivos estáticos vindos do SD
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/index.html", "text/html");
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/admin.html", "text/html");
  });

  // REST API: GET Endpoint
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("key")) {
      String key = request->getParam("key")->value();
      if (key == "engine_size") request->send(200, "text/plain", String(telemetria.motor_litros, 1));
      else if (key == "sim_mode") request->send(200, "text/plain", telemetria.modoSimulador ? "1" : "0");
      else request->send(404, "text/plain", "KEY_NOT_FOUND");
    } else {
      request->send(200, "application/json", gerarSnapshotJSON());
    }
  });

  // REST API: SET Endpoint (Processa os Sliders do /admin)
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("key") && request->hasParam("value")) {
      String key = request->getParam("key")->value();
      String value = request->getParam("value")->value();

      if (key == "engine_size") {
        float novoTamanho = value.toFloat();
        salvarAjusteMotorFlash(novoTamanho, telemetria.eficiencia_ve);
      } 
      else if (key == "sim_mode") {
        telemetria.modoSimulador = (value == "1");
      } 
      else if (telemetria.modoSimulador) {
        // Injeta os dados das barras deslizantes direto na RAM da telemetria
        if (key == "rpm") telemetria.rpm = value.toFloat();
        else if (key == "velocidade") telemetria.velocidade = value.toFloat();
        else if (key == "boost") telemetria.boost = value.toFloat();
        else if (key == "tps") telemetria.tps = value.toFloat();
      }
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "MISSING_PARAMS");
    }
  });

  server.addHandler(&ws);
  server.begin();
}

void gerenciarWebServer() {
  ws.cleanupClients(); 
  
  static uint32_t ultimoBroadcast = 0;
  if (millis() - ultimoBroadcast > 50) {
    ultimoBroadcast = millis();
    if (ws.count() > 0) { 
      String payload = gerarSnapshotJSON();
      ws.textAll(payload);
    }
  }
}
