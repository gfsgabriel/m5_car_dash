#include "spi_lock.h"

SemaphoreHandle_t xSpiSDMutex = NULL;

void inicializarSpiSDLock() {
  if (xSpiSDMutex == NULL) {
    xSpiSDMutex = xSemaphoreCreateMutex();
    Serial.println("SPI Lock: mutex criado");
  }
}