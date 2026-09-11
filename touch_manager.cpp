#include "touch_manager.h"
#include "display_manager.h"
#include <M5Unified.h>

extern QueueHandle_t xFilaTouch;
extern int obterModoTelaAtual();

// Imports hover indices updated by the active screens
extern int globalHoverIdx;
extern int globalTecladoHoverKeyId;

static bool estavaPressionado = false;
static int ultimoX = 0;
static int ultimoY = 0;

void inicializarTouch() {
  estavaPressionado = false;
  globalHoverIdx = -1;
  globalTecladoHoverKeyId = -1;
}

void atualizarTouch() {
  int numToques = M5.Touch.getCount();
  int modoTela = obterModoTelaAtual();

  if (numToques > 0) {
    // -----------------------------------------------------------------
    // PHASE 1: HOVER (Finger is down, sliding over elements)
    // -----------------------------------------------------------------
    auto detalhe = M5.Touch.getDetail(0);
    ultimoX = detalhe.x;
    ultimoY = detalhe.y;
    estavaPressionado = true;

    if (modoTela == TELA_MENU_CONFIG) {
      if (ultimoX >= 15 && ultimoX <= 305) {
        if (ultimoY >= 45 && ultimoY <= 77) globalHoverIdx = 0;       // WiFi Line
        else if (ultimoY >= 87 && ultimoY <= 119) globalHoverIdx = 1;  // BT Line
        else if (ultimoY >= 129 && ultimoY <= 161) globalHoverIdx = 2; // Back Line
        else globalHoverIdx = -1;
      } else { globalHoverIdx = -1; }
    } 
    else if (modoTela == WIFI_TELA_LISTA) {
      if (ultimoX >= 15 && ultimoX <= 305 && ultimoY >= 45 && ultimoY <= 185) {
        globalHoverIdx = (ultimoY - 45) / 35; // Dynamically tracks which scanned row is active
      } else if (ultimoX >= 80 && ultimoX <= 240 && ultimoY >= 195 && ultimoY <= 230) {
        globalHoverIdx = 99; // Highlights "CANCELAR" button
      } else { globalHoverIdx = -1; }
    } 
    else if (modoTela == WIFI_TELA_SENHA) {
      if (ultimoY >= 185 && ultimoY <= 225) {
        if (ultimoX >= 15 && ultimoX <= 90) globalTecladoHoverKeyId = 0;      // [ < ] Button
        else if (ultimoX >= 105 && ultimoX <= 180) globalTecladoHoverKeyId = 1; // [ > ] Button
        else if (ultimoX >= 195 && ultimoX <= 305) globalTecladoHoverKeyId = 2; // [ CONECTAR ] Button
        else globalTecladoHoverKeyId = -1;
      } else { globalTecladoHoverKeyId = -1; }
    }
    else if (modoTela == BT_TELA_MENU) {
      if (ultimoX >= 20 && ultimoX <= 300) {
        if (ultimoY >= 50 && ultimoY <= 82) globalHoverIdx = 0;       // Pair Line
        else if (ultimoY >= 92 && ultimoY <= 124) globalHoverIdx = 1;  // Reset Line
        else if (ultimoY >= 134 && ultimoY <= 166) globalHoverIdx = 2; // Back Line
        else globalHoverIdx = -1;
      } else { globalHoverIdx = -1; }
    }
  } 
  else {
    // -----------------------------------------------------------------
    // PHASE 2: RELEASE (Finger is lifted from the glass)
    // -----------------------------------------------------------------
    if (estavaPressionado) {
      estavaPressionado = false; 
      int ev = 0; 

      if (modoTela == TELA_HUD_PRINCIPAL) {
        ev = 1; // Direct tap anywhere opens main selection menu
      } 
      else if (modoTela == TELA_MENU_CONFIG) {
        if (ultimoX >= 15 && ultimoX <= 305) {
          if (ultimoY >= 45 && ultimoY <= 77) ev = 3;       // Confirmed WiFi row
          else if (ultimoY >= 87 && ultimoY <= 119) ev = 4;  // Confirmed BT row
          else if (ultimoY >= 129 && ultimoY <= 161) ev = 2; // Confirmed Back row
        }
      } 
      else if (modoTela == WIFI_TELA_LISTA) {
        if (ultimoX >= 15 && ultimoX <= 305 && ultimoY >= 45 && ultimoY <= 185) {
          int row = (ultimoY - 45) / 35;
          if (row >= 0 && row <= 3) {
            if (row == 3) ev = 3; // Clicked [Search Again] maps back to a new scan
            else { ev = 5; }      // Row confirmed, loads password field
          }
        } else if (ultimoX >= 80 && ultimoX <= 240 && ultimoY >= 195 && ultimoY <= 230) {
          ev = 11; // Cancel row clicked, steps backward
        }
      } 
      else if (modoTela == WIFI_TELA_SENHA) {
        if (ultimoY >= 185 && ultimoY <= 225) {
          if (ultimoX >= 15 && ultimoX <= 90) ev = 6;       // Trigger [ < ]
          else if (ultimoX >= 105 && ultimoX <= 180) ev = 7; // Trigger [ > ]
          else if (ultimoX >= 195 && ultimoX <= 305) ev = 8; // Trigger [ CONECTAR ]
        }
      }
      else if (modoTela == BT_TELA_MENU) {
        if (ultimoX >= 20 && ultimoX <= 300) {
          if (ultimoY >= 50 && ultimoY <= 82) ev = 9;       // Trigger Pair Dummy
          else if (ultimoY >= 92 && ultimoY <= 124) ev = 9;  // Trigger Restart Dummy
          else if (ultimoY >= 134 && ultimoY <= 166) ev = 11; // Back out to root selection
        }
      }

      // Reset hover highlights down immediately upon finger lift
      globalHoverIdx = -1;
      globalTecladoHoverKeyId = -1;

      // Queue execution block command dispatch
      if (ev != 0 && xFilaTouch != NULL) {
        xQueueSend(xFilaTouch, &ev, 0);
      }
    }
  }
}
