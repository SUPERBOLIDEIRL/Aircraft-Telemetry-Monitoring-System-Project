#include "packet.h"
#include <cstring>
#include <cstdlib>

// MISRA-CPP-2008-7-3-1: All definitions in namespace
namespace Telemetry
{

uint16_t calculate_checksum(const char* data, int32_t size)
{
    uint16_t crc = 0xFFFFU;
    for (int32_t i = 0; i < size; ++i)
    {
        crc ^= static_cast<uint16_t>(static_cast<uint8_t>(data[i]));
        // MISRA-CPP-2008-6-3-1: brace the for body
        for (int j = 0; j < 8; ++j)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = static_cast<uint16_t>((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc = static_cast<uint16_t>(crc >> 1U);
            }
        }
    }
    return crc;
}

// MISRA-CPP-2008-6-6-5: single exit — return -1 at bottom instead of early return
static int recv_exact(SOCKET sock, char* buf, int needed)
{
    int received = 0;
    int result   = 0;

    while ((received < needed) && (result == 0))
    {
        int bytes = recv(sock, buf + received, needed - received, 0);
        if (bytes <= 0)
        {
            result = -1;
        }
        else
        {
            received += bytes;
        }
    }
    return result;
}

TelemetryPacket* create_packet(int32_t type, const char* aircraftID,
                                const char* payload, int32_t payloadSize)
{
    TelemetryPacket* p = new TelemetryPacket{};
    p->packetType = type;
    p->dataSize   = payloadSize;

    if (aircraftID != nullptr)
    {
        // MISRA-CPP-2008-0-1-7: use return value of strncpy_s
        (void)strncpy_s(p->aircraftID, AIRCRAFT_ID_SIZE, aircraftID, AIRCRAFT_ID_SIZE - 1);
        p->aircraftID[AIRCRAFT_ID_SIZE - 1] = '\0';
    }

    if ((payloadSize > 0) && (payload != nullptr))
    {
        p->payload = new char[static_cast<size_t>(payloadSize)];
        // MISRA-CPP-2008-0-1-7: use return value of memcpy
        (void)memcpy(p->payload, payload, static_cast<size_t>(payloadSize));
        p->checksum = calculate_checksum(p->payload, payloadSize);
    }
    else
    {
        p->payload  = nullptr;
        p->dataSize = 0;
        p->checksum = 0U;
    }
    return p;
}

void free_packet(TelemetryPacket* packet)
{
    if (packet != nullptr)
    {
        delete[] packet->payload;
        delete packet;
    }
}

int send_packet(SOCKET sock, const TelemetryPacket* packet)
{
    int32_t  netType = htonl(static_cast<u_long>(packet->packetType));
    int32_t  netSize = htonl(static_cast<u_long>(packet->dataSize));
    uint16_t netCRC  = htons(packet->checksum);

    int result = 0;

    if (send(sock, reinterpret_cast<const char*>(&netType), sizeof(netType), 0) != sizeof(netType))
    {
        result = -1;
    }
    if ((result == 0) &&
        (send(sock, reinterpret_cast<const char*>(&netSize), sizeof(netSize), 0) != sizeof(netSize)))
    {
        result = -1;
    }
    if ((result == 0) &&
        (send(sock, reinterpret_cast<const char*>(&netCRC), sizeof(netCRC), 0) != sizeof(netCRC)))
    {
        result = -1;
    }
    if ((result == 0) &&
        (send(sock, packet->aircraftID, AIRCRAFT_ID_SIZE, 0) != AIRCRAFT_ID_SIZE))
    {
        result = -1;
    }
    if ((result == 0) &&
        (packet->dataSize > 0) && (packet->payload != nullptr))
    {
        if (send(sock, packet->payload, packet->dataSize, 0) != packet->dataSize)
        {
            result = -1;
        }
    }
    return result;
}

TelemetryPacket* receive_packet(SOCKET sock)
{
    int32_t  netType = 0;
    int32_t  netSize = 0;
    uint16_t netCRC  = 0U;

    TelemetryPacket* p = nullptr;

    if (recv_exact(sock, reinterpret_cast<char*>(&netType), sizeof(netType)) == 0)
    {
        if (recv_exact(sock, reinterpret_cast<char*>(&netSize), sizeof(netSize)) == 0)
        {
            if (recv_exact(sock, reinterpret_cast<char*>(&netCRC), sizeof(netCRC)) == 0)
            {
                p = new TelemetryPacket{};
                p->packetType = static_cast<int32_t>(ntohl(static_cast<u_long>(netType)));
                p->dataSize   = static_cast<int32_t>(ntohl(static_cast<u_long>(netSize)));
                p->checksum   = ntohs(netCRC);

                if (recv_exact(sock, p->aircraftID, AIRCRAFT_ID_SIZE) != 0)
                {
                    free_packet(p);
                    p = nullptr;
                }
                else if (p->dataSize > 0)
                {
                    p->payload = new char[static_cast<size_t>(p->dataSize)];
                    if (recv_exact(sock, p->payload, p->dataSize) != 0)
                    {
                        free_packet(p);
                        p = nullptr;
                    }
                    else if (calculate_checksum(p->payload, p->dataSize) != p->checksum)
                    {
                        free_packet(p);
                        p = nullptr;
                    }
                    else
                    {
                        // packet is valid — p remains set
                    }
                }
                else
                {
                    // no payload — packet is valid
                }
            }
        }
    }
    return p;
}

} // namespace Telemetry
