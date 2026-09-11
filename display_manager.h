#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "telemetria.h"
#include <Arduino.h> // Garante o escopo da classe String

void inicializarDisplay();
void atualizarInterfaceGrafica();
int obterModoTelaAtual();

// 🌟 CORREÇÃO: Passagem por referência constante para o Linker fechar o escopo
void adicionarLogDebug(const String& linhaLog);

#endif
