#ifndef SPI_LOCK_H
#define SPI_LOCK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Mutex global que serializa acesso ao SPI do SD entre tasks.
// Use SEMPRE que acessar o SD em runtime (fora do setup()).
extern SemaphoreHandle_t xSpiSDMutex;

inline void spiSDLock()   { if (xSpiSDMutex) xSemaphoreTake(xSpiSDMutex, portMAX_DELAY); }
inline void spiSDUnlock() { if (xSpiSDMutex) xSemaphoreGive(xSpiSDMutex); }

// Inicializa o mutex (chamar 1x no setup, antes de qualquer uso)
void inicializarSpiSDLock();

#endif