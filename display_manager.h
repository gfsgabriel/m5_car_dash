#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

enum ModosTela {
  TELA_HUD_PRINCIPAL,
  TELA_MENU_CONFIG,
  WIFI_TELA_LISTA,
  WIFI_TELA_ACOES,
  WIFI_TELA_SENHA,
  BT_TELA_STATUS,
  BT_TELA_LISTA,
  BT_TELA_SENHA,
  BT_TELA_CONECTANDO,
  BT_TELA_RESULTADO,
  BT_TELA_MSG
};

void inicializarDisplay();
void renderizarDisplay();
void definirModoTela(ModosTela novoModo);
ModosTela obterModoTelaAtual();

void processarTouchHover(int x, int y);
void processarTouchRelease(int x, int y);

void adicionarLogDebug(String linhaLog);

#endif