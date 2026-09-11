#ifndef TELEMETRIA_H
#define TELEMETRIA_H

#include <Arduino.h>

struct TelemetriaVeiculo {
  float rpm;
  float velocidade;
  float boost;
  float boost_max;
  float consumo_ml_min;
  float consumo_l_100km;
  float consumo_total_litros;
  float bateria;
  float tempCoolant;
  float tempIntake;
  float tps;
  float zeroCemUltimo;
  float motor_litros;
  float eficiencia_ve;
  bool modoSimulador;
};

extern TelemetriaVeiculo telemetria;

#endif