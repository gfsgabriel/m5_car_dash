#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

#define MAX_REDES_SALVAS 5

struct RedeWifiItem {
  String ssid;
  int32_t rssi;
  bool salva;
};

// Inicialização / NVS
void inicializarWifiManager();          // carrega NVS + tenta conectar UMA vez (sem retry)
void carregarRedesSalvas();
void salvarNovaRede(String ssid, String senha);
void esquecerRedeSalva(int idx);
String buscarSenhaSalva(String ssid);
int obterQtdRedesSalvas();
String obterSSIDSalvo(int idx);

// Scan
void iniciarScanWifiAsync();
bool wifiScanConcluido();
int obterTotalRedesEscaneadas();
RedeWifiItem obterRedePorIndice(int index);

// Conexão
void iniciarConexaoDireta(String ssid, String senha);
bool wifiEstaConectado();
String obterIPAtual();
String obterSSIDAtual();

#endif