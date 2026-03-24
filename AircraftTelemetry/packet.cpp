#include "packet.h"
#include <winsock2.h>
#include <cstring>
#include <iostream>

TelemetryPacket* create_packet(int32_t type, const char* aircraftID, const char* payload, int32_t payloadSize) {
    TelemetryPacket* p = new TelemetryPacket;
    p->packetType = type;
    p->dataSize = payloadSize;
    strncpy_s(p->aircraftID, AIRCRAFT_ID_SIZE, aircraftID, AIRCRAFT_ID_SIZE - 1);
    p->aircraftID[AIRCRAFT_ID_SIZE - 1] = '\0';
    if (payloadSize > 0 && payload != nullptr) {
        p->payload = new char[payloadSize];
        memcpy(p->payload, payload, payloadSize);
    }
    else {
        p->payload = nullptr;
    }
    return p;
}

void free_packet(TelemetryPacket* packet) {
    if (packet) {
        if (packet->payload) {
            delete[] packet->payload;
        }
        delete packet;
    }
}

int send_packet(SOCKET sock, const TelemetryPacket* packet) {
    int32_t type = htonl(packet->packetType);
    int32_t size = htonl(packet->dataSize);
    int sent = send(sock, (const char*)&type, sizeof(type), 0);
    if (sent != sizeof(type)) return -1;
    sent = send(sock, (const char*)&size, sizeof(size), 0);
    if (sent != sizeof(size)) return -1;
    sent = send(sock, packet->aircraftID, AIRCRAFT_ID_SIZE, 0);
    if (sent != AIRCRAFT_ID_SIZE) return -1;
    if (packet->dataSize > 0 && packet->payload) {
        sent = send(sock, packet->payload, packet->dataSize, 0);
        if (sent != packet->dataSize) return -1;
    }
    return 0;
}

TelemetryPacket* receive_packet(SOCKET sock) {
    TelemetryPacket* p = new TelemetryPacket;
    int32_t type, size;
    int recvd = recv(sock, (char*)&type, sizeof(type), 0);
    if (recvd != sizeof(type)) { delete p; return nullptr; }
    recvd = recv(sock, (char*)&size, sizeof(size), 0);
    if (recvd != sizeof(size)) { delete p; return nullptr; }
    p->packetType = ntohl(type);
    p->dataSize = ntohl(size);
    recvd = recv(sock, p->aircraftID, AIRCRAFT_ID_SIZE, 0);
    if (recvd != AIRCRAFT_ID_SIZE) { delete p; return nullptr; }
    p->aircraftID[AIRCRAFT_ID_SIZE - 1] = '\0';
    if (p->dataSize > 0) {
        p->payload = new char[p->dataSize];
        int total = 0;
        while (total < p->dataSize) {
            int bytes = recv(sock, p->payload + total, p->dataSize - total, 0);
            if (bytes <= 0) { delete[] p->payload; delete p; return nullptr; }
            total += bytes;
        }
    }
    else {
        p->payload = nullptr;
    }
    return p;
}