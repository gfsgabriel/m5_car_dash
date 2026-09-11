#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "telemetria.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Máquina de Estados Global de Telas do Projeto
enum ModosTela { 
  TELA_HUD_PRINCIPAL, 
  TELA_MENU_CONFIG,
  WIFI_TELA_LISTA,
  WIFI_TELA_SENHA,
  BT_TELA_MENU
};

// Declaração das funções de ciclo de vida do display
void inicializarDisplay();
void atualizarInterfaceGrafica();
int obterModoTelaAtual();

// Função leve de log para debugar eventos (Direciona para Serial por enquanto)
void adicionarLogDebug(const String& linhaLog);

// Variáveis Globais de Toque Compartilhadas para o efeito Hover/Highlight
extern int globalHoverIdx;           // Índice da linha do menu sob o dedo
extern int globalTecladoHoverKeyId;  // ID da tecla virtual sob o dedo (0=[<], 1=[>], 2=[CONECTAR])

// Fila global do FreeRTOS para execução reativa de comandos de toque
extern QueueHandle_t xFilaTouch;

#endif
