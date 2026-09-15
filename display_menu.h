#ifndef DISPLAY_MENU_H
#define DISPLAY_MENU_H

#include <M5Unified.h>

void atualizarHoverMenu(int x, int y);
void processarReleaseMenu(int x, int y);

void renderizarMenuConfig(M5Canvas &canvasVirtual);
void renderizarWifiLista(M5Canvas &canvasVirtual);
void renderizarWifiAcoes(M5Canvas &canvasVirtual);
void renderizarWifiSenha(M5Canvas &canvasVirtual);

// BT
void renderizarBTStatus(M5Canvas &canvasVirtual);
void renderizarBTLista(M5Canvas &canvasVirtual);
void renderizarBTSenha(M5Canvas &canvasVirtual);
void renderizarBTConectando(M5Canvas &canvasVirtual);
void renderizarBTResultado(M5Canvas &canvasVirtual);
void renderizarBTMsg(M5Canvas &canvasVirtual);

#endif