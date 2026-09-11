#ifndef OBD2_MANAGER_H
#define OBD2_MANAGER_H

#include "telemetria.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define MAX_LINHAS_DEBUG 8

void inicializarOBD2();
void atualizarDadosOBD2();
void salvarAjusteMotorFlash(float novoTamanhoLitros, float novaEficienciaVE);

// Funções globais de Log gerenciadas pelo módulo OBD2
void adicionarLogDebug(const String& linhaLog);
String obterLinhaLog(int indice);
int obterTotalLogs();

extern QueueHandle_t xFilaPIDsPrioridade;

#endif
