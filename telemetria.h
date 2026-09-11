#ifndef TELEMETRIA_H
#define TELEMETRIA_H

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
  
  // --- Dados de Configuração do Carro ---
  float motor_litros;     // Ex: 2.0 ou 2.5
  float eficiencia_ve;    // Ex: 0.85 (85% de eficiência volumétrica)
};

extern TelemetriaVeiculo telemetria;

#endif
