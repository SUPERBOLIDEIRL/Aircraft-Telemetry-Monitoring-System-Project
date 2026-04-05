#include <iostream>
#include <string>
#include <cstring>

#include "../Shared/packet.h"
#include "../Shared/socket.h"
#include "../Shared/logger.h"
#include "thresholds.h"

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
        log_event("Handshake successful - server acknowledged");
    else
        log_event("Handshake FAILED - server sent NACK or unexpected response");

    free_packet(response);
    return acked;
}

static void request_telemetry(SOCKET sock)
{
    // Send telemetry request
    TelemetryPacket* request = create_packet(PACKET_TYPE_TELEMETRY, "CLIENT001", "GET_TELEMETRY", 13);
    send_packet(sock, request);
    log_packet(true, request->packetType, request->dataSize, request->aircraftID);
    free_packet(request);

    // Receive response
    TelemetryPacket* response = receive_packet(sock);
    if (response == nullptr)
    {
        log_event("ERROR: No telemetry response received");
        return;
    }

    log_packet(false, response->packetType, response->dataSize, response->aircraftID);

    // Cast payload to TelemetryData and print fields
    TelemetryData* data = reinterpret_cast<TelemetryData*>(response->payload);
    std::cout << "Altitude (ft): " << data->altitude_ft << "\n";
    std::cout << "Airspeed (knots): " << data->airspeed_knots << "\n";
    std::cout << "Fuel Level (%): " << data->fuel_level_percent << "\n";
    std::cout << "Engine Temp (C): " << data->engine_temp_celsius << "\n";
    std::cout << "GPS Lat: " << data->gps_latitude << "\n";
    std::cout << "GPS Lon: " << data->gps_longitude << "\n";

    // Check thresholds and print warnings
    if (data->fuel_level_percent < FUEL_LEVEL_MIN_PERCENT)
        std::cout << "WARNING: Fuel level below safe threshold\n";
    if (data->engine_temp_celsius > ENGINE_TEMP_MAX_CELSIUS)
        std::cout << "WARNING: Engine temperature above safe threshold\n";
    if (data->altitude_ft > ALTITUDE_MAX_FEET)
        std::cout << "WARNING: Altitude above safe limit\n";
    if (data->airspeed_knots > AIRSPEED_MAX_KNOTS)
        std::cout << "WARNING: Airspeed above safe limit\n";

    free_packet(response);
}

int main(int argc, char* argv[])
{
    const char* host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);

    init_logger("client_log.txt");
    log_event("Client starting - target: " + std::string(host) +
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
    {
        log_event("Session terminated - authentication failed");
    }
    else
    {
        log_event("Session ready - entering main menu");

        bool running = true;
        while (running)
        {
            std::cout << "\n===== Ground Control Menu =====\n";
            std::cout << "1) Request Telemetry\n";
            std::cout << "2) Download Flight Data File\n";
            std::cout << "3) View Log\n";
            std::cout << "4) Exit\n";
            std::cout << "Enter choice: ";

            int choice;
            if (!(std::cin >> choice))
            {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "Invalid option, try again.\n";
                continue;
            }

            switch (choice)
            {
            case 1:
                request_telemetry(sock);
                log_event("User selected: Request Telemetry");
                break;
            case 2:
                std::cout << "Feature coming in Sprint 2\n";
                log_event("User selected: Download Flight Data File");
                break;
            case 3:
                std::cout << "Feature coming in Sprint 2\n";
                log_event("User selected: View Log");
                break;
            case 4:
                log_event("User selected: Exit");
                running = false;
                break;
            default:
                std::cout << "Invalid option, try again.\n";
                break;
            }
        }
    }

    // cleanup
    close_socket(sock);
    cleanup_winsock();
    log_event("Client shut down");
    close_logger();

    return authenticated ? 0 : 1;
}
