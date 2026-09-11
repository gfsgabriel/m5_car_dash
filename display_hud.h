#ifndef DISPLAY_HUD_H
#define DISPLAY_HUD_H

#include <M5Unified.h>

void inicializarHUD();
void renderizarHUDPrincipal(M5Canvas &canvasVirtual, M5Canvas &sprTurbo, M5Canvas &sprFuel, M5Canvas &sprPonteiro);

#endif