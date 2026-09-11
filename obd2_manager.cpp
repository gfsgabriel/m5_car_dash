#include "obd2_manager.h"
#include <Arduino.h>
#include <Preferences.h>

Preferences prefs;
QueueHandle_t xFilaPIDsPrioridade;

static float dirRPM = 150.0; static float dirVel = 1.2; static float dirMAF = 0.8;
static uint32_t tempoInicioZeroCem = 0; static bool cronometroRodando = false;
static uint32_t ultimoTempoMicros = 0; static double acumuladorMililitros = 0.0;
static float valMAF = 4.5;

static String pidsHIGH[5]   = {"010C", "0110", "011C", "012C", "013C"}; static int totalHIGH = 5;
static String pidsMEDIUM[2] = {"010D", "0111"};                         static int totalMEDIUM = 2;
static String pidsLOW[2]    = {"010F", "0105"};                         static int totalLOW = 2;

static int idxHIGH = 0; static int idxMEDIUM = 0; static int idxLOW = 0;
static int passosHighDados = 0; static int passosMediumDados = 0;

void vTarefaMockOBD2(void *pvParameters) {
  uint32_t tempoUltimoEnvioObd = 0;

  for (;;) {
    if (millis() - tempoUltimoEnvioObd > 35) {
      tempoUltimoEnvioObd = millis();
      String pidEscolhido = "";

      if (passosHighDados < totalHIGH) {
        if (totalHIGH > 0) { pidEscolhido = pidsHIGH[idxHIGH]; idxHIGH = (idxHIGH + 1) % totalHIGH; passosHighDados++; }
      } else {
        if (passosMediumDados < totalMEDIUM) {
          if (totalMEDIUM > 0) { pidEscolhido = pidsMEDIUM[idxMEDIUM]; idxMEDIUM = (idxMEDIUM + 1) % totalMEDIUM; passosMediumDados++; }
          passosHighDados = 0;
        } else {
          if (totalLOW > 0) { pidEscolhido = pidsLOW[idxLOW]; idxLOW = (idxLOW + 1) % totalLOW; }
          passosHighDados = 0; passosMediumDados = 0;
        }
      }

      if (pidEscolhido.length() > 0) {
        // Envia apenas para o Monitor Serial do PC enquanto não ativamos a fila do WebSocket de logs
        Serial.println("TX: " + pidEscolhido + " -> RX: [OK]");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void inicializarOBD2() {
  prefs.begin("ajustes", false);
  float motorSalvo = prefs.getFloat("motor", 2.5f);
  float veSalva = prefs.getFloat("ve", 0.85f);

  xFilaPIDsPrioridade = xQueueCreate(15, sizeof(uint32_t));
  telemetria = {0.0, 0.0, -15.0, 0.0, 0.0, 0.0, 0.0, 13.8, 92.0, 35.0, 0.0, 0.0, motorSalvo, veSalva, false};
  ultimoTempoMicros = micros(); 
  acumuladorMililitros = 0.0;

  xTaskCreatePinnedToCore(vTarefaMockOBD2, "TaskMockOBD2", 4096, NULL, 1, NULL, 0);
  Serial.println("SYS: Cascata H-M-L Dinamica Ativa");
}

void atualizarDadosOBD2() {
  if (telemetria.modoSimulador) {
    uint32_t tempoAtualMicros = micros();
    ultimoTempoMicros = tempoAtualMicros;
    return; 
  }

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

  double maf_kg_s = valMAF / 1000.0; 
  double temp_kelvin = telemetria.tempIntake + 273.15;
  double rpm_limite = (telemetria.rpm > 600.0) ? telemetria.rpm : 600.0;
  double deslocamento_m3 = (double)telemetria.motor_litros / 1000.0; 
  double numerador = maf_kg_s * 287.0 * temp_kelvin;
  double denominador = telemetria.eficiencia_ve * deslocamento_m3 * (rpm_limite / 120.0);
  double pressao_pascal = (numerador / denominador) - 101325.0;
  telemetria.boost = (float)(pressao_pascal * 0.000145038);
  if (telemetria.boost > telemetria.boost_max) telemetria.boost_max = telemetria.boost;

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
