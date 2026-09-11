#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

// Inicializa a fila de toque e prepara o hardware
void inicializarTouch();

// Varre o chip de toque de forma ultra rápida e injeta na fila do FreeRTOS
void atualizarTouch();

#endif
