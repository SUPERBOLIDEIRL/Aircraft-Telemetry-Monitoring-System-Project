#include "socket.h"

#include <ws2tcpip.h>
#include <iostream>

// MISRA-CPP-2008-7-3-1: all socket helpers in a namespace
namespace SocketLib
{

bool init_winsock()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed with error: " << result << "\n";
    }
    return (result == 0);
}

void cleanup_winsock()
{
    // MISRA-CPP-2008-0-1-7: WSACleanup returns int; cast to void to show intent
    (void)WSACleanup();
}

SOCKET create_server_socket(int port)
{
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET result     = INVALID_SOCKET;

    if (listenSock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
    }
    else
    {
        int yes = 1;
        // MISRA-CPP-2008-0-1-7: cast setsockopt result to void
        (void)setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<const char*>(&yes), sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(static_cast<u_short>(port));

        if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        {
            std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
            // MISRA-CPP-2008-0-1-7: cast closesocket result to void
            (void)closesocket(listenSock);
        }
        else if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
        {
            std::cerr << "listen() failed: " << WSAGetLastError() << "\n";
            (void)closesocket(listenSock);
        }
        else
        {
            result = listenSock;
        }
    }
    return result;
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
        // MISRA-CPP-2008-0-1-7: cast inet_ntop result to void (used only for print)
        (void)inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
        std::cout << "Accepted connection from " << ipStr
                  << ":" << ntohs(clientAddr.sin_port) << "\n";
    }
    return clientSock;
}

SOCKET create_client_socket(const std::string& host, int port)
{
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string portStr = std::to_string(port);
    addrinfo* addrResult = nullptr;

    int rv = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &addrResult);

    SOCKET sock = INVALID_SOCKET;

    if (rv != 0)
    {
        std::cerr << "getaddrinfo() failed: " << gai_strerrorA(rv) << "\n";
    }
    else
    {
        for (addrinfo* rp = addrResult; (rp != nullptr) && (sock == INVALID_SOCKET); rp = rp->ai_next)
        {
            sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sock != INVALID_SOCKET)
            {
                if (connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) != 0)
                {
                    (void)closesocket(sock);
                    sock = INVALID_SOCKET;
                }
            }
        }
        freeaddrinfo(addrResult);

        if (sock == INVALID_SOCKET)
        {
            std::cerr << "Could not connect to " << host << ":" << port << "\n";
        }
    }
    return sock;
}

void close_socket(SOCKET sock)
{
    if (sock != INVALID_SOCKET)
    {
        (void)closesocket(sock);
    }
}

} // namespace SocketLib

// Wrappers so call-sites compile unchanged
bool   init_winsock()                                          { return SocketLib::init_winsock(); }
void   cleanup_winsock()                                       { SocketLib::cleanup_winsock(); }
SOCKET create_server_socket(int port)                          { return SocketLib::create_server_socket(port); }
SOCKET accept_client(SOCKET listenSock)                        { return SocketLib::accept_client(listenSock); }
SOCKET create_client_socket(const std::string& host, int port) { return SocketLib::create_client_socket(host, port); }
void   close_socket(SOCKET sock)                               { SocketLib::close_socket(sock); }
