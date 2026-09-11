#include "obd2_manager.h"
#include <Arduino.h>
#include <Preferences.h>

// 🌟 FIX: Forward declaration so the compiler knows this function belongs to display_manager.cpp
extern void adicionarLogDebug(String linhaLog);

Preferences prefs;
QueueHandle_t xFilaPIDsPrioridade;

static float dirRPM = 150.0; static float dirVel = 1.2; static float dirMAF = 0.8;
static uint32_t tempoInicioZeroCem = 0; static bool cronometroRodando = false;
static uint32_t ultimoTempoMicros = 0; static double acumuladorMililitros = 0.0;
static float valMAF = 4.5;

// TAREFA ASSÍNCRONA: Roda puramente no CORE 0 simulando a coleta de dados
void vTarefaMockOBD2(void *pvParameters) {
  uint32_t pidParaRequisitar;
  uint32_t tempoUltimoLog = 0;

  for (;;) {
    // 1. Processa a fila de prioridades caso o display injete alguma requisição
    if (xFilaPIDsPrioridade != NULL && xQueueReceive(xFilaPIDsPrioridade, &pidParaRequisitar, 0) == pdTRUE) {
      // Aqui simularia o envio físico do PID
    }

    // 2. Cospe logs periódicos no terminal de debug do menu para testar o scroll da tela
    if (millis() - tempoUltimoLog > 1500) {
      tempoUltimoLog = millis();
      static int alternador = 0;
      if (alternador == 0) {
        adicionarLogDebug("MOCK_TX: 010C -> RX: 41 0C 1F A0");
        alternador = 1;
      } else {
        adicionarLogDebug("MOCK_TX: 0110 -> RX: 41 10 0B F4");
        alternador = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Alivia o processador no Core 0
  }
}

void inicializarOBD2() {
  prefs.begin("ajustes", false);
  float motorSalvo = prefs.getFloat("motor", 2.5f);
  float veSalva = prefs.getFloat("ve", 0.85f);

  xFilaPIDsPrioridade = xQueueCreate(15, sizeof(uint32_t));

  telemetria = {0.0, 0.0, -15.0, 0.0, 0.0, 0.0, 0.0, 13.8, 92.0, 35.0, 0.0, 0.0, motorSalvo, veSalva};
  ultimoTempoMicros = micros(); 
  acumuladorMililitros = 0.0;

  // Cria a tarefa em segundo plano estável no CORE 0
  xTaskCreatePinnedToCore(
    vTarefaMockOBD2, 
    "TaskMockOBD2", 
    4096, 
    NULL, 
    1, 
    NULL, 
    0 // Fixo no Core 0
  );
  
  adicionarLogDebug("SYS: Tarefa de Mock OBD2 iniciada no Core 0");
}

void atualizarDadosOBD2() {
  // Roda no loop principal (Core 1) atualizando o motor matemático de simulação
  uint32_t tempoAtualMicros = micros();
  uint32_t deltaMicros = tempoAtualMicros - ultimoTempoMicros;
  ultimoTempoMicros = tempoAtualMicros;
  if (deltaMicros > 500000) deltaMicros = 16000; 

  telemetria.rpm += dirRPM;
  if (telemetria.rpm >= 11500.0 || telemetria.rpm <= 0.0) dirRPM = -dirRPM;

  telemetria.tps = (telemetria.rpm / 11500.0) * 100.0;

  telemetria.velocidade += dirVel;
  if (telemetria.velocidade >= 140.0 || telemetria.velocidade <= 0.0) dirVel = -dirVel;

  valMAF += dirMAF + (dirRPM * 0.02);
  if (valMAF < 4.5) valMAF = 4.5; if (valMAF > 240.0) valMAF = 240.0;
  if (valMAF >= 240.0 || valMAF <= 4.5) dirMAF = -dirMAF;

  double combustivelGramsPerSec = (double)valMAF / 14.7;
  double litrosPerSec = combustivelGramsPerSec / 740.0;
  telemetria.consumo_ml_min = (float)(litrosPerSec * 1000.0 * 60.0);

  if (telemetria.velocidade > 5.0) {
    telemetria.consumo_l_100km = (telemetria.consumo_ml_min * 6.0) / telemetria.velocidade;
  } else {
    telemetria.consumo_l_100km = 99.9;
  }

  acumuladorMililitros += ((double)telemetria.consumo_ml_min / 60000000.0) * deltaMicros;
  telemetria.consumo_total_litros = (float)(acumuladorMililitros / 1000.0);

  // Fórmula de Boost por MAF
  double maf_kg_s = valMAF / 1000.0; 
  double temp_kelvin = telemetria.tempIntake + 273.15;
  double rpm_limite = (telemetria.rpm > 600.0) ? telemetria.rpm : 600.0;
  double deslocamento_m3 = (double)telemetria.motor_litros / 1000.0; 
  double numerador = maf_kg_s * 287.0 * temp_kelvin;
  double denominador = telemetria.eficiencia_ve * deslocamento_m3 * (rpm_limite / 120.0);
  double pressao_pascal = (numerador / denominador) - 101325.0;
  telemetria.boost = (float)(pressao_pascal * 0.000145038);
  if (telemetria.boost > telemetria.boost_max) telemetria.boost_max = telemetria.boost;

  // Lógica de teste de arrancada do 0-100 km/h
  if (telemetria.velocidade <= 0.1) { 
    cronometroRodando = false; 
  } 
  else if (telemetria.velocidade > 0.5 && !cronometroRodando && telemetria.velocidade < 100.0) {
    tempoInicioZeroCem = millis(); 
    cronometroRodando = true;
  } 
  else if (cronometroRodando && telemetria.velocidade >= 100.0) {
    telemetria.zeroCemUltimo = (float)(millis() - tempoInicioZeroCem) / 1000.0; 
    cronometroRodando = false; 
  }
}

void salvarAjusteMotorFlash(float novoTamanhoLitros, float novaEficienciaVE) {
  telemetria.motor_litros = novoTamanhoLitros;
  telemetria.eficiencia_ve = novaEficienciaVE;
  prefs.putFloat("motor", novoTamanhoLitros);
  prefs.putFloat("ve", novaEficienciaVE);
}
