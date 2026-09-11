#ifndef TELEMETRIA_H
#define TELEMETRIA_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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
  float zeroCemUltimo;
  float tps;              
  float motor_litros;     
  float eficiencia_ve;    
  bool modoSimulador;     
};

extern TelemetriaVeiculo telemetria;

// 🌟 NOVA FILA GLOBAL DE LOGS: Qualquer módulo pode postar um ponteiro de string aqui!
extern QueueHandle_t xFilaLogs;

// Função macro global rápida para postar mensagens na fila de qualquer lugar do app
void logarMensagemApp(const String& msg);

#endif
