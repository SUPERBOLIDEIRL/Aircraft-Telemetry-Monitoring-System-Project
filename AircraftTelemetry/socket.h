#pragma once

#include <winsock2.h>
#include <string>

bool init_winsock();
void cleanup_winsock();
SOCKET create_server_socket(int port);
SOCKET accept_client(SOCKET listenSock);
SOCKET create_client_socket(const std::string& host, int port);
void close_socket(SOCKET sock);