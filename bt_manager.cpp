#include "bt_manager.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static Preferences prefsBT;
static HardwareSerial XMSerial(1);

static DispositivoBT listaBT[MAX_BT_DEV];
static int totalBT = 0;
static bool conectadoBT = false;
static String nomeConectado = "";
static String mensagemGenerica = "";

// ---------------------------------------------------------------
// Fila (com dono)
// ---------------------------------------------------------------
struct ComandoAT {
  char cmd[64];
  uint32_t timeout;
  uint32_t id;
  DonoAT dono;
};

struct RespostaAT {
  uint32_t id;
  char resp[512];
  size_t len;
  bool ok;
};

static QueueHandle_t xFilaComandosAT = NULL;
static QueueHandle_t xFilaRespMenu   = NULL;
static QueueHandle_t xFilaRespBT     = NULL;
static QueueHandle_t xFilaRespOBD    = NULL;
static uint32_t proximoID = 1;

// ---------------------------------------------------------------
// Controle de posse
// ---------------------------------------------------------------
static EstadoBT estadoBT = BT_LIVRE;
static unsigned long tempoPosse = 0;
static const unsigned long TIMEOUT_POSSE_MS = 15000;

// ---------------------------------------------------------------
// Sniffer de debug raw
// ---------------------------------------------------------------
#define BT_DEBUG_RAW_SIZE 1024
static char btDebugRaw[BT_DEBUG_RAW_SIZE];
static size_t btDebugRawLen = 0;
static SemaphoreHandle_t btDebugRawMutex = NULL;
static volatile bool _btDebugRawAtivo = false;

void btIniciarDebugRaw() {
  if (btDebugRawMutex == NULL) {
    btDebugRawMutex = xSemaphoreCreateMutex();
  }
  _btDebugRawAtivo = true;
  btDebugRawLen = 0;
  btDebugRaw[0] = '\0';
  Serial.println("BT: debug raw ATIVADO");
}

void btPararDebugRaw() {
  _btDebugRawAtivo = false;
  btDebugRawLen = 0;
  btDebugRaw[0] = '\0';
  Serial.println("BT: debug raw DESATIVADO");
}

bool btDebugRawAtivo() {
  return _btDebugRawAtivo;
}

String btObterDebugRaw() {
  if (btDebugRawMutex == NULL) return "";

  String resultado = "";
  if (xSemaphoreTake(btDebugRawMutex, portMAX_DELAY) == pdTRUE) {
    if (btDebugRawLen > 0) {
      resultado = String(btDebugRaw);
      btDebugRawLen = 0;
      btDebugRaw[0] = '\0';
    }
    xSemaphoreGive(btDebugRawMutex);
  }
  return resultado;
}

// ---------------------------------------------------------------
// Mensagem genérica
// ---------------------------------------------------------------
void btSetMensagem(String msg) {
  mensagemGenerica = msg;
  Serial.printf("BT: mensagem = [%s]\n", msg.c_str());
}

String btObterMensagem() {
  return mensagemGenerica;
}

// ---------------------------------------------------------------
// Task do XM (Core 0)
// ---------------------------------------------------------------
void vTaskXM(void *pvParameters) {
  bool comandoEmCurso = false;
  ComandoAT cmdAtual;
  char respBuffer[512];
  size_t respLen = 0;
  unsigned long inicioComando = 0;
  unsigned long ultimoCrescimento = 0;

  Serial.println("BT[task]: iniciada");

  for (;;) {
    if (!comandoEmCurso) {
      ComandoAT novo;
      if (xQueueReceive(xFilaComandosAT, &novo, 0) == pdTRUE) {
        while (XMSerial.available()) XMSerial.read();

        Serial.printf("\nBT[task]: === TX [%s] (timeout %u ms, dono=%d) ===\n",
                      novo.cmd, novo.timeout, novo.dono);
        XMSerial.print(novo.cmd);
        XMSerial.print("\r\n");

        cmdAtual = novo;
        respLen = 0;
        respBuffer[0] = '\0';
        inicioComando = millis();
        ultimoCrescimento = millis();
        comandoEmCurso = true;
      }
    }

    if (comandoEmCurso) {
      size_t tamAntes = respLen;

      while (XMSerial.available() && respLen < 510) {
        char c = XMSerial.read();
        respBuffer[respLen++] = c;
        respBuffer[respLen] = '\0';

        // Sniffer: copia cada byte
        if (_btDebugRawAtivo && btDebugRawMutex != NULL) {
          if (xSemaphoreTake(btDebugRawMutex, 0) == pdTRUE) {
            if (btDebugRawLen < BT_DEBUG_RAW_SIZE - 1) {
              btDebugRaw[btDebugRawLen++] = c;
              btDebugRaw[btDebugRawLen] = '\0';
            }
            xSemaphoreGive(btDebugRawMutex);
          }
        }
      }

      // Descarta excesso
      while (XMSerial.available() && respLen >= 510) {
        char c = XMSerial.read();
        if (_btDebugRawAtivo && btDebugRawMutex != NULL) {
          if (xSemaphoreTake(btDebugRawMutex, 0) == pdTRUE) {
            if (btDebugRawLen < BT_DEBUG_RAW_SIZE - 1) {
              btDebugRaw[btDebugRawLen++] = c;
              btDebugRaw[btDebugRawLen] = '\0';
            }
            xSemaphoreGive(btDebugRawMutex);
          }
        }
      }

      if (respLen > tamAntes) {
        ultimoCrescimento = millis();
      }

      String tmp(respBuffer);
      bool viuOK = tmp.endsWith("OK\r\n") || tmp.endsWith("OK\r");
      bool viuErro = tmp.endsWith("ERROR\r\n") || tmp.endsWith("ERROR\r");
      bool viuComplete = (tmp.indexOf("+INQ:COMPLETE") >= 0);

      bool ehInquiry = (strstr(cmdAtual.cmd, "AT+INQ") != NULL);
      unsigned long tempoInatividade = ehInquiry ? 5000 : 1500;
      bool inatividade = (respLen > 0 && millis() - ultimoCrescimento >= tempoInatividade);
      bool timeout = (millis() - inicioComando >= cmdAtual.timeout);

      bool respostaCompleta = viuOK || viuErro || viuComplete || inatividade || timeout;

      static unsigned long ultimoLogParcial = 0;
      if (millis() - ultimoLogParcial >= 500 && respLen > 0) {
        ultimoLogParcial = millis();
        Serial.printf("BT[task]: ... RX parcial [%d bytes] em %lu ms\n",
                      (int)respLen, millis() - inicioComando);
      }

      if (respostaCompleta) {
        Serial.printf("\nBT[task]: === RX COMPLETA ===\n");
        Serial.printf("BT[task]: tamanho=%d bytes, tempo=%lu ms\n",
                      (int)respLen, millis() - inicioComando);
        Serial.printf("BT[task]: flags: OK=%d ERR=%d COMPLETE=%d INA=%d TO=%d\n",
                      viuOK, viuErro, viuComplete, inatividade, timeout);

        Serial.printf("BT[task]: resposta: [");
        for (size_t i = 0; i < respLen; i++) {
          uint8_t b = (uint8_t)respBuffer[i];
          if (b >= 32 && b < 127) Serial.printf("%c", b);
          else Serial.printf(".");
        }
        Serial.println("]");

        RespostaAT r;
        r.id = cmdAtual.id;
        memcpy(r.resp, respBuffer, respLen);
        r.resp[respLen] = '\0';
        r.len = respLen;
        r.ok = (viuOK || viuErro || viuComplete || (respLen > 0));

        switch (cmdAtual.dono) {
          case DONO_MENU:
            xQueueSend(xFilaRespMenu, &r, 0);
            break;
          case DONO_BT_TERMINAL:
            xQueueSend(xFilaRespBT, &r, 0);
            break;
          case DONO_OBD:
            xQueueSend(xFilaRespOBD, &r, 0);
            break;
        }

        comandoEmCurso = false;
        respLen = 0;
        ultimoLogParcial = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ---------------------------------------------------------------
// Envia AT não-bloqueante (com dono)
// ---------------------------------------------------------------
static uint32_t btEnviarAT(String cmd, uint32_t timeout, DonoAT dono) {
  if (xFilaComandosAT == NULL) return 0;

  ComandoAT c;
  strncpy(c.cmd, cmd.c_str(), sizeof(c.cmd) - 1);
  c.cmd[sizeof(c.cmd) - 1] = '\0';
  c.timeout = timeout;
  c.id = proximoID++;
  c.dono = dono;

  if (xQueueSend(xFilaComandosAT, &c, 0) != pdTRUE) {
    Serial.println("BT: fila de comandos cheia!");
    return 0;
  }
  return c.id;
}

// ---------------------------------------------------------------
// Tenta pegar resposta (com dono)
// ---------------------------------------------------------------
static bool btObterResposta(uint32_t id, String &resp, bool &ok, DonoAT dono) {
  QueueHandle_t fila;
  switch (dono) {
    case DONO_MENU:        fila = xFilaRespMenu; break;
    case DONO_BT_TERMINAL: fila = xFilaRespBT;   break;
    case DONO_OBD:         fila = xFilaRespOBD;  break;
    default: return false;
  }
  if (fila == NULL) return false;

  RespostaAT r;
  if (xQueueReceive(fila, &r, 0) != pdTRUE) return false;

  if (r.id == id) {
    resp = String(r.resp);
    ok = r.ok;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------
// API pública
// ---------------------------------------------------------------
uint32_t btEnviarATPublico(String cmd, uint32_t timeout, DonoAT dono) {
  return btEnviarAT(cmd, timeout, dono);
}

bool btObterRespostaPublico(uint32_t id, String &resp, bool &ok, DonoAT dono) {
  return btObterResposta(id, resp, ok, dono);
}

// ---------------------------------------------------------------
// enviarAT bloqueante
// ---------------------------------------------------------------
String enviarAT(String cmd, uint32_t timeout) {
  while (XMSerial.available()) XMSerial.read();

  Serial.printf("BT: TX [%s]\n", cmd.c_str());
  XMSerial.print(cmd);
  XMSerial.print("\r\n");

  String resp = "";
  unsigned long inicio = millis();
  unsigned long ultimoByte = 0;
  bool logouAlgo = false;

  while (millis() - inicio < timeout) {
    while (XMSerial.available()) {
      char c = XMSerial.read();
      resp += c;
      ultimoByte = millis();

      if (!logouAlgo) {
        Serial.println("BT: bytes recebidos:");
        logouAlgo = true;
      }
      Serial.printf("  0x%02X ('%c')\n",
                    (uint8_t)c, (c >= 32 && c < 127) ? c : '.');
    }

    if (resp.endsWith("OK\r\n") || resp.endsWith("OK\r") || resp.indexOf("ERROR") >= 0) {
      Serial.printf("BT: RX [%s]\n", resp.c_str());
      return resp;
    }

    if (resp.length() > 0 && millis() - ultimoByte >= 2000) {
      Serial.printf("BT: RX (inatividade 2s) [%s]\n", resp.c_str());
      return resp;
    }

    delay(5);
  }
  Serial.printf("BT: RX (timeout %u ms) [%s]\n", timeout, resp.c_str());
  return resp;
}

bool xmEstaRespondendo() {
  String r = enviarAT("AT", 500);
  return r.indexOf("OK") >= 0;
}

// ---------------------------------------------------------------
// NVS
// ---------------------------------------------------------------
String obterMACSalvo() {
  prefsBT.begin("bt_store", true);
  String mac = prefsBT.getString("mac", "");
  prefsBT.end();
  return mac;
}

String obterPINsalvo() {
  prefsBT.begin("bt_store", true);
  String pin = prefsBT.getString("pin", "1234");
  prefsBT.end();
  return pin;
}

void salvarMAC(String mac, String pin) {
  prefsBT.begin("bt_store", false);
  prefsBT.putString("mac", mac);
  prefsBT.putString("pin", pin);
  prefsBT.end();
}

void esquecerMAC() {
  prefsBT.begin("bt_store", false);
  prefsBT.remove("mac");
  prefsBT.remove("pin");
  prefsBT.end();
}

// ---------------------------------------------------------------
// Controle de posse
// ---------------------------------------------------------------
bool btSolicitarUsoMenu() {
  if (estadoBT == BT_LIVRE) {
    estadoBT = BT_MENU_ATIVO;
    tempoPosse = millis();
    return true;
  }
  if (estadoBT == BT_OBD_ATIVO && millis() - tempoPosse >= TIMEOUT_POSSE_MS) {
    Serial.println("BT: forcando liberacao (OBD travou)");
    estadoBT = BT_MENU_ATIVO;
    tempoPosse = millis();
    return true;
  }
  return false;
}

void btLiberarUsoMenu() {
  if (estadoBT == BT_MENU_ATIVO) estadoBT = BT_LIVRE;
}

bool btSolicitarUsoOBD() {
  if (estadoBT == BT_LIVRE) {
    estadoBT = BT_OBD_ATIVO;
    tempoPosse = millis();
    return true;
  }
  if (estadoBT == BT_MENU_ATIVO && millis() - tempoPosse >= TIMEOUT_POSSE_MS) {
    Serial.println("BT: forcando liberacao (menu travou)");
    estadoBT = BT_OBD_ATIVO;
    tempoPosse = millis();
    return true;
  }
  return false;
}

void btLiberarUsoOBD() {
  if (estadoBT == BT_OBD_ATIVO) estadoBT = BT_LIVRE;
}

bool btEstaDisponivel() {
  return (estadoBT == BT_LIVRE);
}

String btObterDonoAtual() {
  switch (estadoBT) {
    case BT_LIVRE:      return "LIVRE";
    case BT_MENU_ATIVO: return "MENU";
    case BT_OBD_ATIVO:  return "OBD";
  }
  return "?";
}

// ---------------------------------------------------------------
// Inicialização
// ---------------------------------------------------------------
void inicializarBTManager() {
  Serial.println("BT: inicializando XM-15B...");

  xFilaComandosAT = xQueueCreate(10, sizeof(ComandoAT));
  xFilaRespMenu   = xQueueCreate(5, sizeof(RespostaAT));
  xFilaRespBT     = xQueueCreate(5, sizeof(RespostaAT));
  xFilaRespOBD    = xQueueCreate(5, sizeof(RespostaAT));

  btDebugRawMutex = xSemaphoreCreateMutex();

  XMSerial.begin(XM_BAUD, SERIAL_8N1, PIN_XM_RX, PIN_XM_TX);
  delay(500);

  if (!xmEstaRespondendo()) {
    Serial.println("BT: XM-15B NAO respondeu. Verifique fiacao.");
    return;
  }
  Serial.println("BT: XM-15B OK");

  enviarAT("AT+ROLE=1", 1000);

  xTaskCreatePinnedToCore(vTaskXM, "TaskXM", 4096, NULL, 1, NULL, 0);
  Serial.println("BT: task do XM iniciada");

  String mac = obterMACSalvo();
  if (mac.length() > 0) {
    Serial.printf("BT: auto-conectando em %s\n", mac.c_str());
    enviarAT("AT+CMODE=0", 1000);
    enviarAT("AT+BIND=" + mac, 1000);
    String r = enviarAT("AT+LINK=" + mac, 3000);
    if (r.indexOf("OK") >= 0) {
      conectadoBT = true;
      nomeConectado = mac;
      Serial.println("BT: conectado!");
    }
  }
}

// ---------------------------------------------------------------
// Scan
// ---------------------------------------------------------------
static EstadoScanBT estadoScan = SCAN_BT_IDLE;
static uint32_t idScanEnviado = 0;
static unsigned long scanInicioMs = 0;
static String scanBuffer;
static const unsigned long SCAN_TIMEOUT_MS = 15000;

volatile bool btScanPendente = false;
static unsigned long ultimoScanMs = 0;
static const unsigned long COOLDOWN_SCAN_MS = 2000;

void btSolicitarScan() {
  btScanPendente = true;
}

void iniciarScanBT() {
  if (estadoScan != SCAN_BT_IDLE && estadoScan != SCAN_BT_CONCLUIDO) {
    Serial.println("BT: scan ja em andamento");
    return;
  }

  if (millis() - ultimoScanMs < COOLDOWN_SCAN_MS) {
    Serial.println("BT: aguarde antes de escanear de novo");
    return;
  }
  ultimoScanMs = millis();

  if (!btSolicitarUsoMenu()) {
    Serial.println("BT: ocupado, nao posso escanear agora");
    return;
  }

  Serial.println("\nBT: ================================");
  Serial.println("BT: iniciando scan...");
  Serial.println("BT: ================================");

  totalBT = 0;
  scanBuffer = "";
  scanBuffer.reserve(2048);
  scanInicioMs = millis();

  while (XMSerial.available()) XMSerial.read();

  Serial.println("BT: AT+CMODE=1...");
  XMSerial.print("AT+CMODE=1\r\n");
  delay(150);
  String respCMODE = "";
  while (XMSerial.available()) respCMODE += (char)XMSerial.read();
  Serial.printf("BT: resposta CMODE: [%s]\n", respCMODE.c_str());

  idScanEnviado = btEnviarAT("AT+INQ", 15000, DONO_MENU);
  estadoScan = SCAN_BT_ENVIADO;
  Serial.printf("BT: AT+INQ enfileirado (id=%u)\n", idScanEnviado);
}

static void processarScanBT() {
  if (estadoScan == SCAN_BT_IDLE || estadoScan == SCAN_BT_CONCLUIDO) return;

  if (estadoScan == SCAN_BT_ENVIADO) {
    String resp;
    bool ok;

    if (btObterResposta(idScanEnviado, resp, ok, DONO_MENU)) {
      scanBuffer = resp;
      Serial.printf("\nBT: === resposta INQ recebida (%d bytes) ===\n", resp.length());

      totalBT = 0;
      int pos = 0;
      while (pos < (int)scanBuffer.length() && totalBT < MAX_BT_DEV) {
        int ini = scanBuffer.indexOf("+INQ:", pos);
        if (ini < 0) break;

        int fim = scanBuffer.indexOf("\r", ini);
        if (fim < 0) fim = scanBuffer.length();

        String linha = scanBuffer.substring(ini + 5, fim);
        linha.trim();

        if (linha.startsWith("COMPLETE")) {
          pos = fim + 1;
          continue;
        }

        int v1 = linha.indexOf(',');
        int v2 = linha.indexOf(',', v1 + 1);

        if (v1 > 0 && v2 > 0) {
          String mac = linha.substring(0, v1);
          String rssiStr = linha.substring(v2 + 1);
          mac.replace(":", "");

          listaBT[totalBT].mac = mac;
          String macCurto = mac.substring(mac.length() - 6);
          listaBT[totalBT].nome = "..." + macCurto;
          listaBT[totalBT].rssi = (int8_t)rssiStr.toInt();
          totalBT++;

          Serial.printf("BT: [%d] MAC=%s RSSI=%d\n",
                        totalBT - 1, mac.c_str(), listaBT[totalBT-1].rssi);
        }
        pos = fim + 1;
      }

      Serial.printf("BT: scan concluido, %d dispositivos\n", totalBT);
      scanBuffer = "";
      estadoScan = SCAN_BT_CONCLUIDO;
      btLiberarUsoMenu();
      iniciarBuscaNomes();
      return;
    }

    if (millis() - scanInicioMs >= SCAN_TIMEOUT_MS) {
      Serial.println("BT: timeout esperando resposta do INQ");
      totalBT = 0;
      estadoScan = SCAN_BT_CONCLUIDO;
      btLiberarUsoMenu();
    }
    return;
  }
}

bool scanBTConcluido() {
  return (estadoScan == SCAN_BT_CONCLUIDO || estadoScan == SCAN_BT_IDLE);
}

bool scanBTTimedOut() {
  return false;
}

int obterTotalDispositivosBT() { return totalBT; }

DispositivoBT obterDispositivoBTPorIndice(int idx) {
  if (idx < 0 || idx >= totalBT) {
    DispositivoBT vazio;
    vazio.mac = "";
    vazio.nome = "N/A";
    vazio.rssi = 0;
    return vazio;
  }
  return listaBT[idx];
}

// ---------------------------------------------------------------
// Fase 2: busca de nomes
// ---------------------------------------------------------------
static int idxNomeAtual = -1;
static uint32_t idNomeEnviado = 0;
static unsigned long nomeInicioMs = 0;
static bool nomesConcluidos = true;
static unsigned long nomesInicioMs = 0;
static bool nomesAguardando = false;
static int tentativasRNAME = 0;
static const int MAX_TENTATIVAS_RNAME = 2;
static const unsigned long RNAME_TIMEOUT_MS = 2000;
static const unsigned long RNAME_DELAY_MS = 500;

void iniciarBuscaNomes() {
  if (totalBT == 0) {
    nomesConcluidos = true;
    return;
  }
  idxNomeAtual = 0;
  idNomeEnviado = 0;
  nomesConcluidos = false;
  nomesInicioMs = millis();
  nomesAguardando = true;
  tentativasRNAME = 0;
  Serial.printf("\nBT: === iniciando busca de nomes (%d devices) ===\n", totalBT);
}

bool buscaNomesConcluida() {
  return nomesConcluidos;
}

static void processarBuscaNomes() {
  if (nomesConcluidos) return;

  if (nomesAguardando) {
    if (millis() - nomesInicioMs < RNAME_DELAY_MS) return;
    nomesAguardando = false;
    Serial.println("BT: comecando a pedir nomes...");
  }

  if (idxNomeAtual < 0 || idxNomeAtual >= totalBT) {
    nomesConcluidos = true;
    Serial.println("BT: busca de nomes concluida");
    return;
  }

  if (idNomeEnviado == 0) {
    String cmd = "AT+RNAME?" + listaBT[idxNomeAtual].mac;
    idNomeEnviado = btEnviarAT(cmd, RNAME_TIMEOUT_MS, DONO_MENU);
    nomeInicioMs = millis();
    Serial.printf("BT: pedindo nome de [%d] %s (tentativa %d)\n",
                  idxNomeAtual, listaBT[idxNomeAtual].mac.c_str(), tentativasRNAME + 1);
    return;
  }

  String resp;
  bool ok;
  if (btObterResposta(idNomeEnviado, resp, ok, DONO_MENU)) {
    bool achou = false;
    int posR = resp.indexOf("+RNAME:");
    if (posR >= 0) {
      int fimR = resp.indexOf("\r", posR);
      if (fimR < 0) fimR = resp.length();
      String nome = resp.substring(posR + 7, fimR);
      nome.trim();
      if (nome.length() > 0 && nome != "ERROR") {
        listaBT[idxNomeAtual].nome = nome;
        Serial.printf("BT: [%d] nome = %s\n", idxNomeAtual, nome.c_str());
        achou = true;
      }
    }

    if (achou) {
      idxNomeAtual++;
      idNomeEnviado = 0;
      tentativasRNAME = 0;
    } else {
      tentativasRNAME++;
      idNomeEnviado = 0;
      if (tentativasRNAME >= MAX_TENTATIVAS_RNAME) {
        Serial.printf("BT: [%d] desistindo (sem nome apos %d tentativas)\n",
                      idxNomeAtual, tentativasRNAME);
        idxNomeAtual++;
        tentativasRNAME = 0;
      }
    }
    return;
  }

  if (millis() - nomeInicioMs >= RNAME_TIMEOUT_MS + 500) {
    tentativasRNAME++;
    idNomeEnviado = 0;
    if (tentativasRNAME >= MAX_TENTATIVAS_RNAME) {
      Serial.printf("BT: [%d] timeout apos %d tentativas, desistindo\n",
                    idxNomeAtual, tentativasRNAME);
      idxNomeAtual++;
      tentativasRNAME = 0;
    } else {
      Serial.printf("BT: [%d] timeout, tentando de novo (%d/%d)\n",
                    idxNomeAtual, tentativasRNAME, MAX_TENTATIVAS_RNAME);
    }
  }
}

// ---------------------------------------------------------------
// Conexão
// ---------------------------------------------------------------
bool parearComBT(String mac, String pin) {
  if (!btSolicitarUsoMenu()) {
    Serial.println("BT: ocupado, nao posso parear");
    return false;
  }

  Serial.printf("\nBT: ====================================\n");
  Serial.printf("BT: pareando com %s (PIN %s)\n", mac.c_str(), pin.c_str());
  Serial.printf("BT: ====================================\n");

  // 1. Salva a senha (caso o ELM peça PIN)
  Serial.println("BT: AT+PSWD=" + pin);
  enviarAT("AT+PSWD=" + pin, 2000);

  // 2. Modo "MAC específico"
  Serial.println("BT: AT+CMODE=0");
  enviarAT("AT+CMODE=0", 2000);

  // 3. Salva o MAC
  Serial.println("BT: AT+BIND=" + mac);
  String rBIND = enviarAT("AT+BIND=" + mac, 2000);
  Serial.printf("BT: resposta BIND: [%s]\n", rBIND.c_str());

  if (rBIND.indexOf("OK") < 0) {
    Serial.println("BT: falha no BIND");
    btLiberarUsoMenu();
    return false;
  }

  // 4. Conecta (faz o pareamento implícito)
  Serial.println("BT: AT+LINK=" + mac);
  String rLINK = enviarAT("AT+LINK=" + mac, 15000);
  Serial.printf("BT: resposta LINK: [%s]\n", rLINK.c_str());

  if (rLINK.indexOf("OK") < 0) {
    Serial.println("BT: falha no LINK");
    btLiberarUsoMenu();
    return false;
  }

  // 5. Salva o MAC na NVS do M5
  salvarMAC(mac, pin);
  Serial.println("BT: MAC salvo na NVS");

  conectadoBT = true;
  nomeConectado = mac;

  btLiberarUsoMenu();
  return true;
}

bool conectarBT(String mac) {
  if (!btSolicitarUsoMenu()) {
    Serial.println("BT: ocupado, nao posso conectar");
    return false;
  }

  Serial.printf("\nBT: ====================================\n");
  Serial.printf("BT: conectando em %s\n", mac.c_str());
  Serial.printf("BT: ====================================\n");

  Serial.println("BT: AT+LINK=" + mac);
  String r = enviarAT("AT+LINK=" + mac, 10000);
  Serial.printf("BT: resposta LINK: [%s]\n", r.c_str());

  btLiberarUsoMenu();

  if (r.indexOf("OK") >= 0) {
    conectadoBT = true;
    nomeConectado = mac;
    Serial.println("BT: conectado!");
    return true;
  }

  Serial.println("BT: falha ao conectar");
  return false;
}

bool desconectarBT() {
  if (!btSolicitarUsoMenu()) return false;
  Serial.println("BT: AT+DISC");
  enviarAT("AT+DISC", 3000);
  btLiberarUsoMenu();
  conectadoBT = false;
  nomeConectado = "";
  return true;
}

bool btEstaConectado() { return conectadoBT; }
String obterNomeConectado() { return nomeConectado; }

// ---------------------------------------------------------------
// Loop
// ---------------------------------------------------------------
static unsigned long ultimoRetry = 0;
static const unsigned long INTERVALO_RETRY_MS = 10000;

void atualizarBTManager() {
  if (estadoScan != SCAN_BT_IDLE && estadoScan != SCAN_BT_CONCLUIDO) {
    processarScanBT();
    return;
  }

  if (!buscaNomesConcluida()) {
    processarBuscaNomes();
    return;
  }

  if (conectadoBT) return;
  String mac = obterMACSalvo();
  if (mac.length() == 0) return;

  if (millis() - ultimoRetry >= INTERVALO_RETRY_MS) {
    ultimoRetry = millis();
    if (!btSolicitarUsoOBD()) return;
    Serial.println("BT: tentando reconectar...");
    String r = enviarAT("AT+LINK=" + mac, 5000);
    btLiberarUsoOBD();
    if (r.indexOf("OK") >= 0) {
      conectadoBT = true;
      nomeConectado = mac;
      Serial.println("BT: reconectado!");
    }
  }
}