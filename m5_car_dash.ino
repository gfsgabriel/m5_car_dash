#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#include "telemetria.h"
#include "obd2_manager.h"
#include "display_manager.h"
#include "touch_manager.h"
#include "webserver_manager.h"
#include "wifi_manager.h"
#include "spi_lock.h"
#include "sd_manager.h"        // ← NOVO (pra acessar uploadEmAndamento)

#define SD_SPI_SCK  34
#define SD_SPI_MISO 35
#define SD_SPI_MOSI 33
#define SD_SPI_CS   4

TelemetriaVeiculo telemetria;
QueueHandle_t xFilaTouch;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.fillScreen(BLACK);

  xFilaTouch = xQueueCreate(10, sizeof(int));

  inicializarSpiSDLock();

  // SD
  SPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
  if (!SD.begin(SD_SPI_CS, SPI, 25000000)) {
    M5.Display.fillScreen(RED);
    while (true) delay(1);
  }

  Serial.println("=== DIAGNOSTICO SD ===");
  Serial.printf("index.html: %d\n", SD.exists("/index.html"));
  Serial.printf("admin.html: %d\n", SD.exists("/admin.html"));
  Serial.printf("logs.html:  %d\n", SD.exists("/logs.html"));
  Serial.printf("turbo.png:  %d\n", SD.exists("/sd/turbo.png"));
  Serial.println("======================");

  carregarHtmlsParaRam();
  inicializarDisplay();
  inicializarOBD2();
  inicializarTouch();
  inicializarWifiManager();
  inicializarWebServer();
}

void loop() {
  M5.update();

  atualizarTouch();

  int ev;
  if (xQueueReceive(xFilaTouch, &ev, 0) == pdTRUE) {
    switch (ev) {
      case 1:  definirModoTela(TELA_MENU_CONFIG); break;
      case 2:  definirModoTela(TELA_HUD_PRINCIPAL); break;
      case 5:  definirModoTela(TELA_MENU_CONFIG); break;
      case 10: definirModoTela(WIFI_TELA_LISTA); break;
      case 11: definirModoTela(BT_TELA_MENU); break;
      case 12: definirModoTela(WIFI_TELA_SENHA); break;
      case 13: definirModoTela(WIFI_TELA_ACOES); break;
      default: break;
    }
  }

  atualizarDadosOBD2();
  atualizarWebSocket();

  // PAUSA o render do HUD durante upload (evita conflito SPI)
  if (!uploadEmAndamento) {
    renderizarDisplay();
  }

  delay(16);
}