#include "display_manager.h"
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Preferences.h>

#define CINZA_ESCURO 0x39E7
#define CINZA_CLARO 0xC618
#define COR_MENU_BG 0x10A2

// --- MÁQUINA DE ESTADOS REESTRUTURADA ---
enum ModosTela {
  TELA_HUD_PRINCIPAL,
  TELA_MENU_CONFIG,
  WIFI_TELA_SCAN,
  WIFI_TELA_LISTA,
  WIFI_TELA_SENHA,
  BT_TELA_PIN
};
static ModosTela modoAtual = TELA_HUD_PRINCIPAL;

extern QueueHandle_t xFilaTouch;

// Buffers Gráficos
static M5Canvas canvasVirtual(&M5.Display);
static M5Canvas sprFundoTurboReduzido(&M5.Display);
static M5Canvas sprFundoFuelReduzido(&M5.Display);
static M5Canvas sprPonteiroOriginal(&M5.Display);

// --- GESTÃO DE REDE (NVS & MULTI) ---
Preferences preferences;
WiFiMulti wifiMulti;
struct RedeSalva {
  String ssid;
  String pass;
};
static RedeSalva redesSalvas[5];
static int qtdRedesSalvas = 0;

// Variáveis de Varredura e Digitação (Herdadas do seu projeto)
static int itemMenuSelecionado = 0;
static int numRedesEncontradas = 0;
static int idxRedeSelecionada = 0;
static String ssidSelecionado = "";
static char senhaBuffer[32] = "";
static int posCursorSenha = 0;
static int idxCharAtual = 0;
static String pinBluetoothBuffer = "1234";

const char ALFABETO[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=";
const int TAM_ALFABETO = sizeof(ALFABETO) - 1;

// Geometria dos Medidores
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

float processarAngulo(float valor, float vMin, float vMax) {
  if (valor < vMin) valor = vMin;
  if (valor > vMax) valor = vMax;
  return (valor - vMin) * (ANGULO_MAX - ANGULO_MIN) / (vMax - vMin) + ANGULO_MIN;
}
int obterModoTelaAtual() {
  return (int)modoAtual;
}

// --- Métodos de NVS de Rede (Seu Projeto) ---
void carregarRedesSalvas() {
  preferences.begin("wifi_cfg", true);
  qtdRedesSalvas = preferences.getInt("qtd", 0);
  if (qtdRedesSalvas > 5) qtdRedesSalvas = 5;
  for (int i = 0; i < qtdRedesSalvas; i++) {
    redesSalvas[i].ssid = preferences.getString(("s_" + String(i)).c_str(), "");
    redesSalvas[i].pass = preferences.getString(("p_" + String(i)).c_str(), "");
  }
  preferences.end();
}

void salvarNovaRede(String ssid, String pass) {
  preferences.begin("wifi_cfg", false);
  if (qtdRedesSalvas < 5) {
    redesSalvas[qtdRedesSalvas].ssid = ssid;
    redesSalvas[qtdRedesSalvas].pass = pass;
    preferences.putString(("s_" + String(qtdRedesSalvas)).c_str(), ssid);
    preferences.putString(("p_" + String(qtdRedesSalvas)).c_str(), pass);
    qtdRedesSalvas++;
    preferences.putInt("qtd", qtdRedesSalvas);
  }
  preferences.end();
}

// --- Ícones Nativos ---
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
  canvasVirtual.fillRect(corteVelocidadeX, inicioCorteY, corteVelocidadeLarg, alturaBarraRPM - inicioCorteY, BLACK);
  canvasVirtual.fillRect(corteRpmDigitalX, inicioCorteY, corteRpmDigitalLarg, alturaBarraRPM - inicioCorteY, BLACK);
}

// INTERFACE 0: DASH PRINCIPAL
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
}

// TELA 1: REESTRUTURAÇÃO DO MENU PRINCIPAL
void renderizarMenuConfig() {
  canvasVirtual.fillScreen(COR_MENU_BG);
  canvasVirtual.setTextColor(WHITE);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextSize(0.8);
  canvasVirtual.setCursor(10, 10);
  canvasVirtual.print("MENU CONFIGURAÇÕES");
  canvasVirtual.drawFastHLine(10, 32, 300, CINZA_CLARO);

  canvasVirtual.setFont(&fonts::Font2);
  canvasVirtual.setTextSize(1.0);
  const char* opcoes[] = { "1. Buscar Rede Wi-Fi", "2. Parear Bluetooth OBD2", "3. Reiniciar Conexão OBD2", "4. Voltar pro Dash" };

  for (int i = 0; i < 4; i++) {
    int y = 45 + (i * 36);
    if (i == itemMenuSelecionado) {
      canvasVirtual.fillRect(15, y - 2, 290, 26, CINZA_ESCURO);
      canvasVirtual.drawRect(15, y - 2, 290, 26, WHITE);
    } else {
      canvasVirtual.fillRect(15, y - 2, 290, 26, CINZA_ESCURO);
    }
    canvasVirtual.setCursor(25, y + 2);
    canvasVirtual.print(opcoes[i]);
  }
}

// TELA 2: LISTA DE SCAN WI-FI (Aparência adaptada para o CoreS3)
void renderizarListaWifi() {
  canvasVirtual.fillScreen(BLACK);


  canvasVirtual.setTextColor(CYAN);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextSize(0.7);
  canvasVirtual.setCursor(10, 10);
  canvasVirtual.print("REDES DISPONÍVEIS");
  canvasVirtual.drawFastHLine(10, 30, 300, CINZA_ESCURO);
  canvasVirtual.setFont(&fonts::Font2);
  canvasVirtual.setTextSize(1.0);
  int totalVisiveis = min(numRedesEncontradas + 1, 4);
  for (int i = 0; i < totalVisiveis; i++) {
    int y = 45 + (i * 35);
    if (i == idxRedeSelecionada) {
      canvasVirtual.fillRect(15, y - 2, 290, 26, CINZA_ESCURO);
      canvasVirtual.drawRect(15, y - 2, 290, 26, WHITE);
    } else {
      canvasVirtual.fillRect(15, y - 2, 290, 26, CINZA_ESCURO);
    }
    canvasVirtual.setCursor(25, y + 2);
    if (i < numRedesEncontradas) canvasVirtual.print(WiFi.SSID(i));
    else canvasVirtual.print("[ Buscar Novamente ]");
  }
  // Botão inferior de cancelamento
  canvasVirtual.fillRect(80, 195, 160, 35, RED);
  canvasVirtual.setCursor(120, 205);
  canvasVirtual.print("CANCELAR");
}
// TELA 3: TECLADO ALFANUMÉRICO DE SENHA (Sua lógica baseada no Alfabeto)
void renderizarTecladoSenha() {
  canvasVirtual.fillScreen(BLACK);
  canvasVirtual.setTextColor(YELLOW);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextSize(0.7);
  canvasVirtual.setCursor(10, 10);
  canvasVirtual.print("DIGITE A SENHA DO WI-FI");
  canvasVirtual.drawFastHLine(10, 30, 300, CINZA_ESCURO);
  canvasVirtual.setFont(&fonts::Font4);
  canvasVirtual.setTextColor(GREEN);
  canvasVirtual.setCursor(15, 55);
  canvasVirtual.print(senhaBuffer);
  // Desenha o cursor piscando embaixo do caractere ativo
  canvasVirtual.fillRect(15 + (posCursorSenha * 14), 85, 12, 3, WHITE);
  // Teclas direcionais virtuais na base do Touch
  canvasVirtual.setFont(&fonts::Font2);
  canvasVirtual.setTextSize(1.0);
  canvasVirtual.setTextColor(WHITE);
  canvasVirtual.fillRect(15, 185, 75, 40, CINZA_ESCURO);
  canvasVirtual.drawString("[ < ]", 35, 197);
  canvasVirtual.fillRect(105, 185, 75, 40, CINZA_ESCURO);
  canvasVirtual.drawString("[ > ]", 125, 197);
  canvasVirtual.fillRect(195, 185, 110, 40, GREEN);
  canvasVirtual.drawString("CONECTAR", 215, 197);
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
  carregarRedesSalvas();
}
void atualizarInterfaceGrafica() {
  int ev;
  if (xQueueReceive(xFilaTouch, &ev, 0) == pdTRUE) {
    switch (ev) {
      case 1: modoAtual = TELA_MENU_CONFIG; break;    // Toque abre menu
      case 2: modoAtual = TELA_HUD_PRINCIPAL; break;  // Voltar pro dash
      case 3:                                         // Clicou no botão de Scan Wi-Fi (Sua rotina)
        modoAtual = WIFI_TELA_SCAN;
        canvasVirtual.fillScreen(BLACK);
        canvasVirtual.setCursor(50, 100);
        canvasVirtual.print("Buscando redes...");
        canvasVirtual.pushSprite(0, 0);
        WiFi.disconnect(true);
        numRedesEncontradas = WiFi.scanNetworks(false, true);
        idxRedeSelecionada = 0;
        modoAtual = WIFI_TELA_LISTA;
        break;
      case 4:  // Clicou em uma rede da lista
        ssidSelecionado = WiFi.SSID(idxRedeSelecionada);
        memset(senhaBuffer, 0, sizeof(senhaBuffer));
        posCursorSenha = 0;
        idxCharAtual = 0;
        senhaBuffer[0] = ALFABETO[0];
        modoAtual = WIFI_TELA_SENHA;
        break;
      case 5:  // Tecla de diminuir caractere [ < ]
        idxCharAtual = (idxCharAtual - 1 + TAM_ALFABETO) % TAM_ALFABETO;
        senhaBuffer[posCursorSenha] = ALFABETO[idxCharAtual];
        break;
      case 6:  // Tecla de avançar caractere [ > ]
        if (posCursorSenha < 30) {
          posCursorSenha++;
          idxCharAtual = 0;
          senhaBuffer[posCursorSenha] = ALFABETO[idxCharAtual];
        }
        break;
      case 7:  // Apertou botão final de Conectar
        salvarNovaRede(ssidSelecionado, String(senhaBuffer));
        WiFi.begin(ssidSelecionado.c_str(), senhaBuffer);
        modoAtual = TELA_HUD_PRINCIPAL;
        break;
      case 8:  // Botão Navegar Menu Abaixo
        itemMenuSelecionado = (itemMenuSelecionado + 1) % 4;
        break;
      case 9:  // Navega na lista de redes abaixo
        idxRedeSelecionada = (idxRedeSelecionada + 1) % (numRedesEncontradas + 1);
        break;
    }
  }
  if (modoAtual == TELA_HUD_PRINCIPAL) renderizarHUDPrincipal();
  else if (modoAtual == TELA_MENU_CONFIG) renderizarMenuConfig();
  else if (modoAtual == WIFI_TELA_LISTA) renderizarListaWifi();
  else if (modoAtual == WIFI_TELA_SENHA) renderizarTecladoSenha();
  canvasVirtual.pushSprite(0, 0);
}
