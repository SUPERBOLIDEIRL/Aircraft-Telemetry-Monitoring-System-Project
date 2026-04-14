#include "socket.h"

#include <ws2tcpip.h>
#include <iostream>

bool init_winsock()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed with error: " << result << "\n";
        return false;
    }
    return true;
}

void cleanup_winsock()
{
    WSACleanup();
}

SOCKET create_server_socket(int port)
{
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        return INVALID_SOCKET;
    }

    int yes = 1;
    setsockopt(listenSock,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&yes),
        sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_short>(port));

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "listen() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    return listenSock;
}

SOCKET accept_client(SOCKET listenSock)
{
    sockaddr_in clientAddr{};
    int addrLen = sizeof(clientAddr);

    SOCKET clientSock = accept(listenSock,
        reinterpret_cast<sockaddr*>(&clientAddr),
        &addrLen);
    if (clientSock == INVALID_SOCKET)
    {
        std::cerr << "accept() failed: " << WSAGetLastError() << "\n";
    }
    else
    {
        char ipStr[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
        std::cout << "Accepted connection from " << ipStr
            << ":" << ntohs(clientAddr.sin_port) << "\n";
    }

    return clientSock;
}


SOCKET create_client_socket(const std::string& host, int port)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string portStr = std::to_string(port);
    addrinfo* result = nullptr;

    int rv = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (rv != 0)
    {
        std::cerr << "getaddrinfo() failed: " << gai_strerrorA(rv) << "\n";
        return INVALID_SOCKET;
    }

    SOCKET sock = INVALID_SOCKET;

    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET)
            continue;

        if (connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0)
            break;

        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (sock == INVALID_SOCKET)
        std::cerr << "Could not connect to " << host << ":" << port << "\n";

    return sock;
}

void close_socket(SOCKET sock)
{
    if (sock != INVALID_SOCKET)
        closesocket(sock);
}
