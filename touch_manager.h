#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

// Inicializa variáveis de controle de toque
void inicializarTouch();

// Processa interações físicas na tela e atualiza hovers/filas
void atualizarTouch();

#endif