#ifndef DISPLAY_MENU_H
#define DISPLAY_MENU_H

#include <M5Unified.h>

void atualizarHoverMenu(int x, int y);
void processarReleaseMenu(int x, int y);

void renderizarMenuConfig(M5Canvas &canvasVirtual);
void renderizarWifiLista(M5Canvas &canvasVirtual);
void renderizarWifiAcoes(M5Canvas &canvasVirtual);   // NOVO
void renderizarWifiSenha(M5Canvas &canvasVirtual);

#endif