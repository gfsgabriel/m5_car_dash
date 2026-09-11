#ifndef OBD2_MANAGER_H
#define OBD2_MANAGER_H

#include "telemetria.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Inicializa o módulo OBD2 (Mock assíncrono)
void inicializarOBD2();

// Função executada no loop principal
void atualizarDadosOBD2();

// Salva ajustes na Flash NVS
void salvarAjusteMotorFlash(float novoTamanhoLitros, float novaEficienciaVE);

// Fila assíncrona pública para tráfego de PIDs
extern QueueHandle_t xFilaPIDsPrioridade;

#endif
