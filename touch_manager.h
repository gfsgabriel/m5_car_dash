#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>

// Initializes the tracking flags
void inicializarTouch();

// Tracks live coordinate bounding boxes (Hover) and pushes to queue on release
void atualizarTouch();

#endif
