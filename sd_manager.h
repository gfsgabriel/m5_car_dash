#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// HTML embutido em PROGMEM (não depende do SD)
extern const char SD_HTML[] PROGMEM;

// Registra as rotas /sd e /api/sd/* no servidor
void registrarRotasSD(AsyncWebServer &server);

// Flag global: true enquanto um upload está em andamento.
// O HUD deve PAUSAR o render quando isso for true.
extern volatile bool uploadEmAndamento;

#endif