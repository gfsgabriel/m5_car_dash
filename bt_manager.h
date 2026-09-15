#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#include <Arduino.h>

#define PIN_XM_RX   18
#define PIN_XM_TX   17
#define XM_BAUD     9600
#define MAX_BT_DEV  15

struct DispositivoBT {
  String mac;
  String nome;
  int8_t rssi;
};

enum EstadoScanBT {
  SCAN_BT_IDLE,
  SCAN_BT_ENVIADO,
  SCAN_BT_AGUARDANDO,
  SCAN_BT_CONCLUIDO
};

enum EstadoBT {
  BT_LIVRE,
  BT_MENU_ATIVO,
  BT_OBD_ATIVO
};

enum DonoAT {
  DONO_MENU,
  DONO_BT_TERMINAL,
  DONO_OBD
};

// Inicialização
void inicializarBTManager();
void atualizarBTManager();

// Scan (não-bloqueante via fila)
void iniciarScanBT();
bool scanBTConcluido();
bool scanBTTimedOut();
int obterTotalDispositivosBT();
DispositivoBT obterDispositivoBTPorIndice(int idx);

// Solicitação de scan (chamada pelo menu)
extern volatile bool btScanPendente;
void btSolicitarScan();

// Fase 2: busca de nomes
void iniciarBuscaNomes();
bool buscaNomesConcluida();

// Conexão (bloqueante)
bool parearComBT(String mac, String pin);
bool conectarBT(String mac);
bool desconectarBT();
bool btEstaConectado();
String obterNomeConectado();

// NVS
String obterMACSalvo();
String obterPINsalvo();
void salvarMAC(String mac, String pin);
void esquecerMAC();

// Controle de posse (mutex lógico)
bool btSolicitarUsoMenu();
void btLiberarUsoMenu();
bool btSolicitarUsoOBD();
void btLiberarUsoOBD();
bool btEstaDisponivel();
String btObterDonoAtual();

// Mensagem genérica (pra UI)
void btSetMensagem(String msg);
String btObterMensagem();

// Debug / util
bool xmEstaRespondendo();
String enviarAT(String cmd, uint32_t timeout = 1000);

// API pública pra fila (usada pelo /bt e OBD)
uint32_t btEnviarATPublico(String cmd, uint32_t timeout, DonoAT dono);
bool btObterRespostaPublico(uint32_t id, String &resp, bool &ok, DonoAT dono);

// Sniffer de debug raw (mostra tudo que o XM manda)
void btIniciarDebugRaw();
void btPararDebugRaw();
bool btDebugRawAtivo();
String btObterDebugRaw();

#endif