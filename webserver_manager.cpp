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

// Banco de dados interno de logs gerenciado pela aplicação de rede
#define MAX_LINHAS_WEB_LOG 8
static String linhasLogWeb[MAX_LINHAS_WEB_LOG];
static int totalLogsWeb = 0;

void armazenarLogInterno(const String& linha) {
  if (totalLogsWeb < MAX_LINHAS_WEB_LOG) {
    linhasLogWeb[totalLogsWeb++] = linha;
  } else {
    for (int i = 0; i < MAX_LINHAS_WEB_LOG - 1; i++) {
      linhasLogWeb[i] = linhasLogWeb[i + 1];
    }
    linhasLogWeb[MAX_LINHAS_WEB_LOG - 1] = linha;
  }
}

String obterLinhaLogWeb(int indice) {
  if (indice >= 0 && indice < totalLogsWeb) return linhasLogWeb[indice];
  return "";
}

int obterTotalLogsWeb() {
  return totalLogsWeb;
}

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
  // Rota principal e administrativa do SD
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/index.html", "text/html");
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/admin.html", "text/html");
  });

  // 🌟 NOVA ROTA: Entrega a página de logs direto da raiz do MicroSD!
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/logs.html", "text/html");
  });

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
  logarMensagemApp("SYS: WebServer rodando na porta 80");
}

void gerenciarWebServer() {
  ws.cleanupClients(); 
  
  // 🌟 CONSUMIDOR DE FILA ASSÍNCRONO: Processa as strings enviadas de qualquer arquivo
  String* msgRecebida;
  while (xFilaLogs != NULL && xQueueReceive(xFilaLogs, &msgRecebida, 0) == pdTRUE) {
    // 1. Salva no banco de dados local da RAM
    armazenarLogInterno(*msgRecebida);
    
    // 2. Printa na serial como contingência caso esteja plugado no PC
    Serial.println(*msgRecebida);
    
    // 3. Transmite via WebSocket em formato texto simples prefixado com "LOG:" para o browser capturar
    if (ws.count() > 0) {
      ws.textAll("LOG:" + (*msgRecebida));
    }
    
    // Deleta a string da heap criada pela função helper logarMensagemApp
    delete msgRecebida;
  }
  
  static uint32_t ultimoBroadcast = 0;
  if (millis() - ultimoBroadcast > 50) {
    ultimoBroadcast = millis();
    if (ws.count() > 0) { 
      String payload = gerarSnapshotJSON();
      ws.textAll(payload);
    }
  }
}
