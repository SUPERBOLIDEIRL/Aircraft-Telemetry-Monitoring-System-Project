#pragma once

#include <stdint.h>

#define PACKET_TYPE_HANDSHAKE      1
#define PACKET_TYPE_TELEMETRY      2
#define PACKET_TYPE_LARGE_DATA     3
#define PACKET_TYPE_CMD_RESPONSE   4
#define PACKET_TYPE_ACK_NACK       5

#define AIRCRAFT_ID_SIZE 16

typedef struct {
    int32_t packetType;
    int32_t dataSize;
    char aircraftID[AIRCRAFT_ID_SIZE];
    char* payload;
} TelemetryPacket;