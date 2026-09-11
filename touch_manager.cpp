#include "touch_manager.h"
#include "display_manager.h"
#include <M5Unified.h>

extern QueueHandle_t xFilaTouch;
extern int obterModoTelaAtual();

static bool estavaPressionado = false;
static int ultimoX = 0;
static int ultimoY = 0;

void inicializarTouch() { estavaPressionado = false; }

void atualizarTouch() {
  int numToques = M5.Touch.getCount();
  int modoTela = obterModoTelaAtual();

  if (numToques > 0) {
    auto detalhe = M5.Touch.getDetail(0);
    ultimoX = detalhe.x;
    ultimoY = detalhe.y;
    estavaPressionado = true;
  } 
  else {
    if (estavaPressionado) {
      estavaPressionado = false; 
      int ev = 0; 

      if (modoTela == 0) { // TELA_HUD_PRINCIPAL
        ev = 1; // Toque rápido abre o menu
      } 
      else if (modoTela == 1) { // TELA_MENU_CONFIG
        // Mapeia os cliques nas linhas do menu
        if (ultimoY >= 45 && ultimoY <= 180) {
          if (ultimoY < 81) ev = 3;       // Escolheu Linha 1: Buscar Wi-Fi
          else if (ultimoY < 117) ev = 8; // Linha 2: Bluetooth (Rola índice por hora)
          else if (ultimoY < 153) ev = 2; // Linha 3: Forçar reinicialização (Volta pro dash)
        }
        if (ultimoY >= 185) ev = 2; // Linha 4: Sair e voltar
      } 
      else if (modoTela == 3) { // WIFI_TELA_LISTA
        if (ultimoY >= 45 && ultimoY <= 170) {
          ev = 4; // Selecionou a rede clicada
        } else if (ultimoY >= 195) {
          ev = 2; // Botão vermelho de cancelar volta pro Dash
        }
      } 
      else if (modoTela == 4) { // WIFI_TELA_SENHA (Teclado Virtual de 3 botões na base)
        if (ultimoY >= 185) {
          if (ultimoX < 95) ev = 5;       // Clicou no botão [ < ]
          else if (ultimoX < 185) ev = 6; // Clicou no botão [ > ]
          else ev = 7;                    // Clicou no botão [ CONECTAR ]
        }
      }

      if (ev != 0 && xFilaTouch != NULL) {
        xQueueSend(xFilaTouch, &ev, 0);
      }
    }
  }
}