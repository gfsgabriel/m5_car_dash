#include "webserver_manager.h"
#include "telemetria.h"
#include "obd2_manager.h"
#include "spi_lock.h"
#include "sd_manager.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <Preferences.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static QueueHandle_t xFilaLogsWebServer = NULL;
static unsigned long ultimoEnvioWS = 0;
const unsigned long INTERVALO_WS_MS = 50; // 20 Hz

#define MAX_LOGS_BUFFER 20
static String bufferLogs[MAX_LOGS_BUFFER];
static int idxBufferLogs = 0;
static int totalBufferLogs = 0;

static Preferences prefsAdmin;

// ---------------------------------------------------------------
// Cache de HTMLs em PSRAM
// ---------------------------------------------------------------
static char* htmlIndex = nullptr;
static char* htmlAdmin = nullptr;
static char* htmlLogs  = nullptr;
static size_t tamIndex = 0, tamAdmin = 0, tamLogs = 0;

static char* carregarArquivoPSRAM(const char* path, size_t* outTam) {
  *outTam = 0;

  // No seu hardware, o SD tá montado na raiz "/"
  if (!SD.exists(path)) {
    Serial.printf("HTML: NAO ENCONTRADO: %s\n", path);
    return nullptr;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.printf("HTML: falha ao abrir %s\n", path);
    return nullptr;
  }

  size_t tam = f.size();
  if (tam == 0) {
    Serial.printf("HTML: %s tem 0 bytes\n", path);
    f.close();
    return nullptr;
  }

  char* buf = (char*)ps_malloc(tam + 1);
  if (!buf) {
    Serial.printf("HTML: sem PSRAM pra %s (%u bytes)\n", path, (unsigned)tam);
    f.close();
    return nullptr;
  }

  size_t lidos = f.readBytes(buf, tam);
  buf[lidos] = '\0';
  f.close();

  Serial.printf("HTML: %s carregado (%u bytes)\n", path, (unsigned)lidos);
  *outTam = lidos;
  return buf;
}

void carregarHtmlsParaRam() {
  Serial.println("=== Carregando HTMLs pra PSRAM ===");
  htmlIndex = carregarArquivoPSRAM("/index.html", &tamIndex);
  htmlAdmin = carregarArquivoPSRAM("/admin.html", &tamAdmin);
  htmlLogs  = carregarArquivoPSRAM("/logs.html",  &tamLogs);
  Serial.println("===================================");
}

// ---------------------------------------------------------------
// API pública de log
// ---------------------------------------------------------------
void registrarLogWebServer(String linhaLog) {
  if (xFilaLogsWebServer == NULL) return;

  String* msg = new String(linhaLog);
  if (xQueueSend(xFilaLogsWebServer, &msg, 0) != pdTRUE) {
    delete msg;
  }
}

// ---------------------------------------------------------------
// Helpers internos
// ---------------------------------------------------------------
static void enviarLogParaTodos(const String& linha) {
  if (ws.count() == 0) return;

  StaticJsonDocument<320> doc;
  doc["tipo"] = "log";
  doc["linha"] = linha;

  String buf;
  serializeJson(doc, buf);
  ws.textAll(buf);
}

static void guardarLogNoBuffer(const String& linha) {
  bufferLogs[idxBufferLogs] = linha;
  idxBufferLogs = (idxBufferLogs + 1) % MAX_LOGS_BUFFER;
  if (totalBufferLogs < MAX_LOGS_BUFFER) totalBufferLogs++;
}

// ---------------------------------------------------------------
// WebSocket events
// ---------------------------------------------------------------
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS: cliente #%u conectado\n", client->id());

    int inicio = (totalBufferLogs < MAX_LOGS_BUFFER) ? 0 : idxBufferLogs;
    for (int i = 0; i < totalBufferLogs; i++) {
      int idx = (inicio + i) % MAX_LOGS_BUFFER;
      StaticJsonDocument<320> doc;
      doc["tipo"] = "log";
      doc["linha"] = bufferLogs[idx];
      String buf;
      serializeJson(doc, buf);
      client->text(buf);
    }
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS: cliente #%u desconectou\n", client->id());
  }
}

// ---------------------------------------------------------------
// Inicialização
// ---------------------------------------------------------------
void inicializarWebServer() {
  xFilaLogsWebServer = xQueueCreate(30, sizeof(String*));

  prefsAdmin.begin("ajustes", false);

  WiFi.softAP("Volvo_C30_T5_Telemetry", "12345678");

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // =============================================================
  // IMPORTANTE: rotas ESPECÍFICAS primeiro, genéricas DEPOIS
  // =============================================================

  // -------- API: admin --------
  server.on("/api/admin/get", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["motor"] = telemetria.motor_litros;
    doc["ve"] = telemetria.eficiencia_ve;
    doc["sim_mode"] = telemetria.modoSimulador;

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    request->send(200, "application/json", jsonBuffer);
  });

  server.on("/api/admin/set", HTTP_GET, [](AsyncWebServerRequest *request){
    float novoMotor = telemetria.motor_litros;
    float novaVE = telemetria.eficiencia_ve;

    if (request->hasParam("motor")) novoMotor = request->getParam("motor")->value().toFloat();
    if (request->hasParam("ve"))    novaVE    = request->getParam("ve")->value().toFloat();

    salvarAjusteMotorFlash(novoMotor, novaVE);

    StaticJsonDocument<128> doc;
    doc["ok"] = true;
    doc["motor"] = telemetria.motor_litros;
    doc["ve"] = telemetria.eficiencia_ve;
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    request->send(200, "application/json", jsonBuffer);
  });

  server.on("/api/admin/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("on")) {
      int v = request->getParam("on")->value().toInt();
      telemetria.modoSimulador = (v != 0);
    }
    StaticJsonDocument<64> doc;
    doc["sim_mode"] = telemetria.modoSimulador;
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    request->send(200, "application/json", jsonBuffer);
  });

  // GET /api/admin/inject?rpm=3000&vel=80&maf=15&boost=10&tps=45&temp_coolant=90&temp_intake=35&bateria=13.8
  server.on("/api/admin/inject", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!telemetria.modoSimulador) {
      request->send(409, "application/json", "{\"erro\":\"modo simulador desligado\"}");
      return;
    }

    // ---- Campos diretos ----
    if (request->hasParam("rpm"))           telemetria.rpm         = request->getParam("rpm")->value().toFloat();
    if (request->hasParam("vel"))           telemetria.velocidade  = request->getParam("vel")->value().toFloat();
    if (request->hasParam("boost"))         telemetria.boost       = request->getParam("boost")->value().toFloat();
    if (request->hasParam("tps"))           telemetria.tps         = request->getParam("tps")->value().toFloat();
    if (request->hasParam("temp_coolant"))  telemetria.tempCoolant = request->getParam("temp_coolant")->value().toFloat();
    if (request->hasParam("temp_intake"))   telemetria.tempIntake  = request->getParam("temp_intake")->value().toFloat();
    if (request->hasParam("bateria"))       telemetria.bateria     = request->getParam("bateria")->value().toFloat();

    // ---- MAF → consumo instantâneo (mesma fórmula do obd2_manager) ----
    if (request->hasParam("maf")) {
      float maf = request->getParam("maf")->value().toFloat();
      double combustivelGramsPerSec = (double)maf / 14.7;   // gasolina ~14.7:1
      double litrosPerSec = combustivelGramsPerSec / 740.0; // densidade ~740 g/L
      telemetria.consumo_ml_min = (float)(litrosPerSec * 1000.0 * 60.0);
    }

    // ---- Derivados ----
    // Consumo L/100km
    if (telemetria.velocidade > 5.0) {
      telemetria.consumo_l_100km = (telemetria.consumo_ml_min * 6.0) / telemetria.velocidade;
    } else {
      telemetria.consumo_l_100km = 99.9;
    }

    // Boost_max (histórico)
    if (telemetria.boost > telemetria.boost_max) {
      telemetria.boost_max = telemetria.boost;
    }

    // Cronômetro 0-100
    static uint32_t tempoInicioZeroCem = 0;
    static bool cronometroRodando = false;
    if (telemetria.velocidade <= 0.1) {
      cronometroRodando = false;
    }
    else if (telemetria.velocidade > 0.5 && !cronometroRodando && telemetria.velocidade < 100.0) {
      tempoInicioZeroCem = millis();
      cronometroRodando = true;
    }
    else if (cronometroRodando && telemetria.velocidade >= 100.0) {
      telemetria.zeroCemUltimo = (float)(millis() - tempoInicioZeroCem) / 1000.0;
      cronometroRodando = false;
    }

    // ---- Resposta ----
    StaticJsonDocument<384> doc;
    doc["ok"] = true;
    doc["rpm"] = telemetria.rpm;
    doc["velocidade"] = telemetria.velocidade;
    doc["boost"] = telemetria.boost;
    doc["tps"] = telemetria.tps;
    doc["maf"] = request->hasParam("maf") ? request->getParam("maf")->value().toFloat() : 0.0f;
    doc["consumo_ml"] = telemetria.consumo_ml_min;
    doc["consumo_l100"] = telemetria.consumo_l_100km;
    doc["temp_coolant"] = telemetria.tempCoolant;
    doc["temp_intake"] = telemetria.tempIntake;
    doc["bateria"] = telemetria.bateria;
    doc["boost_max"] = telemetria.boost_max;
    doc["zero_cem"] = telemetria.zeroCemUltimo;
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    request->send(200, "application/json", jsonBuffer);
  });

  // -------- API: telemetria --------
  server.on("/api/telemetria", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<512> doc;
    doc["rpm"] = telemetria.rpm;
    doc["velocidade"] = telemetria.velocidade;
    doc["boost"] = telemetria.boost;
    doc["boost_max"] = telemetria.boost_max;
    doc["consumo_ml"] = telemetria.consumo_ml_min;
    doc["consumo_l100"] = telemetria.consumo_l_100km;
    doc["consumo_total"] = telemetria.consumo_total_litros;
    doc["bateria"] = telemetria.bateria;
    doc["temp_coolant"] = telemetria.tempCoolant;
    doc["temp_intake"] = telemetria.tempIntake;
    doc["tps"] = telemetria.tps;
    doc["zero_cem"] = telemetria.zeroCemUltimo;
    doc["motor_litros"] = telemetria.motor_litros;
    doc["eficiencia_ve"] = telemetria.eficiencia_ve;
    doc["modo_simulador"] = telemetria.modoSimulador;

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    request->send(200, "application/json", jsonBuffer);
  });

  // Compatibilidade: /get antigo
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<512> doc;
    doc["rpm"] = telemetria.rpm;
    doc["velocidade"] = telemetria.velocidade;
    doc["boost"] = telemetria.boost;
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    request->send(200, "application/json", jsonBuffer);
  });


  // -------- Páginas estáticas (POR ÚLTIMO) --------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (htmlIndex) request->send(200, "text/html", htmlIndex);
    else request->send(500, "text/plain", "index.html nao carregado");
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    if (htmlAdmin) request->send(200, "text/html", htmlAdmin);
    else request->send(404, "text/plain", "admin.html nao carregado");
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    if (htmlLogs) request->send(200, "text/html", htmlLogs);
    else request->send(404, "text/plain", "logs.html nao carregado");
  });

  // -------- API: SD --------
  registrarRotasSD(server);      // ← NOVO

  server.begin();
  Serial.println("WebServer: iniciado (HTMLs em PSRAM, API em /api/*)");
}

// ---------------------------------------------------------------
// Loop
// ---------------------------------------------------------------
void atualizarWebSocket() {
  String* msgPtr;
  while (xQueueReceive(xFilaLogsWebServer, &msgPtr, 0) == pdTRUE) {
    if (msgPtr != NULL) {
      guardarLogNoBuffer(*msgPtr);
      enviarLogParaTodos(*msgPtr);
      delete msgPtr;
    }
  }

  if (ws.count() == 0) return;

  unsigned long agora = millis();
  if (agora - ultimoEnvioWS >= INTERVALO_WS_MS) {
    ultimoEnvioWS = agora;

    StaticJsonDocument<512> doc;
    doc["tipo"] = "telemetria";
    doc["rpm"] = telemetria.rpm;
    doc["velocidade"] = telemetria.velocidade;
    doc["boost"] = telemetria.boost;
    doc["boost_max"] = telemetria.boost_max;
    doc["consumo_ml"] = telemetria.consumo_ml_min;
    doc["consumo_l100"] = telemetria.consumo_l_100km;
    doc["consumo_total"] = telemetria.consumo_total_litros;
    doc["bateria"] = telemetria.bateria;
    doc["temp_coolant"] = telemetria.tempCoolant;
    doc["temp_intake"] = telemetria.tempIntake;
    doc["tps"] = telemetria.tps;
    doc["zero_cem"] = telemetria.zeroCemUltimo;
    doc["motor_litros"] = telemetria.motor_litros;
    doc["eficiencia_ve"] = telemetria.eficiencia_ve;
    doc["modo_simulador"] = telemetria.modoSimulador;

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    ws.textAll(jsonBuffer);
  }

  ws.cleanupClients();
}