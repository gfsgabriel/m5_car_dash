#include "display_menu.h"
#include "display_manager.h"
#include "wifi_manager.h"

extern QueueHandle_t xFilaTouch;

enum ModoTeclado { MAIUSCULAS, MINUSCULAS, NUMEROS, SIMBOLOS };
static ModoTeclado modoKbd = MAIUSCULAS;

static int hoverBtnIdx = -1;
static int wifiScrollOffset = 0;
static int redeSelecionadaIndex = -1;

// --- Estado da tela de AÇÕES ---
static String ssidSelecionado = "";
static bool   redeSelecionadaTemSenha = false;

// --- Buffer do teclado ---
static String senhaWifiBuffer = "";

// Layout do Teclado (4 modos)
static const char* kbdMaiusculas[4] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM", "" };
static const char* kbdMinusculas[4] = { "qwertyuiop", "asdfghjkl", "zxcvbnm", "" };
static const char* kbdNumeros[4]    = { "1234567890", "@#$_&-+()/", ".,:;!?", "" };
static const char* kbdSimbolos[4]   = { "*\"':;!?", "~`|/\\<>[]{}", "=+%^&()#@$", "" };

// ---------------------------------------------------------------
// Hover (não usado profundamente, mas mantido)
// ---------------------------------------------------------------
void atualizarHoverMenu(int x, int y) {
  hoverBtnIdx = -1;
  // (reservado pra highlight futuro, se quiser)
}

// ---------------------------------------------------------------
// Release / ações de toque
// ---------------------------------------------------------------
void processarReleaseMenu(int x, int y) {
  ModosTela modo = obterModoTelaAtual();
  int evento = 0;

  // ============================================================
  // TELA: MENU CONFIG PRINCIPAL
  // ============================================================
  if (modo == TELA_MENU_CONFIG) {
    if (y >= 50 && y <= 95) {
      // Ir para Wi-Fi
      iniciarScanWifiAsync();
      wifiScrollOffset = 0;
      redeSelecionadaIndex = -1;
      evento = 10; // WIFI_TELA_LISTA
    }
    else if (y >= 170 && y <= 215) {
      evento = 2; // Voltar pro HUD
    }
  }

  // ============================================================
  // TELA: LISTA DE REDES (SCAN)
  // ============================================================
  else if (modo == WIFI_TELA_LISTA) {
    int totalRedes = obterTotalRedesEscaneadas();

    // Clique numa rede da lista (4 linhas visíveis)
    if (x < 240 && y >= 40 && y <= 180) {
      int idx = wifiScrollOffset + ((y - 40) / 35);
      if (idx >= 0 && idx < totalRedes) {
        redeSelecionadaIndex = idx;
        RedeWifiItem r = obterRedePorIndice(idx);
        ssidSelecionado = r.ssid;
        redeSelecionadaTemSenha = r.salva;
        evento = 13; // WIFI_TELA_ACOES
      }
    }
    // Scroll UP
    else if (x >= 250 && y >= 40 && y <= 85) {
      if (wifiScrollOffset > 0) wifiScrollOffset--;
    }
    // Scroll DOWN
    else if (x >= 250 && y >= 95 && y <= 140) {
      if (wifiScrollOffset < totalRedes - 4) wifiScrollOffset++;
    }
    // Botão CANCELAR (rodapé direito)
    else if (x >= 210 && x <= 300 && y >= 195) {
      evento = 5; // Volta pro menu config
    }
    // Botão RESCAN (rodapé esquerdo)
    else if (x >= 10 && x <= 100 && y >= 195) {
      iniciarScanWifiAsync();
      wifiScrollOffset = 0;
      redeSelecionadaIndex = -1;
    }
  }

  // ============================================================
  // TELA: AÇÕES DA REDE (CONECTAR / ESQUECER / VOLTAR)
  // ============================================================
  else if (modo == WIFI_TELA_ACOES) {
    // CONECTAR (y 60..110)
    if (y >= 60 && y <= 110) {
      String senhaSalva = buscarSenhaSalva(ssidSelecionado);
      if (senhaSalva.length() > 0) {
        iniciarConexaoDireta(ssidSelecionado, senhaSalva);
        evento = 10; // volta pra lista (mostra IP no topo se conectar)
      } else {
        senhaWifiBuffer = "";
        evento = 12; // WIFI_TELA_SENHA
      }
    }
    // ESQUECER (y 120..170) — só se tiver senha salva
    else if (y >= 120 && y <= 170) {
      if (redeSelecionadaTemSenha) {
        int idxSalvo = -1;
        for (int i = 0; i < obterQtdRedesSalvas(); i++) {
          if (obterSSIDSalvo(i) == ssidSelecionado) { idxSalvo = i; break; }
        }
        if (idxSalvo >= 0) esquecerRedeSalva(idxSalvo);
        redeSelecionadaTemSenha = false;
      }
      evento = 10; // volta pra lista
    }
    // VOLTAR (y 180..230)
    else if (y >= 180) {
      evento = 10; // volta pra lista
    }
  }

  // ============================================================
  // TELA: DIGITAÇÃO DE SENHA (TECLADO QWERTY)
  // ============================================================
  else if (modo == WIFI_TELA_SENHA) {
    // Linha inferior de ações (y >= 180)
    if (y >= 180) {
      // Botão 1: Alternar modo do teclado
      if (x >= 5 && x <= 75) {
        if (modoKbd == MAIUSCULAS) modoKbd = MINUSCULAS;
        else if (modoKbd == MINUSCULAS) modoKbd = NUMEROS;
        else if (modoKbd == NUMEROS) modoKbd = SIMBOLOS;
        else modoKbd = MAIUSCULAS;
      }
      // Botão 2: Backspace
      else if (x >= 80 && x <= 150) {
        if (senhaWifiBuffer.length() > 0) {
          senhaWifiBuffer.remove(senhaWifiBuffer.length() - 1);
        } else {
          evento = 13; // volta pra tela de AÇÕES (não pra lista)
        }
      }
      // Botão 3: Cancelar
      else if (x >= 155 && x <= 230) {
        evento = 10; // volta pra lista
      }
      // Botão 4: OK / Conectar
      else if (x >= 235 && x <= 315) {
        salvarNovaRede(ssidSelecionado, senhaWifiBuffer);
        iniciarConexaoDireta(ssidSelecionado, senhaWifiBuffer);
        evento = 10; // volta pra lista
      }
    }
    // Toque nas teclas do teclado (y entre 45 e 175)
    else if (y >= 45 && y <= 175) {
      int linha = (y - 45) / 32;
      if (linha < 0) linha = 0;
      if (linha > 2) linha = 2;

      const char* setAtual;
      if (modoKbd == MAIUSCULAS) setAtual = kbdMaiusculas[linha];
      else if (modoKbd == MINUSCULAS) setAtual = kbdMinusculas[linha];
      else if (modoKbd == NUMEROS) setAtual = kbdNumeros[linha];
      else setAtual = kbdSimbolos[linha];

      int len = strlen(setAtual);
      if (len > 0) {
        int keyWidth = 310 / len;
        int col = (x - 5) / keyWidth;
        if (col >= 0 && col < len) {
          senhaWifiBuffer += setAtual[col];
        }
      }
    }
  }

  if (evento > 0) xQueueSend(xFilaTouch, &evento, 0);
}

// ---------------------------------------------------------------
// Render: Menu Config Principal
// ---------------------------------------------------------------
void renderizarMenuConfig(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(10, 10);
  canvas.print("=== CONFIGURACOES ===");
  canvas.drawFastHLine(5, 35, 310, 0x39E7);

  // Botão Wi-Fi
  canvas.fillRect(20, 50, 280, 45, 0x18E3);
  canvas.drawRect(20, 50, 280, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(35, 65);
  canvas.print("1. Wi-Fi");
  if (wifiEstaConectado()) {
    canvas.setTextColor(GREEN);
    canvas.setCursor(180, 65);
    canvas.print("CONECTADO");
  } else {
    canvas.setTextColor(YELLOW);
    canvas.setCursor(180, 65);
    canvas.print("Desconectado");
  }

  // Status IP (se conectado)
  if (wifiEstaConectado()) {
    canvas.setTextColor(0x7BEF);
    canvas.setCursor(35, 105);
    canvas.printf("IP: %s", obterIPAtual().c_str());
  }

  // Botão Voltar
  canvas.fillRect(20, 170, 280, 45, RED);
  canvas.drawRect(20, 170, 280, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(130, 185);
  canvas.print("VOLTAR");
}

// ---------------------------------------------------------------
// Render: Lista de Redes Wi-Fi (scan)
// ---------------------------------------------------------------
void renderizarWifiLista(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);

  // Cabeçalho com IP atual (se conectado) ou status
  canvas.setCursor(5, 5);
  if (wifiEstaConectado()) {
    canvas.setTextColor(GREEN);
    canvas.printf("WIFI | IP: %s", obterIPAtual().c_str());
  } else {
    canvas.setTextColor(YELLOW);
    canvas.print("WIFI | Desconectado");
  }
  canvas.drawFastHLine(5, 25, 310, 0x39E7);

  if (!wifiScanConcluido()) {
    canvas.setTextColor(YELLOW);
    canvas.setCursor(60, 100);
    canvas.print("Escaneando Redes...");
    return;
  }

  int totalRedes = obterTotalRedesEscaneadas();

  // 4 linhas visíveis
  for (int i = 0; i < 4; i++) {
    int idxRede = i + wifiScrollOffset;
    int yPos = 35 + (i * 35);

    if (idxRede < totalRedes) {
      RedeWifiItem rede = obterRedePorIndice(idxRede);
      bool isSelected = (idxRede == redeSelecionadaIndex);

      uint16_t corFundo = isSelected ? 0x03E0 : 0x18E3;
      canvas.fillRect(5, yPos, 235, 30, corFundo);
      canvas.drawRect(5, yPos, 235, 30, WHITE);
      canvas.setTextColor(WHITE);
      canvas.setCursor(10, yPos + 7);

      String label = rede.ssid;
      if (label.length() > 13) label = label.substring(0, 10) + "...";

      canvas.printf("%s %s (%d)", label.c_str(),
                    rede.salva ? "[SALVA]" : "",
                    rede.rssi);
    } else {
      // Linha vazia
      canvas.fillRect(5, yPos, 235, 30, BLACK);
    }
  }

  // Setas de scroll
  canvas.fillRect(245, 35, 70, 45, 0x39E7);
  canvas.drawRect(245, 35, 70, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(275, 50);
  canvas.print("^");

  canvas.fillRect(245, 85, 70, 45, 0x39E7);
  canvas.drawRect(245, 85, 70, 45, WHITE);
  canvas.setCursor(275, 100);
  canvas.print("v");

  // Botões do rodapé
  canvas.fillRect(5, 190, 95, 45, GREEN);
  canvas.drawRect(5, 190, 95, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(25, 205);
  canvas.print("RESCAN");

  canvas.fillRect(205, 190, 110, 45, RED);
  canvas.drawRect(205, 190, 110, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(225, 205);
  canvas.print("VOLTAR");
}

// ---------------------------------------------------------------
// Render: Tela de AÇÕES da rede selecionada
// ---------------------------------------------------------------
void renderizarWifiAcoes(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);

  canvas.setCursor(5, 10);
  canvas.printf("Rede: %s", ssidSelecionado.c_str());
  if (redeSelecionadaTemSenha) {
    canvas.setTextColor(GREEN);
    canvas.setCursor(5, 25);
    canvas.print("[senha salva]");
  }
  canvas.drawFastHLine(5, 40, 310, 0x39E7);

  // Botão CONECTAR
  canvas.fillRect(40, 60, 240, 50, GREEN);
  canvas.drawRect(40, 60, 240, 50, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(120, 78);
  canvas.print("CONECTAR");

  // Botão ESQUECER (só ativo se tem senha salva)
  if (redeSelecionadaTemSenha) {
    canvas.fillRect(40, 120, 240, 50, ORANGE);
    canvas.drawRect(40, 120, 240, 50, WHITE);
    canvas.setTextColor(BLACK);
    canvas.setCursor(120, 138);
    canvas.print("ESQUECER");
  } else {
    canvas.fillRect(40, 120, 240, 50, 0x39E7);
    canvas.drawRect(40, 120, 240, 50, 0x7BEF);
    canvas.setTextColor(0x7BEF);
    canvas.setCursor(115, 138);
    canvas.print("(sem senha)");
  }

  // Botão VOLTAR
  canvas.fillRect(40, 180, 240, 50, RED);
  canvas.drawRect(40, 180, 240, 50, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(135, 198);
  canvas.print("VOLTAR");
}

// ---------------------------------------------------------------
// Render: Teclado de senha
// ---------------------------------------------------------------
void renderizarWifiSenha(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);

  // Campo de senha
  canvas.fillRect(5, 5, 310, 32, 0x18E3);
  canvas.drawRect(5, 5, 310, 32, WHITE);
  canvas.setTextColor(GREEN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(10, 12);
  canvas.printf("Senha (%s): %s_", ssidSelecionado.c_str(), senhaWifiBuffer.c_str());

  // Matriz de teclas
  const char** setMatriz;
  if (modoKbd == MAIUSCULAS) setMatriz = kbdMaiusculas;
  else if (modoKbd == MINUSCULAS) setMatriz = kbdMinusculas;
  else if (modoKbd == NUMEROS) setMatriz = kbdNumeros;
  else setMatriz = kbdSimbolos;

  canvas.setTextColor(WHITE);
  for (int l = 0; l < 3; l++) {
    const char* linhaStr = setMatriz[l];
    int len = strlen(linhaStr);
    if (len == 0) continue;

    int keyWidth = 310 / len;
    int yPos = 42 + (l * 33);

    for (int c = 0; c < len; c++) {
      int xPos = 5 + (c * keyWidth);
      canvas.fillRect(xPos, yPos, keyWidth - 2, 30, 0x39E7);
      canvas.drawRect(xPos, yPos, keyWidth - 2, 30, WHITE);
      canvas.setCursor(xPos + (keyWidth / 2) - 4, yPos + 7);
      canvas.write(linhaStr[c]);
    }
  }

  // Linha inferior de ações
  // 1. Alternar modo
  canvas.fillRect(5, 180, 70, 45, 0x7BE0);
  canvas.drawRect(5, 180, 70, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(15, 195);
  if (modoKbd == MAIUSCULAS) canvas.print("abc");
  else if (modoKbd == MINUSCULAS) canvas.print("123");
  else if (modoKbd == NUMEROS) canvas.print("#%&");
  else canvas.print("ABC");

  // 2. Backspace
  canvas.fillRect(80, 180, 70, 45, ORANGE);
  canvas.drawRect(80, 180, 70, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(95, 195);
  canvas.print("< BK");

  // 3. Cancelar
  canvas.fillRect(155, 180, 75, 45, RED);
  canvas.drawRect(155, 180, 75, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(162, 195);
  canvas.print("CANCEL");

  // 4. OK
  canvas.fillRect(235, 180, 80, 45, GREEN);
  canvas.drawRect(235, 180, 80, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(258, 195);
  canvas.print("OK");
}