#include "display_menu.h"
#include "display_manager.h"
#include "wifi_manager.h"
#include "bt_manager.h"

extern QueueHandle_t xFilaTouch;

enum ModoTeclado { MAIUSCULAS, MINUSCULAS, NUMEROS, SIMBOLOS };
static ModoTeclado modoKbd = MAIUSCULAS;

static int hoverBtnIdx = -1;
static int wifiScrollOffset = 0;
static int redeSelecionadaIndex = -1;

// --- Estado WiFi ---
static String ssidSelecionado = "";
static bool   redeSelecionadaTemSenha = false;
static String senhaWifiBuffer = "";

// --- Estado BT ---
static int btScrollOffset = 0;
static int btSelecionadoIndex = -1;
static String btMACSelecionado = "";
static String btPINBuffer = "";
static String btResultadoMsg = "";
static bool btResultadoOK = false;
static unsigned long btAguardandoInicio = 0;

// Layout do Teclado WiFi (4 modos)
static const char* kbdMaiusculas[4] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM", "" };
static const char* kbdMinusculas[4] = { "qwertyuiop", "asdfghjkl", "zxcvbnm", "" };
static const char* kbdNumeros[4]    = { "1234567890", "@#$_&-+()/", ".,:;!?", "" };
static const char* kbdSimbolos[4]   = { "*\"':;!?", "~`|/\\<>[]{}", "=+%^&()#@$", "" };

// ---------------------------------------------------------------
// Hover
// ---------------------------------------------------------------
void atualizarHoverMenu(int x, int y) {
  hoverBtnIdx = -1;
}

// ---------------------------------------------------------------
// Release / ações de toque
// ---------------------------------------------------------------
void processarReleaseMenu(int x, int y) {
  ModosTela modo = obterModoTelaAtual();
  int evento = 0;

  // ============================================================
  // MENU CONFIG PRINCIPAL
  // ============================================================
  if (modo == TELA_MENU_CONFIG) {
    if (y >= 50 && y <= 95) {
      iniciarScanWifiAsync();
      wifiScrollOffset = 0;
      redeSelecionadaIndex = -1;
      evento = 10;
    }
    else if (y >= 100 && y <= 145) {
      definirModoTela(BT_TELA_STATUS);
    }
    else if (y >= 170 && y <= 215) {
      evento = 2;
    }
  }

  // ============================================================
  // WIFI LISTA
  // ============================================================
  else if (modo == WIFI_TELA_LISTA) {
    int totalRedes = obterTotalRedesEscaneadas();

    if (x < 240 && y >= 40 && y <= 180) {
      int idx = wifiScrollOffset + ((y - 40) / 35);
      if (idx >= 0 && idx < totalRedes) {
        redeSelecionadaIndex = idx;
        RedeWifiItem r = obterRedePorIndice(idx);
        ssidSelecionado = r.ssid;
        redeSelecionadaTemSenha = r.salva;
        evento = 13;
      }
    }
    else if (x >= 250 && y >= 40 && y <= 85) {
      if (wifiScrollOffset > 0) wifiScrollOffset--;
    }
    else if (x >= 250 && y >= 95 && y <= 140) {
      if (wifiScrollOffset < totalRedes - 4) wifiScrollOffset++;
    }
    else if (x >= 210 && x <= 300 && y >= 195) {
      evento = 5;
    }
    else if (x >= 10 && x <= 100 && y >= 195) {
      iniciarScanWifiAsync();
      wifiScrollOffset = 0;
      redeSelecionadaIndex = -1;
    }
  }

  // ============================================================
  // WIFI AÇÕES
  // ============================================================
  else if (modo == WIFI_TELA_ACOES) {
    if (y >= 60 && y <= 110) {
      String senhaSalva = buscarSenhaSalva(ssidSelecionado);
      if (senhaSalva.length() > 0) {
        iniciarConexaoDireta(ssidSelecionado, senhaSalva);
        evento = 10;
      } else {
        senhaWifiBuffer = "";
        evento = 12;
      }
    }
    else if (y >= 120 && y <= 170) {
      if (redeSelecionadaTemSenha) {
        int idxSalvo = -1;
        for (int i = 0; i < obterQtdRedesSalvas(); i++) {
          if (obterSSIDSalvo(i) == ssidSelecionado) { idxSalvo = i; break; }
        }
        if (idxSalvo >= 0) esquecerRedeSalva(idxSalvo);
        redeSelecionadaTemSenha = false;
      }
      evento = 10;
    }
    else if (y >= 180) {
      evento = 10;
    }
  }

  // ============================================================
  // WIFI SENHA (teclado QWERTY)
  // ============================================================
  else if (modo == WIFI_TELA_SENHA) {
    if (y >= 180) {
      if (x >= 5 && x <= 75) {
        if (modoKbd == MAIUSCULAS) modoKbd = MINUSCULAS;
        else if (modoKbd == MINUSCULAS) modoKbd = NUMEROS;
        else if (modoKbd == NUMEROS) modoKbd = SIMBOLOS;
        else modoKbd = MAIUSCULAS;
      }
      else if (x >= 80 && x <= 150) {
        if (senhaWifiBuffer.length() > 0) {
          senhaWifiBuffer.remove(senhaWifiBuffer.length() - 1);
        } else {
          evento = 13;
        }
      }
      else if (x >= 155 && x <= 230) {
        evento = 10;
      }
      else if (x >= 235 && x <= 315) {
        salvarNovaRede(ssidSelecionado, senhaWifiBuffer);
        iniciarConexaoDireta(ssidSelecionado, senhaWifiBuffer);
        evento = 10;
      }
    }
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

  // ============================================================
  // BT STATUS
  // ============================================================
  else if (modo == BT_TELA_STATUS) {
    if (y >= 120 && y <= 165) {
      if (btSolicitarUsoMenu()) {
        btLiberarUsoMenu();
        btSolicitarScan();
        btScrollOffset = 0;
        btSelecionadoIndex = -1;
        btAguardandoInicio = 0;
        evento = 20;
      } else {
        btAguardandoInicio = millis();
      }
    }
    else if (y >= 175 && y <= 215 && x >= 20 && x <= 155) {
      if (obterMACSalvo().length() > 0) {
        if (btEstaConectado()) desconectarBT();
        esquecerMAC();
      }
    }
    else if (y >= 175 && y <= 215 && x >= 165 && x <= 300) {
      evento = 5;
    }
  }

  // ============================================================
  // BT LISTA
  // ============================================================
  else if (modo == BT_TELA_LISTA) {
    int total = obterTotalDispositivosBT();

    if (x < 240 && y >= 40 && y <= 180) {
      int idx = btScrollOffset + ((y - 40) / 35);
      if (idx >= 0 && idx < total) {
        btSelecionadoIndex = idx;
        DispositivoBT d = obterDispositivoBTPorIndice(idx);
        btMACSelecionado = d.mac;
        btPINBuffer = "";
        evento = 22;
      }
    }
    else if (x >= 250 && y >= 40 && y <= 85) {
      if (btScrollOffset > 0) btScrollOffset--;
    }
    else if (x >= 250 && y >= 95 && y <= 140) {
      if (btScrollOffset < total - 4) btScrollOffset++;
    }
    else if (x >= 10 && x <= 100 && y >= 195) {
      if (btSolicitarUsoMenu()) {
        btLiberarUsoMenu();
        btSolicitarScan();
        btScrollOffset = 0;
        btSelecionadoIndex = -1;
      }
    }
    else if (x >= 210 && x <= 300 && y >= 195) {
      evento = 21;
    }
  }

  // ============================================================
  // BT SENHA (teclado numérico)
  // ============================================================
  else if (modo == BT_TELA_SENHA) {
    if (y >= 45 && y <= 175) {
      int linha = (y - 45) / 32;
      const char* linhas[4] = {"123", "456", "789", "0"};
      if (linha >= 0 && linha < 4) {
        int keyWidth = 103;
        int col = (x - 5) / keyWidth;
        if (col >= 0 && col < 3) {
          if (btPINBuffer.length() < 4) {
            btPINBuffer += linhas[linha][col];
          }
        }
      }
    }
    else if (y >= 180) {
      if (x >= 5 && x <= 100) {
        if (btPINBuffer.length() > 0) btPINBuffer.remove(btPINBuffer.length() - 1);
      }
      else if (x >= 105 && x <= 205) {
        evento = 20;
      }
      else if (x >= 215 && x <= 315) {
        // OK — INICIA PAREAMENTO (bloqueante, mas com tela de msg antes)
        Serial.println("BT[menu]: OK pressionado, iniciando pareamento...");

        btSetMensagem("Pareando com dispositivo...");
        definirModoTela(BT_TELA_MSG);
        renderizarDisplay();

        btResultadoOK = parearComBT(btMACSelecionado, btPINBuffer);
        if (btResultadoOK) {
          btSetMensagem("Conectando...");
          renderizarDisplay();
          btResultadoOK = conectarBT(btMACSelecionado);
        }
        btResultadoMsg = btResultadoOK ? "Conectado!" : "Falha ao parear";

        definirModoTela(BT_TELA_RESULTADO);
        renderizarDisplay();
        return;   // não manda evento
      }
    }
  }

  // ============================================================
  // BT CONECTANDO / RESULTADO / MSG
  // ============================================================
  else if (modo == BT_TELA_CONECTANDO) {
    if (y >= 180) evento = 21;
  }
  else if (modo == BT_TELA_RESULTADO) {
    if (y >= 175) evento = 21;
  }
  else if (modo == BT_TELA_MSG) {
    // Tela bloqueante — não processa toque
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

  canvas.fillRect(20, 100, 280, 45, 0x18E3);
  canvas.drawRect(20, 100, 280, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(35, 115);
  canvas.print("2. Bluetooth");
  if (btEstaConectado()) {
    canvas.setTextColor(GREEN);
    canvas.setCursor(180, 115);
    canvas.print("CONECTADO");
  } else {
    canvas.setTextColor(YELLOW);
    canvas.setCursor(180, 115);
    canvas.print("Desconectado");
  }

  canvas.fillRect(20, 170, 280, 45, RED);
  canvas.drawRect(20, 170, 280, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(130, 185);
  canvas.print("VOLTAR");
}

// ---------------------------------------------------------------
// Render: WiFi Lista
// ---------------------------------------------------------------
void renderizarWifiLista(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);

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
      canvas.fillRect(5, yPos, 235, 30, BLACK);
    }
  }

  canvas.fillRect(245, 35, 70, 45, 0x39E7);
  canvas.drawRect(245, 35, 70, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(275, 50);
  canvas.print("^");

  canvas.fillRect(245, 85, 70, 45, 0x39E7);
  canvas.drawRect(245, 85, 70, 45, WHITE);
  canvas.setCursor(275, 100);
  canvas.print("v");

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
// Render: WiFi Ações
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

  canvas.fillRect(40, 60, 240, 50, GREEN);
  canvas.drawRect(40, 60, 240, 50, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(120, 78);
  canvas.print("CONECTAR");

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

  canvas.fillRect(40, 180, 240, 50, RED);
  canvas.drawRect(40, 180, 240, 50, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(135, 198);
  canvas.print("VOLTAR");
}

// ---------------------------------------------------------------
// Render: WiFi Senha
// ---------------------------------------------------------------
void renderizarWifiSenha(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);

  canvas.fillRect(5, 5, 310, 32, 0x18E3);
  canvas.drawRect(5, 5, 310, 32, WHITE);
  canvas.setTextColor(GREEN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(10, 12);
  canvas.printf("Senha (%s): %s_", ssidSelecionado.c_str(), senhaWifiBuffer.c_str());

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

  canvas.fillRect(5, 180, 70, 45, 0x7BE0);
  canvas.drawRect(5, 180, 70, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(15, 195);
  if (modoKbd == MAIUSCULAS) canvas.print("abc");
  else if (modoKbd == MINUSCULAS) canvas.print("123");
  else if (modoKbd == NUMEROS) canvas.print("#%&");
  else canvas.print("ABC");

  canvas.fillRect(80, 180, 70, 45, ORANGE);
  canvas.drawRect(80, 180, 70, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(95, 195);
  canvas.print("< BK");

  canvas.fillRect(155, 180, 75, 45, RED);
  canvas.drawRect(155, 180, 75, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(162, 195);
  canvas.print("CANCEL");

  canvas.fillRect(235, 180, 80, 45, GREEN);
  canvas.drawRect(235, 180, 80, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(258, 195);
  canvas.print("OK");
}

// ---------------------------------------------------------------
// Render: BT STATUS
// ---------------------------------------------------------------
void renderizarBTStatus(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(10, 10);
  canvas.print("=== BLUETOOTH ===");
  canvas.drawFastHLine(5, 35, 310, 0x39E7);

  canvas.setCursor(10, 50);
  if (btEstaConectado()) {
    canvas.setTextColor(GREEN);
    canvas.print("Conectado:");
    canvas.setTextColor(WHITE);
    canvas.setCursor(10, 70);
    canvas.print(obterNomeConectado());
  } else {
    canvas.setTextColor(YELLOW);
    canvas.print("Desconectado");
  }

  String mac = obterMACSalvo();
  bool temMAC = (mac.length() > 0);
  canvas.setTextColor(0x7BEF);
  canvas.setCursor(10, 95);
  if (temMAC) {
    canvas.printf("MAC: %s", mac.c_str());
  } else {
    canvas.print("Nenhum dispositivo salvo");
  }

  if (btAguardandoInicio > 0 && millis() - btAguardandoInicio < 3000) {
    canvas.setTextColor(ORANGE);
    canvas.setCursor(10, 110);
    canvas.print("OBD Manager em uso. Aguarde...");
  }

  canvas.fillRect(20, 120, 280, 45, GREEN);
  canvas.drawRect(20, 120, 280, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(120, 135);
  canvas.print("PROCURAR NOVO");

  if (temMAC) {
    canvas.fillRect(20, 175, 135, 40, ORANGE);
    canvas.drawRect(20, 175, 135, 40, WHITE);
    canvas.setTextColor(BLACK);
    canvas.setCursor(50, 188);
    canvas.print("ESQUECER");
  } else {
    canvas.fillRect(20, 175, 135, 40, 0x39E7);
    canvas.drawRect(20, 175, 135, 40, 0x7BEF);
    canvas.setTextColor(0x7BEF);
    canvas.setCursor(45, 188);
    canvas.print("(sem salvo)");
  }

  canvas.fillRect(165, 175, 135, 40, RED);
  canvas.drawRect(165, 175, 135, 40, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(210, 188);
  canvas.print("VOLTAR");
}

// ---------------------------------------------------------------
// Render: BT LISTA
// ---------------------------------------------------------------
void renderizarBTLista(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(5, 5);
  canvas.print("BT | Buscando...");
  canvas.drawFastHLine(5, 25, 310, 0x39E7);

  int total = obterTotalDispositivosBT();

  if (total == 0) {
    canvas.setTextColor(YELLOW);
    canvas.setCursor(50, 100);
    canvas.print("Buscando... aguarde");
  } else {
    for (int i = 0; i < 4; i++) {
      int idx = i + btScrollOffset;
      int yPos = 35 + (i * 35);

      if (idx < total) {
        DispositivoBT d = obterDispositivoBTPorIndice(idx);
        bool sel = (idx == btSelecionadoIndex);
        uint16_t fundo = sel ? 0x03E0 : 0x18E3;

        canvas.fillRect(5, yPos, 235, 30, fundo);
        canvas.drawRect(5, yPos, 235, 30, WHITE);
        canvas.setTextColor(WHITE);
        canvas.setCursor(10, yPos + 7);

        String label = d.nome;
        if (label.length() > 15) label = label.substring(0, 12) + "...";
        canvas.printf("%s (%d)", label.c_str(), d.rssi);
      }
    }

    canvas.fillRect(245, 35, 70, 45, 0x39E7);
    canvas.drawRect(245, 35, 70, 45, WHITE);
    canvas.setTextColor(WHITE);
    canvas.setCursor(275, 50);
    canvas.print("^");

    canvas.fillRect(245, 85, 70, 45, 0x39E7);
    canvas.drawRect(245, 85, 70, 45, WHITE);
    canvas.setCursor(275, 100);
    canvas.print("v");
  }

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
// Render: BT SENHA (teclado numérico)
// ---------------------------------------------------------------
void renderizarBTSenha(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);

  canvas.fillRect(5, 5, 310, 32, 0x18E3);
  canvas.drawRect(5, 5, 310, 32, WHITE);
  canvas.setTextColor(GREEN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(10, 12);
  canvas.printf("PIN: %s_", btPINBuffer.c_str());

  const char* linhas[4] = {"123", "456", "789", "0"};
  for (int l = 0; l < 4; l++) {
    int yPos = 45 + (l * 32);
    for (int c = 0; c < 3; c++) {
      int xPos = 5 + (c * 103);
      canvas.fillRect(xPos, yPos, 100, 30, 0x39E7);
      canvas.drawRect(xPos, yPos, 100, 30, WHITE);
      canvas.setTextColor(WHITE);
      canvas.setCursor(xPos + 45, yPos + 7);
      canvas.print(linhas[l][c]);
    }
  }

  canvas.fillRect(5, 180, 95, 45, ORANGE);
  canvas.drawRect(5, 180, 95, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(30, 195);
  canvas.print("< BK");

  canvas.fillRect(105, 180, 100, 45, RED);
  canvas.drawRect(105, 180, 100, 45, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(130, 195);
  canvas.print("CANCEL");

  canvas.fillRect(215, 180, 100, 45, GREEN);
  canvas.drawRect(215, 180, 100, 45, WHITE);
  canvas.setTextColor(BLACK);
  canvas.setCursor(255, 195);
  canvas.print("OK");
}

// ---------------------------------------------------------------
// Render: BT CONECTANDO
// ---------------------------------------------------------------
void renderizarBTConectando(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(60, 100);
  canvas.print("Conectando...");
}

// ---------------------------------------------------------------
// Render: BT RESULTADO
// ---------------------------------------------------------------
void renderizarBTResultado(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);
  canvas.setTextColor(btResultadoOK ? GREEN : RED);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(40, 100);
  canvas.print(btResultadoMsg);

  canvas.fillRect(20, 175, 280, 40, RED);
  canvas.drawRect(20, 175, 280, 40, WHITE);
  canvas.setTextColor(WHITE);
  canvas.setCursor(140, 188);
  canvas.print("OK");
}

// ---------------------------------------------------------------
// Render: BT MSG (tela genérica de mensagem)
// ---------------------------------------------------------------
void renderizarBTMsg(M5Canvas &canvas) {
  canvas.fillScreen(BLACK);

  canvas.setTextColor(CYAN);
  canvas.setFont(&fonts::Font2);
  canvas.setCursor(10, 10);
  canvas.print("=== AGUARDE ===");
  canvas.drawFastHLine(5, 35, 310, 0x39E7);

  // Mensagem central
  canvas.setTextColor(WHITE);
  canvas.setFont(&fonts::Font4);
  canvas.setTextDatum(textdatum_t::middle_center);
  canvas.drawString(btObterMensagem(), 160, 120);
  canvas.setTextDatum(textdatum_t::top_left);

  // Spinner
  static int angulo = 0;
  angulo = (angulo + 15) % 360;
  int cx = 160, cy = 180;
  int r = 15;
  float rad = angulo * DEG_TO_RAD;
  int x1 = cx + cos(rad) * r;
  int y1 = cy + sin(rad) * r;
  canvas.drawCircle(cx, cy, r, 0x39E7);
  canvas.drawLine(cx, cy, x1, y1, CYAN);
}