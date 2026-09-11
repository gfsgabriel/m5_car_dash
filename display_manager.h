#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "telemetria.h"
#include <Arduino.h>

void inicializarDisplay();
void atualizarInterfaceGrafica();
int obterModoTelaAtual();

// 🌟 THE CLEAN FIX: Simplified global debugger function signature
void adicionarLogDebug(const String& linhaLog);

#endif
