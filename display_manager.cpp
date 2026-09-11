#include "display_manager.h"
#include "display_hud.h"
#include "display_menu.h"
#include <M5Unified.h>
#include <WiFi.h> // 🌟 ADICIONADO! Resolve o escopo do WiFi.scan

int globalHoverIdx = -1;
int globalTecladoHoverKeyId = -1;

static ModosTela modoAtual = TELA_HUD_PRINCIPAL;
extern QueueHandle_t xFilaTouch;

static M5Canvas canvasVirtual(&M5.Display);       
static M5Canvas sprFundoTurboReduzido(&M5.Display);    
static M5Canvas sprFundoFuelReduzido(&M5.Display);    
static M5Canvas sprPonteiroOriginal(&M5.Display); 

static int numRedesEncontradas = 0;
static int idxRedeSelecionada = 0;
static String ssidSelecionado = "";
static char senhaBuffer[32] = ""; // 🌟 CONSERTADO: De char simples para Array de 32 bytes!
static int posCursorSenha = 0;
static int idxCharAtual = 0;

const char ALFABETO_GLOBAL[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=";
const int TAM_ALFABETO_GLOBAL = sizeof(ALFABETO_GLOBAL) - 1;

void adicionarLogDebug(const String& linhaLog) {
  Serial.println(linhaLog);
}

int obterModoTelaAtual() { 
  return (int)modoAtual; 
}

void inicializarDisplay() {
  canvasVirtual.setPsram(true); canvasVirtual.setColorDepth(16); canvasVirtual.createSprite(320, 240);
  sprFundoTurboReduzido.setPsram(true); sprFundoTurboReduzido.setColorDepth(16); sprFundoTurboReduzido.createSprite(160, 120);
  sprFundoFuelReduzido.setPsram(true); sprFundoFuelReduzido.setColorDepth(16); sprFundoFuelReduzido.createSprite(160, 120);
  sprPonteiroOriginal.setPsram(true); sprPonteiroOriginal.setColorDepth(16); sprPonteiroOriginal.createSprite(320, 240);

  sprPonteiroOriginal.fillScreen(0);
  sprPonteiroOriginal.setPivot(160, 180);
  sprPonteiroOriginal.drawPngFile("/sd/ponteiro.png", 0, 0);

  M5Canvas tempGrande(&M5.Display);
  tempGrande.setPsram(true); tempGrande.createSprite(320, 240);
  
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
        modoAtual = WIFI_TELA_SCAN;
        canvasVirtual.fillScreen(BLACK);
        canvasVirtual.setTextColor(YELLOW); canvasVirtual.setFont(&fonts::Font4); canvasVirtual.setTextSize(0.8);
        canvasVirtual.drawString("Buscando Redes...", 160, 120, textdatum_t::middle_center);
        canvasVirtual.pushSprite(0, 0);
        
        WiFi.disconnect(true);
        numRedesEncontradas = WiFi.scanNetworks(false, true);
        idxRedeSelecionada = 0;
        modoAtual = WIFI_TELA_LISTA;
        break;
      case 4: modoAtual = BT_TELA_MENU; break;       
      case 5: 
        if (idxRedeSelecionada < numRedesEncontradas) {
          ssidSelecionado = WiFi.SSID(idxRedeSelecionada);
          memset(senhaBuffer, 0, sizeof(senhaBuffer));
          posCursorSenha = 0; idxCharAtual = 0;
          senhaBuffer[0] = ALFABETO_GLOBAL[0];
          modoAtual = WIFI_TELA_SENHA;
        } else {
          int scanEv = 3;
          xQueueSend(xFilaTouch, &scanEv, 0);
        }
        break;
      case 6: 
        idxCharAtual = (idxCharAtual - 1 + TAM_ALFABETO_GLOBAL) % TAM_ALFABETO_GLOBAL;
        senhaBuffer[posCursorSenha] = ALFABETO_GLOBAL[idxCharAtual];
        break;
      case 7: 
        if (posCursorSenha < 30) {
          posCursorSenha++; idxCharAtual = 0;
          senhaBuffer[posCursorSenha] = ALFABETO_GLOBAL[idxCharAtual];
        }
        break;
      case 8: 
        WiFi.begin(ssidSelecionado.c_str(), senhaBuffer);
        modoAtual = TELA_HUD_PRINCIPAL;
        break;
      case 9: 
        adicionarLogDebug("SYS: Reiniciando barramento OBD2...");
        modoAtual = TELA_HUD_PRINCIPAL;
        break;
      case 10: 
        idxRedeSelecionada = (idxRedeSelecionada + 1) % (numRedesEncontradas + 1);
        break;
      case 11: modoAtual = TELA_MENU_CONFIG; break; 
    }
  }

  if (modoAtual == TELA_HUD_PRINCIPAL) {
    renderizarHUDPrincipal(&canvasVirtual, &sprPonteiroOriginal, &sprFundoTurboReduzido, &sprFundoFuelReduzido);
  } else if (modoAtual == TELA_MENU_CONFIG) {
    renderizarMenuConfig(&canvasVirtual, globalHoverIdx);
  } else if (modoAtual == WIFI_TELA_LISTA) {
    renderizarListaWifi(&canvasVirtual, numRedesEncontradas, idxRedeSelecionada, globalHoverIdx);
  } else if (modoAtual == WIFI_TELA_SENHA) {
    renderizarTecladoSenha(&canvasVirtual, senhaBuffer, posCursorSenha, globalTecladoHoverKeyId);
  } else if (modoAtual == BT_TELA_MENU) {
    renderizarMenuBluetooth(&canvasVirtual, globalHoverIdx);
  }

  canvasVirtual.pushSprite(0, 0);
}
