#include "display_hud.h"
#include "telemetria.h"

#define CINZA_ESCURO 0x39E7
#define CINZA_CLARO  0xC618

static const int barraLarguraBloco = 4;   
static const int barraLarguraEspaco = 1;  
static const int alturaBarraRPM = 32;      
static const int inicioCorteY = 18; 
static const int corteVelocidadeX = 110;   static const int corteVelocidadeLarg = 100; 
static const int corteRpmDigitalX = 230;   static const int corteRpmDigitalLarg = 85; 
static const float ANGULO_MIN = -60.0;     static const float ANGULO_MAX = 60.0;
static const int pivotX_R = 80;            static const int pivotY_R = 90;  

static float mapearAngulo(float valor, float vMin, float vMax) {
  if (valor < vMin) valor = vMin; if (valor > vMax) valor = vMax;
  return (valor - vMin) * (ANGULO_MAX - ANGULO_MIN) / (vMax - vMin) + ANGULO_MIN;
}

static void desenharBarraRPMAnaDigi(M5Canvas* cv, float rpmAtual) {
  float rpmMin = 0.0; float redlineStart = 6500.0; float rpmMax = 8000.0;
  int cicloTotal = barraLarguraBloco + barraLarguraEspaco;
  int limitePixels = (rpmAtual - rpmMin) * 320.0 / (rpmMax - rpmMin);
  if (limitePixels < 0) limitePixels = 0; if (limitePixels > 320) limitePixels = 320;

  for (int x = 0; x < 320; x++) {
    if ((x % cicloTotal) >= barraLarguraBloco) continue; 
    if (x <= limitePixels) {
      float rpmNestePixel = rpmMin + (x * (rpmMax - rpmMin) / 320.0);
      cv->drawFastVLine(x, 0, alturaBarraRPM, (rpmNestePixel < redlineStart) ? GREEN : RED);
    } else {
      cv->drawFastVLine(x, 0, alturaBarraRPM, CINZA_ESCURO);
    }
  }
  // 🌟 CONFIGURADO E SEGURO: Máscaras limpas sem variáveis soltas
  int alturaDoCorte = alturaBarraRPM - inicioCorteY;
  cv->fillRect(corteVelocidadeX, inicioCorteY, corteVelocidadeLarg, alturaDoCorte, BLACK);
  cv->fillRect(corteRpmDigitalX, inicioCorteY, corteRpmDigitalLarg, alturaDoCorte, BLACK);
}

static void desenharIconeTPS(M5Canvas* cv, int centroX, int centroY, float tpsPercent) {
  int raioBola = 6; int alturaTracoMax = 24; 
  cv->drawCircle(centroX, centroY, raioBola, WHITE);
  float anguloGraus = 90.0 - (tpsPercent * 90.0 / 100.0); float anguloRad = anguloGraus * DEG_TO_RAD;
  int x1 = centroX + (cos(anguloRad) * (alturaTracoMax / 2)); int y1 = centroY - (sin(anguloRad) * (alturaTracoMax / 2));
  int x2 = centroX - (cos(anguloRad) * (alturaTracoMax / 2)); int y2 = centroY + (sin(anguloRad) * (alturaTracoMax / 2));
  cv->drawLine(x1, y1, x2, y2, ORANGE);
  cv->setFont(&fonts::Font0); cv->setTextSize(1.0); cv->setTextColor(CINZA_CLARO);
  cv->setTextDatum(textdatum_t::top_center); cv->drawString(String((int)tpsPercent) + "% tps", centroX, centroY + 14);
  cv->setTextDatum(textdatum_t::top_left);
}

void renderizarHUDPrincipal(M5Canvas* cv, M5Canvas* sprPonteiro, M5Canvas* sprTurbo, M5Canvas* sprFuel) {
  desenharBarraRPMAnaDigi(cv, telemetria.rpm);
  
  cv->setTextDatum(textdatum_t::top_right);
  cv->setFont(&fonts::Font4); cv->setTextSize(0.75); cv->setTextColor(WHITE);
  char bufferRPM[16]; snprintf(bufferRPM, sizeof(bufferRPM), "%.0f rpm", telemetria.rpm);
  cv->drawString(bufferRPM, 310, 26);
  cv->setTextDatum(textdatum_t::top_left);

  desenharIconeTPS(cv, 272, 56, telemetria.tps);

  cv->setFont(&fonts::Font7); cv->setTextSize(0.9);
  int vel = (int)telemetria.velocidade;
  if (vel < 0) vel = 0; if (vel > 999) vel = 999;
  int c = vel / 100; int d = (vel % 100) / 10; int u = vel % 10;
  int startX = 114; int posY = 24; int largCaractere = 24;
  cv->setTextColor((c == 0) ? BLACK : WHITE); cv->setCursor(startX, posY); cv->printf("%d", c);
  cv->setTextColor((c == 0 && d == 0) ? BLACK : WHITE); cv->setCursor(startX + largCaractere, posY); cv->printf("%d", d);
  cv->setTextColor(WHITE); cv->setCursor(startX + (largCaractere * 2), posY); cv->printf("%d", u);
  
  cv->setFont(&fonts::Font0); cv->setTextSize(1.5); cv->setCursor(192, 70); cv->print("km/h");

  cv->setCursor(10, 42); cv->printf("Bat: %.1fV", telemetria.bateria);
  cv->setCursor(10, 60); cv->printf("Cool: %.0f C", telemetria.tempCoolant);
  cv->setCursor(10, 78); cv->printf("Intk: %.0f C", telemetria.tempIntake);

  cv->setFont(&fonts::Font2); cv->setTextSize(1.0);
  cv->setTextDatum(textdatum_t::top_left); cv->setTextColor(CYAN);
  cv->drawString("Boost: " + String(telemetria.boost, 2) + " psi", 10, 94);
  cv->drawString("Max: " + String(telemetria.boost_max, 2) + " psi", 10, 107);

  cv->setTextDatum(textdatum_t::top_right); cv->setTextColor(CYAN);
  cv->drawString(String(telemetria.consumo_ml_min, 0) + " ml/min", 310, 94);
  cv->drawString(String(telemetria.consumo_l_100km, 1) + " L/100km", 310, 107);
  cv->drawString(String(telemetria.consumo_total_litros, 3) + " L", 310, 81);
  cv->setTextDatum(textdatum_t::top_left);

  sprTurbo->pushSprite(cv, 0, 120);
  float angBoost = mapearAngulo(telemetria.boost, -15.0, 15.0);
  sprPonteiro->pushRotateZoom(cv, 0 + pivotX_R, 120 + pivotY_R, angBoost, 0.5, 0.5, 0);

  if (telemetria.boost > 15.0) {
    cv->fillCircle(15, 132, 5, RED); cv->drawCircle(15, 132, 7, WHITE);
  }

  sprFuel->pushSprite(cv, 160, 120);
  float angConsumo = mapearAngulo(telemetria.consumo_ml_min, 0.0, 400.0); 
  sprPonteiro->pushRotateZoom(cv, 160 + pivotX_R, 120 + pivotY_R, angConsumo, 0.5, 0.5, 0);

  if (telemetria.consumo_ml_min > 400.0) {
    cv->fillCircle(300, 132, 5, RED); cv->drawCircle(300, 132, 7, WHITE);
    int animOffset = (millis() / 15) % 30;
    cv->setFont(&fonts::Font2); cv->setTextColor(YELLOW);
    cv->setCursor(270, 170 - animOffset); cv->print("$");
  }
}
