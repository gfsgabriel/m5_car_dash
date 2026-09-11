#include "display_menu.h"

#define CINZA_ESCURO 0x39E7
#define CINZA_CLARO  0xC618
#define COR_MENU_BG  0x10A2 

void renderizarMenuConfig(M5Canvas* cv, int hoverIdx) {
  cv->fillScreen(COR_MENU_BG);
  cv->setTextColor(WHITE); cv->setFont(&fonts::Font4); cv->setTextSize(0.8);
  cv->setCursor(10, 10); cv->print("MENU DE SELECAO");
  cv->drawFastHLine(10, 32, 300, CINZA_CLARO);

  cv->setFont(&fonts::Font2); cv->setTextSize(1.0);
  const char* opcoes[] = {
    "1. Buscar Rede Wi-Fi", 
    "2. Configurar Bluetooth", 
    "3. Voltar pro Dash"
  };
  
  for (int i = 0; i < 3; i++) {
    int y = 45 + (i * 42);
    
    // Se o dedo estiver em cima (Hover), pinta o fundo de AZUL, senão CINZA
    uint16_t corFundo = (i == hoverIdx) ? BLUE : CINZA_ESCURO;
    uint16_t corBorda = (i == hoverIdx) ? WHITE : CINZA_ESCURO;
    
    cv->fillRect(15, y - 2, 290, 32, corFundo);
    cv->drawRect(15, y - 2, 290, 32, corBorda);
    
    cv->setCursor(25, y + 4); cv->print(opcoes[i]);
  }
}

void renderizarListaWifi(M5Canvas* cv, int numRedes, int selecionadaIdx, int hoverIdx) {
  cv->fillScreen(BLACK);
  cv->setTextColor(CYAN); cv->setFont(&fonts::Font4); cv->setTextSize(0.7);
  cv->setCursor(10, 10); cv->print("REDES DISPONIVEIS");
  cv->drawFastHLine(10, 30, 300, CINZA_ESCURO);

  cv->setFont(&fonts::Font2); cv->setTextSize(1.0);
  int totalVisiveis = min(numRedes + 1, 4);
  
  for (int i = 0; i < totalVisiveis; i++) {
    int y = 45 + (i * 35);
    
    // Realce visual de Hover se o dedo arrastar por cima da linha
    uint16_t corFundo = (i == hoverIdx) ? BLUE : CINZA_ESCURO;
    
    cv->fillRect(15, y - 2, 290, 26, corFundo);
    cv->setCursor(25, y + 2);
    
    if (i < numRedes) {
      cv->setTextColor(WHITE);
      // Se for a rede selecionada fixa (marcada), põe um asterisco
      if (i == selecionadaIdx) cv->print("* " + WiFi.SSID(i));
      else cv->print(WiFi.SSID(i));
    } else {
      cv->setTextColor(YELLOW);
      cv->print("[ Buscar Novamente ]");
    }
  }
  
  // Botão inferior fixo de Cancelar
  uint16_t corVoltar = (hoverIdx == 99) ? BLUE : RED; // 99 = Código de Hover para Cancelar
  cv->fillRect(80, 195, 160, 35, corVoltar);
  cv->setTextColor(WHITE);
  cv->setTextDatum(textdatum_t::top_center);
  cv->drawString("CANCELAR", 160, 205);
  cv->setTextDatum(textdatum_t::top_left);
}

void renderizarTecladoSenha(M5Canvas* cv, const char* buffer, int cursor, int hoverKeyId) {
  cv->fillScreen(BLACK);
  cv->setTextColor(YELLOW); cv->setFont(&fonts::Font4); cv->setTextSize(0.7);
  cv->setCursor(10, 10); cv->print("DIGITE A SENHA DO WI-FI");
  cv->drawFastHLine(10, 30, 300, CINZA_ESCURO);

  cv->setFont(&fonts::Font4); cv->setTextColor(GREEN);
  cv->setCursor(15, 60); cv->print(buffer);
  
  // Cursor de digitação incremental
  cv->fillRect(15 + (cursor * 14), 90, 12, 3, WHITE);

  // Teclas direcionais virtuais na base com realce de Hover
  cv->setFont(&fonts::Font2); cv->setTextSize(1.0); cv->setTextColor(WHITE);
  
  uint16_t bgVoltar = (hoverKeyId == 0) ? BLUE : CINZA_ESCURO;
  uint16_t bgAvancar = (hoverKeyId == 1) ? BLUE : CINZA_ESCURO;
  uint16_t bgConectar = (hoverKeyId == 2) ? BLUE : GREEN;

  cv->fillRect(15, 185, 75, 40, bgVoltar);     cv->drawString("[ < ]", 35, 197);
  cv->fillRect(105, 185, 75, 40, bgAvancar);   cv->drawString("[ > ]", 125, 197);
  cv->fillRect(195, 185, 110, 40, bgConectar); cv->drawString("CONECTAR", 215, 197);
}

void renderizarMenuBluetooth(M5Canvas* cv, int hoverIdx) {
  cv->fillScreen(COR_MENU_BG);
  cv->setTextColor(WHITE); cv->setFont(&fonts::Font4); cv->setTextSize(0.8);
  cv->setCursor(10, 10); cv->print("AJUSTES BLUETOOTH (DUMMY)");
  cv->drawFastHLine(10, 32, 300, CINZA_CLARO);

  cv->setFont(&fonts::Font2); cv->setTextSize(1.0);
  const char* opcoes[] = {
    "1. Parear Novo Dispositivo", 
    "2. Reiniciar Conexao ECU", 
    "3. Voltar pro Menu"
  };
  
  for (int i = 0; i < 3; i++) {
    int y = 50 + (i * 42);
    
    uint16_t corFundo = (i == hoverIdx) ? BLUE : CINZA_ESCURO;
    uint16_t corBorda = (i == hoverIdx) ? WHITE : CINZA_ESCURO;
    
    cv->fillRect(20, y - 2, 280, 32, corFundo);
    cv->drawRect(20, y - 2, 280, 32, corBorda);
    
    cv->setCursor(35, y + 4); cv->print(opcoes[i]);
  }
}
