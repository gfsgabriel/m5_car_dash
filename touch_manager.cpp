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

      if (modoTela == 0) { 
        // Qualquer toque rápido em qualquer lugar da HUD principal abre o menu limpo
        ev = 1; 
      } 
      else if (modoTela == 1) { 
        if (ultimoX >= 20 && ultimoX <= 300) {
          if (ultimoY >= 50 && ultimoY <= 90) ev = 3;       // Botão Ajustar Litragem Motor
          else if (ultimoY >= 105 && ultimoY <= 145) ev = 4; // Botão Abrir Terminal Debug
        }
        if (ultimoX >= 80 && ultimoX <= 240 && ultimoY >= 185 && ultimoY <= 220) {
          ev = 2; // Botão Vermelho Voltar Pro Dash
        }
      }

      if (ev != 0 && xFilaTouch != NULL) {
        xQueueSend(xFilaTouch, &ev, 0);
      }
    }
  }
}