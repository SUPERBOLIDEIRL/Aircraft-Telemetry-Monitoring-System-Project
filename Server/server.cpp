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



// state machine
enum class ServerState
{
    IDLE,
    WAITING_FOR_CLIENT,
    CONNECTED,
    PROCESSING_COMMAND,
    TRANSFERRING_DATA,
    DISCONNECTED,
    ERROR_STATE     // avoid collision with windows error
};

static const char* state_name(ServerState s)
{
    switch (s)
    {
    case ServerState::IDLE: return "IDLE";
    case ServerState::WAITING_FOR_CLIENT: return "WAITING_FOR_CLIENT";
    case ServerState::CONNECTED: return "CONNECTED";
    case ServerState::PROCESSING_COMMAND: return "PROCESSING_COMMAND";
    case ServerState::TRANSFERRING_DATA: return "TRANSFERRING_DATA";
    case ServerState::DISCONNECTED: return "DISCONNECTED";
    case ServerState::ERROR_STATE: return "ERROR";
    default: return "UNKNOWN";
    }
}

// transitions the state machine and logs the event
static void transition(ServerState& current,
    ServerState  next,
    const std::string& event)
{
    if (current == next)
        return;

    log_event(std::string("State: ") + state_name(current) + " -> " + state_name(next) + "  [event: " + event + "]");
    current = next;
}


// Handshake handler
//
// returns true if the handshake was valid and ACK was sent returns false if NACK was sent
static bool handle_handshake(SOCKET clientSock, TelemetryPacket* packet, ServerState& state)
{
    log_event("Handshake received from aircraft: " +
        std::string(packet->aircraftID));

    // Validate: must carry exactly the 4-byte payload "AUTH"  (US-07)
    bool valid = (packet->dataSize == 4 && packet->payload != nullptr && memcmp(packet->payload, "AUTH", 4) == 0);

    if (valid)
    {
        TelemetryPacket* ack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "ACK", 3);
        send_packet(clientSock, ack);
        log_packet(true, ack->packetType, ack->dataSize, ack->aircraftID);
        free_packet(ack);

        transition(state, ServerState::PROCESSING_COMMAND, "handshake-ok");
        log_event("Client authenticated successfully");
        return true;
    }
    else
    {
        TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "NACK", 4);
        send_packet(clientSock, nack);
        log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
        free_packet(nack);

        log_event("Handshake FAILED, bad or missing AUTH payload, rejecting client");
        return false;
    }
}

static constexpr int CHUNK_SIZE = 4096;

static void handle_telemetry(SOCKET clientSock, TelemetryPacket* packet)
{
    TelemetryData data{};
    data.altitude_ft = 30000.0f + static_cast<float>(rand() % 10000);
    data.airspeed_knots = 400.0f + static_cast<float>(rand() % 100);
    data.fuel_level_percent = 15.0f + static_cast<float>(rand() % 85);
    data.engine_temp_celsius = 800.0f + static_cast<float>(rand() % 300);
    data.gps_latitude = 43.0f + static_cast<float>(rand() % 100) / 100.0f;
    data.gps_longitude = -79.0f - static_cast<float>(rand() % 100) / 100.0f;

    TelemetryPacket* response = create_packet(PACKET_TYPE_TELEMETRY, packet->aircraftID, reinterpret_cast<const char*>(&data), sizeof(TelemetryData));
    send_packet(clientSock, response);
    log_packet(true, response->packetType, response->dataSize, response->aircraftID);
    free_packet(response);

    TelemetryPacket* ack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "ACK", 3);
    send_packet(clientSock, ack);
    log_packet(true, ack->packetType, ack->dataSize, ack->aircraftID);
    free_packet(ack);

    log_event("Telemetry data sent and ACK issued");
}

static void handle_large_data(SOCKET clientSock, TelemetryPacket* packet, ServerState& state)
{
    transition(state, ServerState::TRANSFERRING_DATA, "large-data-start");

    std::ifstream inFile("flight_data.bin", std::ios::binary);
    bool usingFile = inFile.is_open();

    const int    GENERATED_SIZE = 1024 * 1024;
    int          totalSize = 0;
    int          bytesSent = 0;

    char* generatedData = nullptr;

    if (usingFile)
    {
        inFile.seekg(0, std::ios::end);
        totalSize = static_cast<int>(inFile.tellg());
        inFile.seekg(0, std::ios::beg);
        log_event("Sending flight_data.bin (" + std::to_string(totalSize) + " bytes)");
    }
    else
    {
        totalSize = GENERATED_SIZE;
        generatedData = new char[totalSize];
        for (int i = 0; i < totalSize; ++i)
            generatedData[i] = static_cast<char>(i & 0xFF);
        log_event("flight_data.bin not found - sending " +
            std::to_string(totalSize) + " bytes of generated data");
    }

    char chunkBuf[CHUNK_SIZE];

    while (bytesSent < totalSize)
    {
        int remaining = totalSize - bytesSent;
        int chunkBytes = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

        if (usingFile)
        {
            inFile.read(chunkBuf, chunkBytes);
            chunkBytes = static_cast<int>(inFile.gcount());
            if (chunkBytes <= 0) break;
        }
        else
        {
            memcpy(chunkBuf, generatedData + bytesSent, chunkBytes);
        }

        TelemetryPacket* chunk = create_packet(PACKET_TYPE_LARGE_DATA,
            packet->aircraftID,
            chunkBuf,
            chunkBytes);
        send_packet(clientSock, chunk);
        log_packet(true, chunk->packetType, chunk->dataSize, chunk->aircraftID);
        free_packet(chunk);

        bytesSent += chunkBytes;
    }

    if (usingFile) inFile.close();
    delete[] generatedData;

    TelemetryPacket* end = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "END", 3);
    send_packet(clientSock, end);
    log_packet(true, end->packetType, end->dataSize, end->aircraftID);
    free_packet(end);

    log_event("Large data transfer complete - " + std::to_string(bytesSent) + " bytes sent");
    transition(state, ServerState::PROCESSING_COMMAND, "large-data-done");
}

// session loop
static void run_session(SOCKET clientSock, ServerState& state)
{
    transition(state, ServerState::CONNECTED, "client-accepted");

    bool sessionActive = true;

    while (sessionActive)
    {
        TelemetryPacket* packet = receive_packet(clientSock);

        if (packet == nullptr)
        {
            // nullptr means the peer disconnected or the socket errored.
            log_event("Client disconnected or receive error");
            transition(state, ServerState::ERROR_STATE, "recv-failed");
            break;
        }

        log_packet(false, packet->packetType, packet->dataSize, packet->aircraftID);

        // only accept commands in CONNECTED or PROCESSING_COMMAND
        if (state == ServerState::CONNECTED || state == ServerState::PROCESSING_COMMAND)
        {
            switch (packet->packetType)
            {
            case PACKET_TYPE_HANDSHAKE:
            {
                // Reject a second handshake once already processing commands.
                if (state == ServerState::PROCESSING_COMMAND)
                {
                    log_event("Duplicate handshake ignored - already authenticated");
                    TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "NACK", 4);
                    send_packet(clientSock, nack);
                    log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                    free_packet(nack);
                    break;
                }

                if (!handle_handshake(clientSock, packet, state))
                    sessionActive = false;  // if auth failed close connection
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
                log_event("Received packet type " + std::to_string(packet->packetType) + " - not implemented");
                TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "UNSUPPORTED", 11);
                send_packet(clientSock, nack);
                log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                free_packet(nack);
                break;
            }

            default:
            {
                log_event("Unknown packet type: " + std::to_string(packet->packetType));
                TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "UNKNOWN", 7);
                send_packet(clientSock, nack);
                log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
                free_packet(nack);
                break;
            }
            }
        }
        else
        {
            // invalid state
            log_event("Command received in invalid state " + std::string(state_name(state)) + ", rejecting");
            TelemetryPacket* nack = create_packet(PACKET_TYPE_ACK_NACK, packet->aircraftID, "INVALID_STATE", 13);
            send_packet(clientSock, nack);
            log_packet(true, nack->packetType, nack->dataSize, nack->aircraftID);
            free_packet(nack);
            sessionActive = false;
        }

        free_packet(packet);
    }
}


int main(int argc, char* argv[])
{
    int port = DEFAULT_PORT;
    if (argc > 1)
        port = std::stoi(argv[1]);

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

    // accept loop for handling sequential clients
    while (true)
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
            continue;
        }

        run_session(clientSock, state);

        close_socket(clientSock);

        if (state != ServerState::ERROR_STATE)
        {
            transition(state, ServerState::WAITING_FOR_CLIENT, "client-disconnected");
        }
    }

    close_socket(listenSock);
    cleanup_winsock();
    log_event("Server shut down");
    close_logger();
    return 0;
}