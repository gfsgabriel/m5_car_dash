#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#include "telemetria.h"
#include "obd2_manager.h"
#include "display_manager.h"
#include "touch_manager.h"
#include "webserver_manager.h"

#define SD_SPI_SCK  34
#define SD_SPI_MISO 35
#define SD_SPI_MOSI 33
#define SD_SPI_CS   4

TelemetriaVeiculo telemetria;
QueueHandle_t xFilaTouch;
QueueHandle_t xFilaLogs; // 🌟 INSTANCIAÇÃO FÍSICA DA FILA DE LOGS

// Função global helper: Aloca a string dinamicamente e posta o ponteiro dela na fila
void logarMensagemApp(const String& msg) {
  if (xFilaLogs == NULL) return;
  String* msgAlocada = new String(msg);
  if (xQueueSend(xFilaLogs, &msgAlocada, 0) != pdTRUE) {
    delete msgAlocada; // Evita memory leak se a fila encher
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  M5.Display.setRotation(1);
  M5.Display.fillScreen(BLACK);

  // Inicializa as filas do FreeRTOS
  xFilaTouch = xQueueCreate(10, sizeof(int));
  xFilaLogs = xQueueCreate(20, sizeof(String*)); // Fila guarda ponteiros de strings

  SPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
  if (!SD.begin(SD_SPI_CS, SPI, 25000000)) { while (true) delay(1); }

  inicializarOBD2();
  inicializarDisplay();
  inicializarTouch();
  inicializarWebServer(); 
}

void loop() {
  M5.update();
  atualizarTouch();
  atualizarDadosOBD2();
  atualizarInterfaceGrafica();
  gerenciarWebServer(); 
  delay(16); 
}
