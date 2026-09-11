#include "display_manager.h"
#include "obd2_manager.h"  // Incluído para ler as linhas de log
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#define CINZA_ESCURO 0x39E7
#define CINZA_CLARO 0xC618
#define COR_MENU_BG 0x10A2

enum ModosTela { TELA_HUD_PRINCIPAL,
                 TELA_MENU_CONFIG,
                 TELA_SUB_DEBUG };
static ModosTela modoAtual = TELA_HUD_PRINCIPAL;

extern QueueHandle_t xFilaTouch;

static M5Canvas canvasVirtual(&M5.Display);
static M5Canvas sprFundoTurboReduzido(&M5.Display);
static M5Canvas sprFundoFuelReduzido(&M5.Display);
static M5Canvas sprPonteiroOriginal(&M5.Display);

static float ANGULO_MIN = -60.0;
static float ANGULO_MAX = 60.0;
static const int pivotX_G = 160;
static const int pivotY_G = 180;
static const int pivotX_R = 80;
static const int pivotY_R = 90;

int barraLarguraBloco = 4;
int barraLarguraEspaco = 1;
int alturaBarraRPM = 32;
int inicioCorteY = 18;
int corteVelocidadeX = 110;
int corteVelocidadeLarg = 100;
int corteRpmDigitalX = 230;
int corteRpmDigitalLarg = 85;

extern void salvarAjusteMotorFlash(float novoTamanhoLitros, float novaEficienciaVE);

float processarAngulo(float valor, float vMin, float vMax) {
  if (valor < vMin) valor = vMin;
  if (valor > vMax) valor = vMax;
  return (valor - vMin) * (ANGULO_MAX - ANGULO_MIN) / (vMax - vMin) + ANGULO_MIN;
}

int obterModoTelaAtual() {
  return (int)modoAtual;
}

static void desenharIconeBateria(int x, int y) {
  canvasVirtual.drawRect(x, y + 2, 14, 9, WHITE);
  canvasVirtual.fillRect(x + 2, y, 3, 2, WHITE);
  canvasVirtual.fillRect(x + 9, y, 3, 2, WHITE);
  canvasVirtual.drawFastHLine(x + 2, y + 6, 3, WHITE);
  canvasVirtual.drawRect(x + 9, y + 5, 3, 3, WHITE);
}
static void desenharIconeCoolant(int x, int y) {
  canvasVirtual.drawRect(x + 5, y, 4, 10, WHITE);
  canvasVirtual.fillCircle(x + 7, y + 10, 4, WHITE);
  canvasVirtual.drawFastHLine(x, y + 13, 14, CYAN);
  canvasVirtual.drawFastHLine(x + 2, y + 15, 10, CYAN);
}
static void desenharIconeIntake(int x, int y) {
  canvasVirtual.drawRect(x, y + 2, 14, 8, WHITE);
  canvasVirtual.fillRect(x + 5, y, 4, 12, BLUE);
}
static void desenharIconeTPS(int centroX, int centroY, float tpsPercent) {
  int raioBola = 6;
  int alturaTracoMax = 24;
  canvasVirtual.drawCircle(centroX, centroY, raioBola, WHITE);
  float anguloGraus = 90.0 - (tpsPercent * 90.0 / 100.0);
  float anguloRad = anguloGraus * DEG_TO_RAD;
  int x1 = centroX + (cos(anguloRad) * (alturaTracoMax / 2));
  int y1 = centroY - (sin(anguloRad) * (alturaTracoMax / 2));
  int x2 = centroX - (cos(anguloRad) * (alturaTracoMax / 2));
  int y2 = centroY + (sin(anguloRad) * (alturaTracoMax / 2));
  canvasVirtual.drawLine(x1, y1, x2, y2, ORANGE);
  canvasVirtual.setFont(&fonts::Font0);
  canvasVirtual.setTextSize(1.0);
  canvasVirtual.setTextColor(CINZA_CLARO);
  canvasVirtual.setTextDatum(textdatum_t::top_center);
  canvasVirtual.drawString(String((int)tpsPercent) + "% tps", centroX, centroY + 14);
  canvasVirtual.setTextDatum(textdatum_t::top_left);
}

static void desenharBarraRPMAnaDigi(float rpmAtual) {
  float rpmMin = 0.0;
  float redlineStart = 6500.0;
  float rpmMax = 8000.0;
  int cicloTotal = barraLarguraBloco + barraLarguraEspaco;
  int limitePixels = (rpmAtual - rpmMin) * 320.0 / (rpmMax - rpmMin);
  if (limitePixels < 0) limitePixels = 0;
  if (limitePixels > 320) limitePixels = 320;

  for (int x = 0; x < 320; x++) {
    if ((x % cicloTotal) >= barraLarguraBloco) continue;
    if (x <= limitePixels) {
      float rpmNestePixel = rpmMin + (x * (rpmMax - rpmMin) / 320.0);
      canvasVirtual.drawFastVLine(x, 0, alturaBarraRPM, (rpmNestePixel < redlineStart) ? GREEN : RED);
    } else {
      canvasVirtual.drawFastVLine(x, 0, alturaBarraRPM, CINZA_ESCURO);
    }
  }
  int alturaDoCorte = alturaBarraRPM - inicioCorteY;
  canvasVirtual.fillRect(corteVelocidadeX, inicioCorteY, corteVelocidadeLarg, alturaDoCorte, BLACK);
  canvasVirtual.fillRect(corteRpmDigitalX, inicioCorteY, corteRpmDigitalLarg, alturaDoCorte, BLACK);
}

void renderizarHUDPrincipal() {
  canvasVirtual.fillScreen(BLACK);
  desenharBarraRPMAnaDigi(telemetria.rpm);

  canvasVirtual.setTextDatum(textdatum_t::top_right);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextSize(0.75);
  canvasVirtual.setTextColor(WHITE);
  char bufferRPM[16];
  snprintf(bufferRPM, sizeof(bufferRPM), "%.0f rpm", telemetria.rpm);
  canvasVirtual.drawString(bufferRPM, 310, 26);
  canvasVirtual.setTextDatum(textdatum_t::top_left);

  desenharIconeTPS(272, 56, telemetria.tps);

  canvasVirtual.setFont(&fonts::Font7);
  canvasVirtual.setTextSize(0.9);
  int vel = (int)telemetria.velocidade;
  if (vel < 0) vel = 0;
  if (vel > 999) vel = 999;
  int c = vel / 100;
  int d = (vel % 100) / 10;
  int u = vel % 10;
  int startX = 114;
  int posY = 24;
  int largCaractere = 24;
  canvasVirtual.setTextColor((c == 0) ? BLACK : WHITE);
  canvasVirtual.setCursor(startX, posY);
  canvasVirtual.printf("%d", c);
  canvasVirtual.setTextColor((c == 0 && d == 0) ? BLACK : WHITE);
  canvasVirtual.setCursor(startX + largCaractere, posY);
  canvasVirtual.printf("%d", d);
  canvasVirtual.setTextColor(WHITE);
  canvasVirtual.setCursor(startX + (largCaractere * 2), posY);
  canvasVirtual.printf("%d", u);

  canvasVirtual.setFont(&fonts::Font0);
  canvasVirtual.setTextSize(1.5);
  canvasVirtual.setCursor(192, 70);
  canvasVirtual.print("km/h");

  int offsetHUD_Y = 42;
  desenharIconeBateria(10, offsetHUD_Y);
  canvasVirtual.setCursor(30, offsetHUD_Y);
  canvasVirtual.printf("%.1fV", telemetria.bateria);
  desenharIconeCoolant(10, offsetHUD_Y + 18);
  canvasVirtual.setCursor(30, offsetHUD_Y + 18);
  canvasVirtual.printf("%.0f C", telemetria.tempCoolant);
  desenharIconeIntake(10, offsetHUD_Y + 36);
  canvasVirtual.setCursor(30, offsetHUD_Y + 36);
  canvasVirtual.printf("%.0f C", telemetria.tempIntake);

  canvasVirtual.setFont(&fonts::Font2);
  canvasVirtual.setTextSize(1.0);
  canvasVirtual.setTextDatum(textdatum_t::top_left);
  canvasVirtual.setTextColor(CYAN);
  canvasVirtual.drawString("Boost: " + String(telemetria.boost, 2) + " psi", 10, 94);
  canvasVirtual.drawString("Max: " + String(telemetria.boost_max, 2) + " psi", 10, 107);

  canvasVirtual.setTextDatum(textdatum_t::top_right);
  canvasVirtual.setTextColor(CYAN);
  canvasVirtual.drawString(String(telemetria.consumo_ml_min, 0) + " ml/min", 310, 94);
  canvasVirtual.drawString(String(telemetria.consumo_l_100km, 1) + " L/100km", 310, 107);
  canvasVirtual.drawString(String(telemetria.consumo_total_litros, 3) + " L", 310, 81);
  canvasVirtual.setTextDatum(textdatum_t::top_left);

  sprFundoTurboReduzido.pushSprite(&canvasVirtual, 0, 120);
  float angBoost = processarAngulo(telemetria.boost, -15.0, 15.0);
  sprPonteiroOriginal.pushRotateZoom(&canvasVirtual, 0 + pivotX_R, 120 + pivotY_R, angBoost, 0.5, 0.5, 0);

  if (telemetria.boost > 15.0) {
    canvasVirtual.fillCircle(15, 132, 5, RED);
    canvasVirtual.drawCircle(15, 132, 7, WHITE);
  }

  sprFundoFuelReduzido.pushSprite(&canvasVirtual, 160, 120);
  float angConsumo = processarAngulo(telemetria.consumo_ml_min, 0.0, 400.0);
  sprPonteiroOriginal.pushRotateZoom(&canvasVirtual, 160 + pivotX_R, 120 + pivotY_R, angConsumo, 0.5, 0.5, 0);

  if (telemetria.consumo_ml_min > 400.0) {
    canvasVirtual.fillCircle(300, 132, 5, RED);
    canvasVirtual.drawCircle(300, 132, 7, WHITE);
    int animOffset = (millis() / 15) % 30;
    canvasVirtual.setFont(&fonts::Font2);
    canvasVirtual.setTextColor(YELLOW);
    canvasVirtual.setCursor(270, 170 - animOffset);
    canvasVirtual.print("$");
  }
}

void renderizarMenuConfig() {
  canvasVirtual.fillScreen(COR_MENU_BG);
  canvasVirtual.setTextColor(WHITE);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextSize(0.8);
  canvasVirtual.setCursor(10, 10);
  canvasVirtual.print("MENU DE CONFIGURAÇÃO");
  canvasVirtual.drawFastHLine(10, 32, 300, CINZA_CLARO);

  canvasVirtual.setFont(&fonts::Font2);
  canvasVirtual.setTextSize(1.0);

  canvasVirtual.fillRect(20, 50, 280, 40, CINZA_ESCURO);
  canvasVirtual.drawRect(20, 50, 280, 40, WHITE);
  canvasVirtual.setCursor(35, 62);
  canvasVirtual.printf("MOTOR: %.1f Litros [Alterar]", telemetria.motor_litros);

  canvasVirtual.fillRect(20, 105, 280, 40, CINZA_ESCURO);
  canvasVirtual.drawRect(20, 105, 280, 40, WHITE);
  canvasVirtual.setCursor(35, 117);
  canvasVirtual.print("> VER TERMINAL DEBUG OBD2");

  canvasVirtual.fillRect(80, 185, 160, 35, RED);
  canvasVirtual.drawRect(80, 185, 160, 35, WHITE);
  canvasVirtual.setTextDatum(textdatum_t::top_center);
  canvasVirtual.drawString("VOLTAR DASH", 160, 195);
  canvasVirtual.setTextDatum(textdatum_t::top_left);
}

void renderizarSubDebug() {
  canvasVirtual.fillScreen(BLACK);
  canvasVirtual.setTextColor(CYAN);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextSize(0.7);
  canvasVirtual.setCursor(10, 10);
  canvasVirtual.print("OBD2 TERMINAL TRAFEGO LOG");
  canvasVirtual.drawFastHLine(10, 30, 300, CINZA_ESCURO);

  canvasVirtual.setFont(&fonts::Font0);
  canvasVirtual.setTextSize(1.5);
  canvasVirtual.setTextColor(GREEN);
  int startY = 40;
  // 🌟 CORREÇÃO: Lê as strings de log de forma pública e limpa vindas do obd2_manager
  int totalLogs = obterTotalLogs();
  for (int i = 0; i < totalLogs; i++) {
    canvasVirtual.setCursor(10, startY + (i * 15));
    canvasVirtual.print(obterLinhaLog(i));
  }

  canvasVirtual.fillRect(200, 195, 110, 35, CINZA_ESCURO);
  canvasVirtual.drawRect(200, 195, 110, 35, WHITE);
  canvasVirtual.setCursor(220, 205);
  canvasVirtual.print("VOLTAR");
}

void inicializarDisplay() {
  canvasVirtual.setPsram(true);
  canvasVirtual.setColorDepth(16);
  canvasVirtual.createSprite(320, 240);
  sprFundoTurboReduzido.setPsram(true);
  sprFundoTurboReduzido.setColorDepth(16);
  sprFundoTurboReduzido.createSprite(160, 120);
  sprFundoFuelReduzido.setPsram(true);
  sprFundoFuelReduzido.setColorDepth(16);
  sprFundoFuelReduzido.createSprite(160, 120);
  sprPonteiroOriginal.setPsram(true);
  sprPonteiroOriginal.setColorDepth(16);
  sprPonteiroOriginal.createSprite(320, 240);
  sprPonteiroOriginal.fillScreen(0);
  sprPonteiroOriginal.setPivot(pivotX_G, pivotY_G);
  sprPonteiroOriginal.drawPngFile("/sd/ponteiro.png", 0, 0);
  M5Canvas tempGrande(&M5.Display);
  tempGrande.setPsram(true);
  tempGrande.createSprite(320, 240);
  tempGrande.drawPngFile("/sd/turbo.png", 0, 0);
  tempGrande.pushRotateZoom(&sprFundoTurboReduzido, 80, 60, 0.0, 0.5, 0.5);
  tempGrande.drawPngFile("/sd/fuel.png", 0, 0);
  tempGrande.pushRotateZoom(&sprFundoFuelReduzido, 80, 60, 0.0, 0.5, 0.5);
  tempGrande.deleteSprite();
}
void atualizarInterfaceGrafica() {
  int ev;
  if (xQueueReceive(xFilaTouch, &ev, 0) == pdTRUE) {
    switch (ev) {
      case 1: modoAtual = TELA_MENU_CONFIG; break;
      case 2: modoAtual = TELA_HUD_PRINCIPAL; break;
      case 3:
        {
          float novoTamanho = telemetria.motor_litros + 0.5f;
          if (novoTamanho > 4.0f) novoTamanho = 1.0f;
          salvarAjusteMotorFlash(novoTamanho, telemetria.eficiencia_ve);
        }
        break;
      case 4: modoAtual = TELA_SUB_DEBUG; break;
      case 5: modoAtual = TELA_MENU_CONFIG; break;  // 🌟 CONSERTO: Botão de voltar da tela de logs agora responde!
      default: break;
    }
  }
  if (modoAtual == TELA_HUD_PRINCIPAL) {
    renderizarHUDPrincipal();
  } else if (modoAtual == TELA_MENU_CONFIG) {
    renderizarMenuConfig();
  } else if (modoAtual == TELA_SUB_DEBUG) {
    renderizarSubDebug();
  }
  canvasVirtual.pushSprite(0, 0);
}
