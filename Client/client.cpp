#include <iostream>
#include <string>
#include <cstring>

#include "../Shared/packet.h"
#include "../Shared/socket.h"
#include "../Shared/logger.h"

// Default connection target overridable via argv[1] and argv[2].
static constexpr const char* DEFAULT_HOST = "127.0.0.1";
static constexpr int          DEFAULT_PORT = 8080;

// Aircraft identifier sent with every packet from this client
static constexpr const char* AIRCRAFT_ID = "CLIENT001";

// Handshake sequence

static bool perform_handshake(SOCKET sock)
{
    // Send handshake
    TelemetryPacket* hs = create_packet(PACKET_TYPE_HANDSHAKE, AIRCRAFT_ID, "AUTH", 4);
    int sendResult = send_packet(sock, hs);
    log_packet(true, hs->packetType, hs->dataSize, hs->aircraftID);
    free_packet(hs);

    if (sendResult != 0)
    {
        log_event("ERROR: Failed to send handshake packet");
        return false;
    }

    // Receive response
    TelemetryPacket* response = receive_packet(sock);
    if (response == nullptr)
    {
        log_event("ERROR: No response to handshake (server closed connection?)");
        return false;
    }

    log_packet(false, response->packetType, response->dataSize, response->aircraftID);

    bool acked = (response->packetType == PACKET_TYPE_ACK_NACK &&
        response->dataSize >= 3 &&
        response->payload != nullptr &&
        memcmp(response->payload, "ACK", 3) == 0);

    if (acked)
        log_event("Handshake successful — server acknowledged");
    else
        log_event("Handshake FAILED — server sent NACK or unexpected response");

    free_packet(response);
    return acked;
}


int main(int argc, char* argv[])
{
    const char* host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);

    init_logger("client_log.txt");
    log_event("Client starting — target: " + std::string(host) +
        ":" + std::to_string(port));

    if (!init_winsock())
    {
        log_event("FATAL: Failed to initialise Winsock");
        close_logger();
        return 1;
    }

    // connect
    SOCKET sock = create_client_socket(host, port);
    if (sock == INVALID_SOCKET)
    {
        log_event("FATAL: Could not connect to server");
        cleanup_winsock();
        close_logger();
        return 1;
    }
    log_event("Connected to server at " + std::string(host) +
        ":" + std::to_string(port));

    // handshake
    bool authenticated = perform_handshake(sock);

    if (!authenticated)
        log_event("Session terminated — authentication failed");
    else
        log_event("Session ready — Sprint 2 features will go here");

    // cleanup
    close_socket(sock);
    cleanup_winsock();
    log_event("Client shut down");
    close_logger();

    return authenticated ? 0 : 1;
}
