#include <iostream>
#include <string>
#include <winsock2.h>
#include "packet.h"
#include "socket.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);

    init_logger("client_log.txt");
    log_event("Client starting, connecting to " + host + ":" + std::to_string(port));

    if (!init_winsock()) {
        log_event("Failed to initialise Winsock");
        close_logger();
        return 1;
    }

    SOCKET sock = create_client_socket(host, port);
    if (sock == INVALID_SOCKET) {
        log_event("Failed to connect to server");
        cleanup_winsock();
        close_logger();
        return 1;
    }
    log_event("Connected to server");

    const char* aircraftID = "CLIENT001";
    const char* authPayload = "AUTH";
    TelemetryPacket* handshake = create_packet(PACKET_TYPE_HANDSHAKE, aircraftID, authPayload, 4);
    if (send_packet(sock, handshake) == 0) {
        log_packet(true, handshake->packetType, handshake->dataSize, handshake->aircraftID);
    }
    else {
        log_event("Failed to send handshake");
        free_packet(handshake);
        close_socket(sock);
        cleanup_winsock();
        close_logger();
        return 1;
    }
    free_packet(handshake);

    TelemetryPacket* response = receive_packet(sock);
    if (response == nullptr) {
        log_event("Failed to receive handshake response");
        close_socket(sock);
        cleanup_winsock();
        close_logger();
        return 1;
    }
    log_packet(false, response->packetType, response->dataSize, response->aircraftID);
    if (response->packetType == PACKET_TYPE_ACK_NACK && response->dataSize >= 3 &&
        memcmp(response->payload, "ACK", 3) == 0) {
        log_event("Handshake successful, ACK received");
    }
    else {
        log_event("Handshake failed, no ACK");
    }
    free_packet(response);

    close_socket(sock);
    cleanup_winsock();
    close_logger();
    return 0;
}