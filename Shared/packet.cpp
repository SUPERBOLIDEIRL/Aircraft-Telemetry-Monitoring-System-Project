#include "packet.h"
#include <cstring>
#include <cstdlib>

uint16_t calculate_checksum(const char* data, int32_t size)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < size; ++i)
    {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

static int recv_exact(SOCKET sock, char* buf, int needed)
{
    int received = 0;
    while (received < needed)
    {
        int bytes = recv(sock, buf + received, needed - received, 0);
        if (bytes <= 0) return -1;
        received += bytes;
    }
    return 0;
}

TelemetryPacket* create_packet(int32_t type, const char* aircraftID, const char* payload, int32_t payloadSize)
{
    TelemetryPacket* p = new TelemetryPacket{};
    p->packetType = type;
    p->dataSize = payloadSize;

    if (aircraftID != nullptr)
    {
        strncpy_s(p->aircraftID, AIRCRAFT_ID_SIZE, aircraftID, AIRCRAFT_ID_SIZE - 1);
        p->aircraftID[AIRCRAFT_ID_SIZE - 1] = '\0';
    }

    if (payloadSize > 0 && payload != nullptr)
    {
        p->payload = new char[payloadSize];
        memcpy(p->payload, payload, payloadSize);
        p->checksum = calculate_checksum(p->payload, payloadSize);
    }
    else
    {
        p->payload = nullptr;
        p->dataSize = 0;
        p->checksum = 0;
    }
    return p;
}

void free_packet(TelemetryPacket* packet)
{
    if (packet)
    {
        delete[] packet->payload;
        delete packet;
    }
}

int send_packet(SOCKET sock, const TelemetryPacket* packet)
{
    int32_t netType = htonl(packet->packetType);
    int32_t netSize = htonl(packet->dataSize);
    uint16_t netCRC = htons(packet->checksum);

    if (send(sock, reinterpret_cast<const char*>(&netType), sizeof(netType), 0) != sizeof(netType)) return -1;
    if (send(sock, reinterpret_cast<const char*>(&netSize), sizeof(netSize), 0) != sizeof(netSize)) return -1;
    if (send(sock, reinterpret_cast<const char*>(&netCRC), sizeof(netCRC), 0) != sizeof(netCRC)) return -1;
    if (send(sock, packet->aircraftID, AIRCRAFT_ID_SIZE, 0) != AIRCRAFT_ID_SIZE) return -1;

    if (packet->dataSize > 0 && packet->payload != nullptr)
    {
        if (send(sock, packet->payload, packet->dataSize, 0) != packet->dataSize) return -1;
    }
    return 0;
}

TelemetryPacket* receive_packet(SOCKET sock)
{
    int32_t netType, netSize;
    uint16_t netCRC;

    if (recv_exact(sock, reinterpret_cast<char*>(&netType), sizeof(netType)) != 0) return nullptr;
    if (recv_exact(sock, reinterpret_cast<char*>(&netSize), sizeof(netSize)) != 0) return nullptr;
    if (recv_exact(sock, reinterpret_cast<char*>(&netCRC), sizeof(netCRC)) != 0) return nullptr;

    TelemetryPacket* p = new TelemetryPacket{};
    p->packetType = ntohl(netType);
    p->dataSize = ntohl(netSize);
    p->checksum = ntohs(netCRC);

    if (recv_exact(sock, p->aircraftID, AIRCRAFT_ID_SIZE) != 0)
    {
        delete p;
        return nullptr;
    }

    if (p->dataSize > 0)
    {
        p->payload = new char[p->dataSize];
        if (recv_exact(sock, p->payload, p->dataSize) != 0)
        {
            free_packet(p);
            return nullptr;
        }

        if (calculate_checksum(p->payload, p->dataSize) != p->checksum)
        {
            free_packet(p);
            return nullptr;
        }
    }
    return p;
}