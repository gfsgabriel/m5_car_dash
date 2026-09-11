#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "telemetria.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Máquina de Estados Global de Telas do Projeto (CONSERTADO)
enum ModosTela { 
  TELA_HUD_PRINCIPAL, 
  TELA_MENU_CONFIG,
  WIFI_TELA_SCAN,      // 🌟 ADICIONADO! Resolve o erro do compilador
  WIFI_TELA_LISTA,
  WIFI_TELA_SENHA,
  BT_TELA_MENU
};

void inicializarDisplay();
void atualizarInterfaceGrafica();
int obterModoTelaAtual();
void adicionarLogDebug(const String& linhaLog);

extern int globalHoverIdx;           
extern int globalTecladoHoverKeyId;  
extern QueueHandle_t xFilaTouch;

#endif
