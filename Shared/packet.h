#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#include <cstdint>

// MISRA-CPP-2008-7-3-1: Wrap constants/types in a namespace
namespace Telemetry
{
    constexpr int32_t PACKET_TYPE_HANDSHAKE    = 1;
    constexpr int32_t PACKET_TYPE_TELEMETRY    = 2;
    constexpr int32_t PACKET_TYPE_LARGE_DATA   = 3;
    constexpr int32_t PACKET_TYPE_CMD_RESPONSE = 4;
    constexpr int32_t PACKET_TYPE_ACK_NACK     = 5;

    constexpr int AIRCRAFT_ID_SIZE = 16;

    struct TelemetryPacket
    {
        int32_t  packetType;
        int32_t  dataSize;
        uint16_t checksum;
        char     aircraftID[AIRCRAFT_ID_SIZE];
        char*    payload;
    };

    // Serialized into the payload field of a PACKET_TYPE_TELEMETRY packet
    struct TelemetryData
    {
        float altitude_ft;
        float airspeed_knots;
        float fuel_level_percent;
        float engine_temp_celsius;
        float gps_latitude;
        float gps_longitude;
    };

    uint16_t       calculate_checksum(const char* data, int32_t size);
    TelemetryPacket* create_packet(int32_t type, const char* aircraftID, const char* payload, int32_t payloadSize);
    void           free_packet(TelemetryPacket* packet);
    int            send_packet(SOCKET sock, const TelemetryPacket* packet);
    TelemetryPacket* receive_packet(SOCKET sock);

} // namespace Telemetry

// Convenience aliases so existing call-sites keep compiling without changes
using Telemetry::TelemetryPacket;
using Telemetry::TelemetryData;
using Telemetry::create_packet;
using Telemetry::free_packet;
using Telemetry::send_packet;
using Telemetry::receive_packet;
using Telemetry::calculate_checksum;
using Telemetry::PACKET_TYPE_HANDSHAKE;
using Telemetry::PACKET_TYPE_TELEMETRY;
using Telemetry::PACKET_TYPE_LARGE_DATA;
using Telemetry::PACKET_TYPE_CMD_RESPONSE;
using Telemetry::PACKET_TYPE_ACK_NACK;
using Telemetry::AIRCRAFT_ID_SIZE;
