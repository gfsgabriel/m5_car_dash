#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>

// Inicializa o WebServer + WebSocket
void inicializarWebServer();

// Loop: drena fila de logs + envia telemetria 20 Hz
void atualizarWebSocket();

// API pública de log (chame de qualquer lugar)
void registrarLogWebServer(String linhaLog);

// Cache dos HTMLs em PSRAM (chamar ANTES de inicializarWebServer)
void carregarHtmlsParaRam();

#endif