#include "display_hud.h"
#include "telemetria.h"

#define CINZA_ESCURO 0x39E7
#define CINZA_CLARO  0xC618

static float ANGULO_MIN = -60.0; 
static float ANGULO_MAX = 60.0;
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

static float processarAngulo(float valor, float vMin, float vMax) {
  if (valor < vMin) valor = vMin; 
  if (valor > vMax) valor = vMax;
  return (valor - vMin) * (ANGULO_MAX - ANGULO_MIN) / (vMax - vMin) + ANGULO_MIN;
}

static void desenharIconeBateria(M5Canvas &canvas, int x, int y) {
  canvas.drawRect(x, y + 2, 14, 9, WHITE); 
  canvas.fillRect(x + 2, y, 3, 2, WHITE);  
  canvas.fillRect(x + 9, y, 3, 2, WHITE);  
  canvas.drawFastHLine(x + 2, y + 6, 3, WHITE); 
  canvas.drawRect(x + 9, y + 5, 3, 3, WHITE);  
}

static void desenharIconeCoolant(M5Canvas &canvas, int x, int y) {
  canvas.drawRect(x + 5, y, 4, 10, WHITE); 
  canvas.fillCircle(x + 7, y + 10, 4, WHITE);
  canvas.drawFastHLine(x, y + 13, 14, CYAN); 
  canvas.drawFastHLine(x + 2, y + 15, 10, CYAN);
}

static void desenharIconeIntake(M5Canvas &canvas, int x, int y) {
  canvas.drawRect(x, y + 2, 14, 8, WHITE); 
  canvas.fillRect(x + 5, y, 4, 12, BLUE); 
}

static void desenharIconeTPS(M5Canvas &canvas, int centroX, int centroY, float tpsPercent) {
  int raioBola = 6; 
  int alturaTracoMax = 24; 
  canvas.drawCircle(centroX, centroY, raioBola, WHITE);
  float anguloGraus = 90.0 - (tpsPercent * 90.0 / 100.0); 
  float anguloRad = anguloGraus * DEG_TO_RAD;
  int x1 = centroX + (cos(anguloRad) * (alturaTracoMax / 2)); 
  int y1 = centroY - (sin(anguloRad) * (alturaTracoMax / 2));
  int x2 = centroX - (cos(anguloRad) * (alturaTracoMax / 2)); 
  int y2 = centroY + (sin(anguloRad) * (alturaTracoMax / 2));
  canvas.drawLine(x1, y1, x2, y2, ORANGE);
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.0); 
  canvas.setTextColor(CINZA_CLARO);
  canvas.setTextDatum(textdatum_t::top_center); 
  canvas.drawString(String((int)tpsPercent) + "% tps", centroX, centroY + 14);
  canvas.setTextDatum(textdatum_t::top_left);
}

static void desenharBarraRPMAnaDigi(M5Canvas &canvas, float rpmAtual) {
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
      canvas.drawFastVLine(x, 0, alturaBarraRPM, (rpmNestePixel < redlineStart) ? GREEN : RED);
    } else {
      canvas.drawFastVLine(x, 0, alturaBarraRPM, CINZA_ESCURO);
    }
  }
  int alturaDoCorte = alturaBarraRPM - inicioCorteY;
  canvas.fillRect(corteVelocidadeX, inicioCorteY, corteVelocidadeLarg, alturaDoCorte, BLACK);
  canvas.fillRect(corteRpmDigitalX, inicioCorteY, corteRpmDigitalLarg, alturaDoCorte, BLACK);
}

void inicializarHUD() {}

void renderizarHUDPrincipal(M5Canvas &canvasVirtual, M5Canvas &sprTurbo, M5Canvas &sprFuel, M5Canvas &sprPonteiro) {
  canvasVirtual.fillScreen(BLACK);
  desenharBarraRPMAnaDigi(canvasVirtual, telemetria.rpm);
  
  canvasVirtual.setTextDatum(textdatum_t::top_right);
  canvasVirtual.setFont(&fonts::Font4); 
  canvasVirtual.setTextSize(0.75); 
  canvasVirtual.setTextColor(WHITE);
  char bufferRPM[16]; 
  snprintf(bufferRPM, sizeof(bufferRPM), "%.0f rpm", telemetria.rpm);
  canvasVirtual.drawString(bufferRPM, 310, 26);
  canvasVirtual.setTextDatum(textdatum_t::top_left);

  desenharIconeTPS(canvasVirtual, 272, 56, telemetria.tps);

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
  desenharIconeBateria(canvasVirtual, 10, offsetHUD_Y); 
  canvasVirtual.setCursor(30, offsetHUD_Y); 
  canvasVirtual.printf("%.1fV", telemetria.bateria);
  desenharIconeCoolant(canvasVirtual, 10, offsetHUD_Y + 18); 
  canvasVirtual.setCursor(30, offsetHUD_Y + 18); 
  canvasVirtual.printf("%.0f C", telemetria.tempCoolant);
  desenharIconeIntake(canvasVirtual, 10, offsetHUD_Y + 36); 
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

  sprTurbo.pushSprite(&canvasVirtual, 0, 120);
  float angBoost = processarAngulo(telemetria.boost, -15.0, 15.0);
  sprPonteiro.pushRotateZoom(&canvasVirtual, 0 + pivotX_R, 120 + pivotY_R, angBoost, 0.5, 0.5, 0);

  if (telemetria.boost > 15.0) {
    canvasVirtual.fillCircle(15, 132, 5, RED); 
    canvasVirtual.drawCircle(15, 132, 7, WHITE);
  }

  sprFuel.pushSprite(&canvasVirtual, 160, 120);
  float angConsumo = processarAngulo(telemetria.consumo_ml_min, 0.0, 400.0); 
  sprPonteiro.pushRotateZoom(&canvasVirtual, 160 + pivotX_R, 120 + pivotY_R, angConsumo, 0.5, 0.5, 0);

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