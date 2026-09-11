#include "display_manager.h"
#include "display_hud.h"
#include "display_menu.h"
#include "telemetria.h"
#include "webserver_manager.h"
#include "spi_lock.h"
#include <M5Unified.h>

static ModosTela modoAtual = TELA_HUD_PRINCIPAL;

static M5Canvas canvasVirtual(&M5.Display);
static M5Canvas sprFundoTurboReduzido(&M5.Display);
static M5Canvas sprFundoFuelReduzido(&M5.Display);
static M5Canvas sprPonteiroOriginal(&M5.Display);

static const int pivotX_G = 160;
static const int pivotY_G = 180;

ModosTela obterModoTelaAtual() { return modoAtual; }

void definirModoTela(ModosTela novoModo) {
  modoAtual = novoModo;
}

void processarTouchHover(int x, int y) {
  if (modoAtual != TELA_HUD_PRINCIPAL) {
    atualizarHoverMenu(x, y);
  }
}

void processarTouchRelease(int x, int y) {
  if (modoAtual == TELA_HUD_PRINCIPAL) {
    int ev = 1;
    extern QueueHandle_t xFilaTouch;
    xQueueSend(xFilaTouch, &ev, 0);
  }
  else {
    processarReleaseMenu(x, y);
  }
}

void adicionarLogDebug(String linhaLog) {
  registrarLogWebServer(linhaLog);
}

void inicializarDisplay() {
  // Protege o SPI do SD durante a leitura das PNGs
  spiSDLock();

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

  spiSDUnlock();
}

void renderizarDisplay() {
  if (modoAtual == TELA_HUD_PRINCIPAL) {
    renderizarHUDPrincipal(canvasVirtual, sprFundoTurboReduzido, sprFundoFuelReduzido, sprPonteiroOriginal);
  }
  else if (modoAtual == TELA_MENU_CONFIG) {
    renderizarMenuConfig(canvasVirtual);
  }
  else if (modoAtual == WIFI_TELA_LISTA) {
    renderizarWifiLista(canvasVirtual);
  }
  else if (modoAtual == WIFI_TELA_ACOES) {
    renderizarWifiAcoes(canvasVirtual);
  }
  else if (modoAtual == WIFI_TELA_SENHA) {
    renderizarWifiSenha(canvasVirtual);
  }

  canvasVirtual.pushSprite(0, 0);
}