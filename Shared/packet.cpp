#include "packet.h"

#include <cstring>
#include <cstdlib>

static int recv_exact(SOCKET sock, char* buf, int needed)
{
    int received = 0;
    while (received < needed)
    {
        int bytes = recv(sock, buf + received, needed - received, 0);
        if (bytes <= 0)
            return -1;
        received += bytes;
    }
    return 0;
}

TelemetryPacket* create_packet(int32_t     type,
    const char* aircraftID,
    const char* payload,
    int32_t     payloadSize)
{
    TelemetryPacket* p = new TelemetryPacket{};
    p->packetType = type;
    p->dataSize = payloadSize;

    if (aircraftID != nullptr)
    {
        strncpy_s(p->aircraftID, AIRCRAFT_ID_SIZE, aircraftID, AIRCRAFT_ID_SIZE - 1);
        p->aircraftID[AIRCRAFT_ID_SIZE - 1] = '\0';
    }
    else
    {
        memset(p->aircraftID, 0, AIRCRAFT_ID_SIZE);
    }

    if (payloadSize > 0 && payload != nullptr)
    {
        p->payload = new char[payloadSize];
        memcpy(p->payload, payload, payloadSize);
    }
    else
    {
        p->payload = nullptr;
        p->dataSize = 0;
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

    if (send(sock, reinterpret_cast<const char*>(&netType), sizeof(netType), 0) != sizeof(netType))
        return -1;

    if (send(sock, reinterpret_cast<const char*>(&netSize), sizeof(netSize), 0) != sizeof(netSize))
        return -1;

    if (send(sock, packet->aircraftID, AIRCRAFT_ID_SIZE, 0) != AIRCRAFT_ID_SIZE)
        return -1;

    if (packet->dataSize > 0 && packet->payload != nullptr)
    {
        int sent = 0;
        while (sent < packet->dataSize)
        {
            int bytes = send(sock,
                packet->payload + sent,
                packet->dataSize - sent,
                0);
            if (bytes <= 0)
                return -1;
            sent += bytes;
        }
    }

    if (!packet) return -1; 

    return 0;
}

TelemetryPacket* receive_packet(SOCKET sock)
{
    int32_t netType = 0;
    int32_t netSize = 0;

    if (recv_exact(sock, reinterpret_cast<char*>(&netType), sizeof(netType)) != 0)
        return nullptr;

    if (recv_exact(sock, reinterpret_cast<char*>(&netSize), sizeof(netSize)) != 0)
        return nullptr;

    TelemetryPacket* p = new TelemetryPacket{};
    p->packetType = ntohl(netType);
    p->dataSize = ntohl(netSize);

    if (recv_exact(sock, p->aircraftID, AIRCRAFT_ID_SIZE) != 0)
    {
        delete p; 
        return nullptr;
    }
    p->aircraftID[AIRCRAFT_ID_SIZE - 1] = '\0';

    if (p->dataSize > 0)
    {
        p->payload = new char[p->dataSize];
        if (recv_exact(sock, p->payload, p->dataSize) != 0)
        {
            delete[] p->payload;
            delete p;
            return nullptr;
        }
    }
    else
    {
        p->payload = nullptr;
    }

    return p;
}