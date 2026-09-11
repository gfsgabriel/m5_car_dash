#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>

void inicializarWebServer();
void gerenciarWebServer();

// 🌟 Novas funções para o display manager ler o banco de logs centralizado do WebServer
String obterLinhaLogWeb(int indice);
int obterTotalLogsWeb();

#endif
