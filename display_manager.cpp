#include "display_manager.h"
#include "display_hud.h"
#include "display_menu.h"
#include <M5Unified.h>

// Instanciação das variáveis globais de Hover consumidas pelos menus
int globalHoverIdx = -1;
int globalTecladoHoverKeyId = -1;

static ModosTela modoAtual = TELA_HUD_PRINCIPAL;

// Buffers Gráficos em PSRAM
static M5Canvas canvasVirtual(&M5.Display);       
static M5Canvas sprFundoTurboReduzido(&M5.Display);    
static M5Canvas sprFundoFuelReduzido(&M5.Display);    
static M5Canvas sprPonteiroOriginal(&M5.Display); 

// Variáveis de controle de estado do Wi-Fi (Mock/Herdado)
static int numRedesEncontradas = 0;
static int idxRedeSelecionada = 0;
static String ssidSelecionado = "";
static char senhaBuffer[32] = "";
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
  // Configuração dos Buffers Gráficos na PSRAM do CoreS3
  canvasVirtual.setPsram(true); canvasVirtual.setColorDepth(16); canvasVirtual.createSprite(320, 240);
  sprFundoTurboReduzido.setPsram(true); sprFundoTurboReduzido.setColorDepth(16); sprFundoTurboReduzido.createSprite(160, 120);
  sprFundoFuelReduzido.setPsram(true); sprFundoFuelReduzido.setColorDepth(16); sprFundoFuelReduzido.createSprite(160, 120);
  sprPonteiroOriginal.setPsram(true); sprPonteiroOriginal.setColorDepth(16); sprPonteiroOriginal.createSprite(320, 240);

  // Carrega e monta os Sprites e agulhas do cartão SD uma única vez
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
  // Escuta os comandos despachados com exclusividade pelo touch_manager após o Release do dedo
  if (xQueueReceive(xFilaTouch, &ev, 0) == pdTRUE) {
    switch (ev) {
      case 1: modoAtual = TELA_MENU_CONFIG; break;   // Toque na HUD abre o Menu Principal
      case 2: modoAtual = TELA_HUD_PRINCIPAL; break; // Comando para voltar ao painel principal
      case 3: // Dispara busca de Redes Wi-Fi
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
      case 4: modoAtual = BT_TELA_MENU; break;       // Abre Menu Bluetooth Dummy
      case 5: // Selecionou a rede Wi-Fi da lista
        if (idxRedeSelecionada < numRedesEncontradas) {
          ssidSelecionado = WiFi.SSID(idxRedeSelecionada);
          memset(senhaBuffer, 0, sizeof(senhaBuffer));
          posCursorSenha = 0; idxCharAtual = 0;
          senhaBuffer[0] = ALFABETO_GLOBAL[0];
          modoAtual = WIFI_TELA_SENHA;
        } else {
          // Clicou na opção de buscar de novo
          xQueueSend(xFilaTouch, (int[]){3}, 0);
        }
        break;
      case 6: // Teclado: Diminui Caractere [ < ]
        idxCharAtual = (idxCharAtual - 1 + TAM_ALFABETO_GLOBAL) % TAM_ALFABETO_GLOBAL;
        senhaBuffer[posCursorSenha] = ALFABETO_GLOBAL[idxCharAtual];
        break;
      case 7: // Teclado: Avança Caractere [ > ]
        if (posCursorSenha < 30) {
          posCursorSenha++; idxCharAtual = 0;
          senhaBuffer[posCursorSenha] = ALFABETO_GLOBAL[idxCharAtual];
        }
        break;
      case 8: // Teclado: Finaliza e conecta
        WiFi.begin(ssidSelecionado.c_str(), senhaBuffer);
        modoAtual = TELA_HUD_PRINCIPAL;
        break;
      case 9: // Bluetooth: Reinicia Conexão (Dummy)
        adicionarLogDebug("SYS: Reiniciando barramento OBD2...");
        modoAtual = TELA_HUD_PRINCIPAL;
        break;
      case 10: // Rola o carrossel de Wi-Fi abaixo
        idxRedeSelecionada = (idxRedeSelecionada + 1) % (numRedesEncontradas + 1);
        break;
      case 11: modoAtual = TELA_MENU_CONFIG; break; // Sub-menus voltam para a raiz
    }
  }

  // Máquina de Estados de Renderização Dedicada
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

  // Cospe o frame estável montado direto na tela física do CoreS3
  canvasVirtual.pushSprite(0, 0);
}
