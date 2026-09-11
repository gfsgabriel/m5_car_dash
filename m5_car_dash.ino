#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#include "telemetria.h"
#include "obd2_manager.h"
#include "display_manager.h"
#include "touch_manager.h"

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

  SPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
  if (!SD.begin(SD_SPI_CS, SPI, 25000000)) { 
    M5.Display.fillScreen(RED);
    while (true) delay(1); 
  }

  inicializarOBD2();
  inicializarDisplay();
  inicializarTouch();
}

void loop() {
  // CORREÇÃO CRUCIAL: O M5.update() alimenta as funções do M5.Touch.getCount()
  // Ele precisa rodar obrigatoriamente a cada ciclo do loop principal para atualizar o chip de toque!
  M5.update();

  atualizarTouch();
  atualizarDadosOBD2();
  atualizarInterfaceGrafica();
  delay(16); 
}
