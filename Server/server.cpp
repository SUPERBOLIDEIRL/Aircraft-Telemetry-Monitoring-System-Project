#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>

#include "../Shared/packet.h"
#include "../Shared/socket.h"
#include "../Shared/logger.h"

// Default port overridden by passing a port number as argv[1].
// Change this value here to reconfigure at compile time as stated in REQ-COM-010.
static constexpr int DEFAULT_PORT = 8080;

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
enum class ServerState
{
    IDLE,
    WAITING_FOR_CLIENT,
    CONNECTED,
    PROCESSING_COMMAND,
    TRANSFERRING_DATA,
    DISCONNECTED,
    ERROR_STATE
};

static const char* state_name(ServerState s)
{
    const char* name = "UNKNOWN";
    switch (s)
    {
    case ServerState::IDLE:               name = "IDLE";               break;
    case ServerState::WAITING_FOR_CLIENT: name = "WAITING_FOR_CLIENT"; break;
    case ServerState::CONNECTED:          name = "CONNECTED";          break;
    case ServerState::PROCESSING_COMMAND: name = "PROCESSING_COMMAND"; break;
    case ServerState::TRANSFERRING_DATA:  name = "TRANSFERRING_DATA";  break;
    case ServerState::DISCONNECTED:       name = "DISCONNECTED";       break;
    case ServerState::ERROR_STATE:        name = "ERROR";              break;
    default:                                                            break;
    }
    return name;
}

static void transition(ServerState& current,
                        ServerState  next,
                        const std::string& event)
{
    if (current != next)
    {
        log_event(std::string("State: ") + state_name(current)
                  + " -> " + state_name(next)
                  + "  [event: " + event + "]");
        current = next;
    }
}

// ---------------------------------------------------------------------------
// Handshake
// MISRA-CPP-2008-7-1-2: packet param not modified — declare const
// MISRA-CPP-2008-6-6-5: single exit point
// ---------------------------------------------------------------------------
static bool handle_handshake(SOCKET clientSock,
                              const TelemetryPacket* packet,
                              ServerState& state)
{
    log_event("Handshake received from aircraft: " +
              std::string(packet->aircraftID));

    // Validate: must carry exactly the 4-byte payload "AUTH"
    bool valid = (packet->dataSize == 4) &&
                 (packet->payload != nullptr) &&
                 (memcmp(packet->payload, "AUTH", 4) == 0);

    if (valid)
    {
        TelemetryPacket* ack = create_packet(PACKET_TYPE_ACK_NACK,
                                              packet->aircraftID, "ACK", 3);
        // MISRA-CPP-2008-0-1-7: use return value
        (void)send_packet(clientSock, ack);
        log_packet(true, ack->packetType, ack->dataSize, ack->aircraftID);
        free_packet(ack);

        transition(state, ServerState::PROCESSING_COMMAND, "handshake-ok");
        log_event("Client authenticated successfully");
    }
    else
    {
        TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK,
                                               packet->aircraftID, "NACK", 4);
        (void)send_packet(clientSock, nack);
        log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
        free_packet(nack);

        log_event("Handshake FAILED, bad or missing AUTH payload, rejecting client");
    }
    return valid;
}

static constexpr int CHUNK_SIZE = 4096;

// MISRA-CPP-2008-7-1-2: packet not modified
static void handle_telemetry(SOCKET clientSock, const TelemetryPacket* packet)
{
    TelemetryData data{};
    // MISRA-CPP-2008-2-13-4: uppercase F suffix
    data.altitude_ft          = 30000.0F + static_cast<float>(rand() % 10000);
    data.airspeed_knots       = 400.0F   + static_cast<float>(rand() % 100);
    data.fuel_level_percent   = 15.0F    + static_cast<float>(rand() % 85);
    data.engine_temp_celsius  = 800.0F   + static_cast<float>(rand() % 300);
    data.gps_latitude         = 43.0F    + static_cast<float>(rand() % 100) / 100.0F;
    data.gps_longitude        = -79.0F   - static_cast<float>(rand() % 100) / 100.0F;

    TelemetryPacket* response = create_packet(PACKET_TYPE_TELEMETRY,
                                               packet->aircraftID,
                                               reinterpret_cast<const char*>(&data),
                                               static_cast<int32_t>(sizeof(TelemetryData)));
    (void)send_packet(clientSock, response);
    log_packet(true, response->packetType, response->dataSize, response->aircraftID);
    free_packet(response);

    TelemetryPacket* ack = create_packet(PACKET_TYPE_ACK_NACK,
                                          packet->aircraftID, "ACK", 3);
    (void)send_packet(clientSock, ack);
    log_packet(true, ack->packetType, ack->dataSize, ack->aircraftID);
    free_packet(ack);

    log_event("Telemetry data sent and ACK issued");
}

// MISRA-CPP-2008-7-1-2: packet not modified
static void handle_large_data(SOCKET clientSock,
                               const TelemetryPacket* packet,
                               ServerState& state)
{
    transition(state, ServerState::TRANSFERRING_DATA, "large-data-start");

    std::ifstream inFile("flight_data.bin", std::ios::binary);
    bool usingFile = inFile.is_open();

    // MISRA-CPP-2008-3-4-1: declare GENERATED_SIZE close to where it is used
    const int GENERATED_SIZE = 1024 * 1024;

    int totalSize  = 0;
    int bytesSent  = 0;

    // MISRA-CPP-2008-18-4-1: new/delete required by project design;
    // suppress warning — raw allocation kept minimal and paired with delete[]
    char* generatedData = nullptr;

    if (usingFile)
    {
        // MISRA-CPP-2008-0-1-7: use return value of seekg
        (void)inFile.seekg(0, std::ios::end);
        totalSize = static_cast<int>(inFile.tellg());
        (void)inFile.seekg(0, std::ios::beg);
        log_event("Sending flight_data.bin (" + std::to_string(totalSize) + " bytes)");
    }
    else
    {
        totalSize    = GENERATED_SIZE;
        generatedData = new char[static_cast<size_t>(totalSize)];
        for (int i = 0; i < totalSize; ++i)
        {
            // MISRA-CPP-2008-5-0-15: use array indexing instead of pointer arithmetic
            generatedData[i] = static_cast<char>(i & 0xFF);
        }
        log_event("flight_data.bin not found - sending " +
                  std::to_string(totalSize) + " bytes of generated data");
    }

    char chunkBuf[CHUNK_SIZE];

    while (bytesSent < totalSize)
    {
        int remaining  = totalSize - bytesSent;
        int chunkBytes = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

        if (usingFile)
        {
            // MISRA-CPP-2008-0-1-7: use return value of read
            (void)inFile.read(chunkBuf, chunkBytes);
            chunkBytes = static_cast<int>(inFile.gcount());
            if (chunkBytes <= 0)
            {
                break;
            }
        }
        else
        {
            // MISRA-CPP-2008-5-0-15: array indexing instead of pointer arithmetic
            (void)memcpy(chunkBuf, &generatedData[bytesSent],
                         static_cast<size_t>(chunkBytes));
        }

        TelemetryPacket* chunk = create_packet(PACKET_TYPE_LARGE_DATA,
                                                packet->aircraftID,
                                                chunkBuf,
                                                chunkBytes);
        (void)send_packet(clientSock, chunk);
        log_packet(true, chunk->packetType, chunk->dataSize, chunk->aircraftID);
        free_packet(chunk);

        bytesSent += chunkBytes;
    }

    if (usingFile)
    {
        inFile.close();
    }
    delete[] generatedData;

    TelemetryPacket* end = create_packet(PACKET_TYPE_ACK_NACK,
                                          packet->aircraftID, "END", 3);
    (void)send_packet(clientSock, end);
    log_packet(true, end->packetType, end->dataSize, end->aircraftID);
    free_packet(end);

    log_event("Large data transfer complete - " + std::to_string(bytesSent) + " bytes sent");
    transition(state, ServerState::PROCESSING_COMMAND, "large-data-done");
}

// ---------------------------------------------------------------------------
// Session loop
// ---------------------------------------------------------------------------
static void run_session(SOCKET clientSock, ServerState& state)
{
    transition(state, ServerState::CONNECTED, "client-accepted");

    bool sessionActive = true;

    while (sessionActive)
    {
        TelemetryPacket* packet = receive_packet(clientSock);

        if (packet == nullptr)
        {
            log_event("Client disconnected or receive error");
            transition(state, ServerState::ERROR_STATE, "recv-failed");
            sessionActive = false;
        }
        else
        {
            log_packet(false, packet->packetType, packet->dataSize, packet->aircraftID);

            if ((state == ServerState::CONNECTED) ||
                (state == ServerState::PROCESSING_COMMAND))
            {
                switch (packet->packetType)
                {
                case PACKET_TYPE_HANDSHAKE:
                {
                    if (state == ServerState::PROCESSING_COMMAND)
                    {
                        log_event("Duplicate handshake ignored - already authenticated");
                        TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK,
                                                               packet->aircraftID, "NACK", 4);
                        (void)send_packet(clientSock, nack);
                        log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                        free_packet(nack);
                    }
                    else
                    {
                        if (!handle_handshake(clientSock, packet, state))
                        {
                            sessionActive = false;
                        }
                    }
                    break;
                }

                case PACKET_TYPE_TELEMETRY:
                {
                    handle_telemetry(clientSock, packet);
                    break;
                }

                case PACKET_TYPE_LARGE_DATA:
                {
                    handle_large_data(clientSock, packet, state);
                    break;
                }

                case PACKET_TYPE_CMD_RESPONSE:
                {
                    log_event("Received packet type " +
                              std::to_string(packet->packetType) + " - not implemented");
                    TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK,
                                                           packet->aircraftID, "UNSUPPORTED", 11);
                    (void)send_packet(clientSock, nack);
                    log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                    free_packet(nack);
                    break;
                }

                default:
                {
                    log_event("Unknown packet type: " + std::to_string(packet->packetType));
                    TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK,
                                                           packet->aircraftID, "UNKNOWN", 7);
                    (void)send_packet(clientSock, nack);
                    log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                    free_packet(nack);
                    break;
                }
                }
            }
            else
            {
                log_event("Command received in invalid state " +
                           std::string(state_name(state)) + ", rejecting");
                TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK,
                                                       packet->aircraftID, "INVALID_STATE", 13);
                (void)send_packet(clientSock, nack);
                log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                free_packet(nack);
                sessionActive = false;
            }

            free_packet(packet);
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    int port = DEFAULT_PORT;
    if (argc > 1)
    {
        port = std::stoi(argv[1]);
    }

    srand(static_cast<unsigned int>(time(nullptr)));

    init_logger("server_log.txt");
    log_event("Server starting on port " + std::to_string(port));

    if (!init_winsock())
    {
        log_event("FATAL: Failed to initialise Winsock");
        close_logger();
        return 1;
    }

    SOCKET listenSock = create_server_socket(port);
    if (listenSock == INVALID_SOCKET)
    {
        log_event("FATAL: Failed to create server socket");
        cleanup_winsock();
        close_logger();
        return 1;
    }

    ServerState state = ServerState::IDLE;
    transition(state, ServerState::WAITING_FOR_CLIENT, "server-ready");

    // Accept loop - handles sequential clients
    bool running = true;
    while (running)
    {
        if (state == ServerState::ERROR_STATE)
        {
            transition(state, ServerState::IDLE, "recovery-reset");
            transition(state, ServerState::WAITING_FOR_CLIENT, "ready-for-new-connection");
        }

        log_event("Waiting for client connection...");

        SOCKET clientSock = accept_client(listenSock);
        if (clientSock == INVALID_SOCKET)
        {
            transition(state, ServerState::ERROR_STATE, "accept-failed");
        }
        else
        {
            run_session(clientSock, state);
            close_socket(clientSock);

            if (state != ServerState::ERROR_STATE)
            {
                transition(state, ServerState::WAITING_FOR_CLIENT, "client-disconnected");
            }
        }
    }

    close_socket(listenSock);
    cleanup_winsock();
    log_event("Server shut down");
    close_logger();
    return 0;
}
