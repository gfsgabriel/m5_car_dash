#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>

// Inicializa a rede Wi-Fi, configura os endpoints REST e liga o WebSocket
void inicializarWebServer();

// Executa em loop para limpar conexões mortas e disparar o broadcast de dados
void gerenciarWebServer();

#endif
