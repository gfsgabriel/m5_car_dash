#ifndef DISPLAY_MENU_H
#define DISPLAY_MENU_H

#include <M5Unified.h>

void renderizarMenuConfig(M5Canvas* cv, int hoverIdx);
void renderizarListaWifi(M5Canvas* cv, int numRedes, int selecionadaIdx, int hoverIdx);
void renderizarTecladoSenha(M5Canvas* cv, const char* buffer, int cursor, int hoverKeyId);
void renderizarMenuBluetooth(M5Canvas* cv, int hoverIdx);

#endif
