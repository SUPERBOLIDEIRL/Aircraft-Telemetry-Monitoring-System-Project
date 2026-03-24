#include <iostream>
#include <string>
#include <winsock2.h>
#include "packet.h"
#include "socket.h"
#include "logger.h"

enum ServerState {
    IDLE,
    WAITING_FOR_CLIENT,
    CONNECTED,
    PROCESSING_COMMAND,
    TRANSFERRING_DATA,
    DISCONNECTED,
    ERROR
};

const char* stateToString(ServerState s) {
    switch (s) {
    case IDLE: return "IDLE";
    case WAITING_FOR_CLIENT: return "WAITING_FOR_CLIENT";
    case CONNECTED: return "CONNECTED";
    case PROCESSING_COMMAND: return "PROCESSING_COMMAND";
    case TRANSFERRING_DATA: return "TRANSFERRING_DATA";
    case DISCONNECTED: return "DISCONNECTED";
    case ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

void transitionState(ServerState& current, ServerState newState, const std::string& event) {
    if (current != newState) {
        log_event("State transition: " + std::string(stateToString(current)) + " -> " +
            std::string(stateToString(newState)) + " (event: " + event + ")");
        current = newState;
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    init_logger("server_log.txt");
    log_event("Server starting on port " + std::to_string(port));

    if (!init_winsock()) {
        log_event("Failed to initialize Winsock");
        close_logger();
        return 1;
    }

    SOCKET listenSock = create_server_socket(port);
    if (listenSock == INVALID_SOCKET) {
        log_event("Failed to create server socket");
        cleanup_winsock();
        close_logger();
        return 1;
    }

    ServerState state = IDLE;
    transitionState(state, WAITING_FOR_CLIENT, "server ready");

    while (true) {
        log_event("Waiting for client connection...");
        SOCKET clientSock = accept_client(listenSock);
        if (clientSock == INVALID_SOCKET) {
            transitionState(state, ERROR, "accept failed");
            break;
        }
        transitionState(state, CONNECTED, "client accepted");

        bool sessionActive = true;
        while (sessionActive) {
            TelemetryPacket* packet = receive_packet(clientSock);
            if (packet == nullptr) {
                log_event("Connection closed by client or receive error");
                transitionState(state, DISCONNECTED, "client disconnect or error");
                sessionActive = false;
                break;
            }

            log_packet(false, packet->packetType, packet->dataSize, packet->aircraftID);

            if (state == CONNECTED || state == PROCESSING_COMMAND) {
                if (packet->packetType == PACKET_TYPE_HANDSHAKE) {
                    log_event("Handshake received");
                    bool valid = true;
                    if (packet->dataSize > 0 && packet->payload) {
                        if (packet->dataSize != 4 || memcmp(packet->payload, "AUTH", 4) != 0) {
                            valid = false;
                        }
                    }
                    else {
                        valid = true;
                    }
                    if (valid) {
                        TelemetryPacket* ack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "ACK", 3);
                        send_packet(clientSock, ack);
                        log_packet(true, ack->packetType, ack->dataSize, ack->aircraftID);
                        free_packet(ack);
                        transitionState(state, CONNECTED, "handshake success");
                        log_event("Handshake successful, client authenticated");
                    }
                    else {
                        TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "NACK", 4);
                        send_packet(clientSock, nack);
                        log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                        free_packet(nack);
                        log_event("Handshake failed, rejecting client");
                        sessionActive = false;
                    }
                }
                else {
                    log_event("Received non-handshake command in state " + std::string(stateToString(state)) + ", rejecting");
                    TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "UNSUPPORTED", 11);
                    send_packet(clientSock, nack);
                    log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                    free_packet(nack);
                }
            }
            else {
                log_event("Received command in invalid state " + std::string(stateToString(state)) + ", rejecting");
                TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "INVALID_STATE", 13);
                send_packet(clientSock, nack);
                log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                free_packet(nack);
                sessionActive = false;
            }

            free_packet(packet);
        }

        close_socket(clientSock);
        transitionState(state, WAITING_FOR_CLIENT, "client disconnected, waiting for next");
    }

    close_socket(listenSock);
    cleanup_winsock();
    close_logger();
    return 0;
}