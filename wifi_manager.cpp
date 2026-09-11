#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;
static bool escaneando = false;
static int totalRedes = 0;

// Lista de redes salvas (carregada da NVS)
static String ssidsSalvos[MAX_REDES_SALVAS];
static String senhasSalvas[MAX_REDES_SALVAS];
static int qtdRedesSalvas = 0;

// ---------------------------------------------------------------
// Inicialização
// ---------------------------------------------------------------
void carregarRedesSalvas() {
  prefs.begin("wifi", true);
  qtdRedesSalvas = prefs.getInt("qtd", 0);
  if (qtdRedesSalvas > MAX_REDES_SALVAS) qtdRedesSalvas = MAX_REDES_SALVAS;
  if (qtdRedesSalvas < 0) qtdRedesSalvas = 0;

  for (int i = 0; i < qtdRedesSalvas; i++) {
    ssidsSalvos[i]  = prefs.getString(("s_" + String(i)).c_str(), "");
    senhasSalvas[i] = prefs.getString(("p_" + String(i)).c_str(), "");
  }
  prefs.end();
}

void inicializarWifiManager() {
  prefs.begin("wifi", false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false);   // sem retry automático
  WiFi.persistent(false);         // não grava credenciais automaticamente na NVS do WiFi
  carregarRedesSalvas();

  // Tenta conectar em UMA rede salva, sem retry, sem bloquear
  for (int i = 0; i < qtdRedesSalvas; i++) {
    if (ssidsSalvos[i].length() > 0) {
      WiFi.begin(ssidsSalvos[i].c_str(), senhasSalvas[i].c_str());
      break;   // só a primeira da lista
    }
  }
}

// ---------------------------------------------------------------
// NVS
// ---------------------------------------------------------------
String buscarSenhaSalva(String ssid) {
  for (int i = 0; i < qtdRedesSalvas; i++) {
    if (ssidsSalvos[i] == ssid) return senhasSalvas[i];
  }
  return "";
}

void salvarNovaRede(String ssid, String senha) {
  if (ssid.length() == 0) return;

  // Já existe? só atualiza senha
  for (int i = 0; i < qtdRedesSalvas; i++) {
    if (ssidsSalvos[i] == ssid) {
      senhasSalvas[i] = senha;
      prefs.begin("wifi", false);
      prefs.putString(("p_" + String(i)).c_str(), senha);
      prefs.end();
      return;
    }
  }
  // Nova
  if (qtdRedesSalvas < MAX_REDES_SALVAS) {
    ssidsSalvos[qtdRedesSalvas]  = ssid;
    senhasSalvas[qtdRedesSalvas] = senha;
    prefs.begin("wifi", false);
    prefs.putString(("s_" + String(qtdRedesSalvas)).c_str(), ssid);
    prefs.putString(("p_" + String(qtdRedesSalvas)).c_str(), senha);
    qtdRedesSalvas++;
    prefs.putInt("qtd", qtdRedesSalvas);
    prefs.end();
  }
}

void esquecerRedeSalva(int idx) {
  if (idx < 0 || idx >= qtdRedesSalvas) return;

  prefs.begin("wifi", false);
  for (int i = idx; i < qtdRedesSalvas - 1; i++) {
    ssidsSalvos[i]  = ssidsSalvos[i + 1];
    senhasSalvas[i] = senhasSalvas[i + 1];
    prefs.putString(("s_" + String(i)).c_str(), ssidsSalvos[i]);
    prefs.putString(("p_" + String(i)).c_str(), senhasSalvas[i]);
  }
  qtdRedesSalvas--;
  prefs.remove(("s_" + String(qtdRedesSalvas)).c_str());
  prefs.remove(("p_" + String(qtdRedesSalvas)).c_str());
  prefs.putInt("qtd", qtdRedesSalvas);
  prefs.end();
}

int obterQtdRedesSalvas() { return qtdRedesSalvas; }

String obterSSIDSalvo(int idx) {
  if (idx < 0 || idx >= qtdRedesSalvas) return "";
  return ssidsSalvos[idx];
}

// ---------------------------------------------------------------
// Scan
// ---------------------------------------------------------------
void iniciarScanWifiAsync() {
  if (!escaneando) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    escaneando = true;
  }
}

bool wifiScanConcluido() {
  if (!escaneando) return true;
  int n = WiFi.scanComplete();
  if (n >= 0) {
    totalRedes = n;
    escaneando = false;
    return true;
  }
  return false;
}

int obterTotalRedesEscaneadas() { return totalRedes; }

RedeWifiItem obterRedePorIndice(int index) {
  RedeWifiItem item;
  if (index >= 0 && index < totalRedes) {
    item.ssid  = WiFi.SSID(index);
    item.rssi  = WiFi.RSSI(index);
    item.salva = (buscarSenhaSalva(item.ssid).length() > 0);
  } else {
    item.ssid  = "N/A";
    item.rssi  = 0;
    item.salva = false;
  }
  return item;
}

// ---------------------------------------------------------------
// Conexão
// ---------------------------------------------------------------
void iniciarConexaoDireta(String ssid, String senha) {
  WiFi.disconnect(true);
  delay(50);
  WiFi.begin(ssid.c_str(), senha.c_str());
  // Sem retry — quem chama decide o que fazer se falhar
}

bool wifiEstaConectado() {
  return WiFi.status() == WL_CONNECTED;
}

String obterIPAtual() {
  if (wifiEstaConectado()) return WiFi.localIP().toString();
  return "Desconectado";
}

String obterSSIDAtual() {
  if (wifiEstaConectado()) return WiFi.SSID();
  return "";
}