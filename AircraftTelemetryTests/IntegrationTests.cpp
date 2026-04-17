#include "pch.h"
#include "CppUnitTest.h"
#include "../Shared/packet.h"
#include "../Shared/socket.h"
#include "../Shared/logger.h"
#include "../Client/thresholds.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
using namespace Thresholds;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

void run_mini_server(int port, std::atomic<bool>& ready, std::atomic<int>& receivedType, std::string& receivedAircraftID)
{
    if (!init_winsock())
    {
        ready = true;
        return;
    }

    SOCKET listenSock = create_server_socket(port);
    ready = true;

    if (listenSock == INVALID_SOCKET)
    {
        cleanup_winsock();
        return;
    }

    SOCKET clientSock = accept_client(listenSock);
    if (clientSock != INVALID_SOCKET)
    {
        TelemetryPacket* packet = receive_packet(clientSock);
        if (packet != nullptr)
        {
            receivedType = packet->packetType;
            receivedAircraftID = packet->aircraftID;

            if (packet->packetType == PACKET_TYPE_HANDSHAKE &&
                packet->payload != nullptr &&
                packet->dataSize == 4 &&
                std::memcmp(packet->payload, "AUTH", 4) == 0)
            {
                TelemetryPacket* response = create_packet(
                    PACKET_TYPE_ACK_NACK,
                    packet->aircraftID,
                    "ACK",
                    3);

                if (response != nullptr)
                {
                    send_packet(clientSock, response);
                    free_packet(response);
                }
            }
            else if (packet->packetType == PACKET_TYPE_TELEMETRY)
            {
                TelemetryData data{};
                data.altitude_ft = 35000.0F;
                data.airspeed_knots = 420.0F;
                data.fuel_level_percent = 15.0F;
                data.engine_temp_celsius = 900.0F;
                data.gps_latitude = 43.5F;
                data.gps_longitude = -79.5F;

                TelemetryPacket* telemetryResponse = create_packet(
                    PACKET_TYPE_TELEMETRY,
                    packet->aircraftID,
                    reinterpret_cast<const char*>(&data),
                    sizeof(TelemetryData));

                if (telemetryResponse != nullptr)
                {
                    send_packet(clientSock, telemetryResponse);
                    free_packet(telemetryResponse);
                }

                TelemetryPacket* ackResponse = create_packet(
                    PACKET_TYPE_ACK_NACK,
                    packet->aircraftID,
                    "ACK",
                    3);

                if (ackResponse != nullptr)
                {
                    send_packet(clientSock, ackResponse);
                    free_packet(ackResponse);
                }
            }
            else if (packet->packetType == PACKET_TYPE_LARGE_DATA)
            {
                char chunkBuf[100];
                std::memset(chunkBuf, 'X', sizeof(chunkBuf));

                for (int i = 0; i < 3; ++i)
                {
                    TelemetryPacket* chunkPacket = create_packet(
                        PACKET_TYPE_LARGE_DATA,
                        packet->aircraftID,
                        chunkBuf,
                        100);

                    if (chunkPacket != nullptr)
                    {
                        send_packet(clientSock, chunkPacket);
                        free_packet(chunkPacket);
                    }
                }

                TelemetryPacket* endPacket = create_packet(
                    PACKET_TYPE_ACK_NACK,
                    packet->aircraftID,
                    "END",
                    3);

                if (endPacket != nullptr)
                {
                    send_packet(clientSock, endPacket);
                    free_packet(endPacket);
                }
            }
            else
            {
                TelemetryPacket* response = create_packet(
                    PACKET_TYPE_ACK_NACK,
                    packet->aircraftID,
                    "NACK",
                    4);

                if (response != nullptr)
                {
                    send_packet(clientSock, response);
                    free_packet(response);
                }
            }

            free_packet(packet);
        }
    }

    close_socket(clientSock);
    close_socket(listenSock);
    cleanup_winsock();
}

namespace AircraftTelemetryTests
{
    TEST_CLASS(IntegrationTests)
    {
    public:
        TEST_METHOD(TestHandshakeIntegration)
        {
            std::atomic<bool> serverReady(false);
            std::atomic<int> receivedType(0);
            std::string serverReceivedID;

            std::thread serverThread(run_mini_server, 9191, std::ref(serverReady), std::ref(receivedType), std::ref(serverReceivedID));

            int waitCount = 0;
            while (!serverReady && waitCount < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++waitCount;
            }

            bool winsockInitialized = init_winsock();
            if (!winsockInitialized)
            {
                serverThread.join();
                Assert::Fail(L"Client winsock initialization failed.");
            }

            SOCKET clientSock = create_client_socket("127.0.0.1", 9191);
            if (clientSock == INVALID_SOCKET)
            {
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Client socket connection failed.");
            }

            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "TESTCRAFT", "AUTH", 4);
            if (packet != nullptr)
            {
                send_packet(clientSock, packet);
                free_packet(packet);
            }

            TelemetryPacket* response = nullptr;
            while (true)
            {
                TelemetryPacket* incoming = receive_packet(clientSock);
                if (incoming == nullptr)
                {
                    break;
                }

                if (incoming->packetType == PACKET_TYPE_LARGE_DATA)
                {
                    free_packet(incoming);
                    continue;
                }

                if (incoming->packetType == PACKET_TYPE_ACK_NACK)
                {
                    response = incoming;
                    break;
                }

                free_packet(incoming);
            }
            close_socket(clientSock);
            cleanup_winsock();
            serverThread.join();

            if (response == nullptr)
            {
                Assert::Fail(L"Expected a server response packet.");
            }

            Assert::AreEqual(PACKET_TYPE_ACK_NACK, static_cast<int>(response->packetType));
            Assert::AreEqual(0, std::memcmp(response->payload, "ACK", 3));
            free_packet(response);

            Assert::AreEqual(PACKET_TYPE_HANDSHAKE, receivedType.load());
            Assert::AreEqual(0, std::strcmp(serverReceivedID.c_str(), "TESTCRAFT"));
        }

        TEST_METHOD(TestInvalidHandshakeGetsNACK)
        {
            std::atomic<bool> serverReady(false);
            std::atomic<int> receivedType(0);
            std::string serverReceivedID;

            std::thread serverThread(run_mini_server, 9192, std::ref(serverReady), std::ref(receivedType), std::ref(serverReceivedID));

            int waitCount = 0;
            while (!serverReady && waitCount < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++waitCount;
            }

            bool winsockInitialized = init_winsock();
            if (!winsockInitialized)
            {
                serverThread.join();
                Assert::Fail(L"Client winsock initialization failed.");
            }

            SOCKET clientSock = create_client_socket("127.0.0.1", 9192);
            if (clientSock == INVALID_SOCKET)
            {
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Client socket connection failed.");
            }

            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "TESTCRAFT", "WRONGAUTH", 9);
            if (packet != nullptr)
            {
                send_packet(clientSock, packet);
                free_packet(packet);
            }

            TelemetryPacket* response = receive_packet(clientSock);
            close_socket(clientSock);
            cleanup_winsock();
            serverThread.join();

            if (response == nullptr)
            {
                Assert::Fail(L"Expected a server response packet.");
            }

            Assert::AreEqual(PACKET_TYPE_ACK_NACK, static_cast<int>(response->packetType));
            Assert::AreEqual(0, std::memcmp(response->payload, "NACK", 4));
            free_packet(response);

            Assert::AreEqual(PACKET_TYPE_HANDSHAKE, receivedType.load());
            Assert::AreEqual(0, std::strcmp(serverReceivedID.c_str(), "TESTCRAFT"));
        }

        TEST_METHOD(TestPacketRoundTrip)
        {
            std::atomic<bool> serverReady(false);
            std::atomic<int> receivedType(0);
            std::string serverReceivedID;

            std::thread serverThread(run_mini_server, 9193, std::ref(serverReady), std::ref(receivedType), std::ref(serverReceivedID));

            int waitCount = 0;
            while (!serverReady && waitCount < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++waitCount;
            }

            bool winsockInitialized = init_winsock();
            if (!winsockInitialized)
            {
                serverThread.join();
                Assert::Fail(L"Client winsock initialization failed.");
            }

            SOCKET clientSock = create_client_socket("127.0.0.1", 9193);
            if (clientSock == INVALID_SOCKET)
            {
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Client socket connection failed.");
            }

            TelemetryPacket* packet = create_packet(PACKET_TYPE_TELEMETRY, "AIRCRAFT1", "GET_TELEMETRY", 13);
            if (packet != nullptr)
            {
                send_packet(clientSock, packet);
                free_packet(packet);
            }

            TelemetryPacket* response = nullptr;
            while (true)
            {
                TelemetryPacket* incoming = receive_packet(clientSock);
                if (incoming == nullptr) break;
                if (incoming->packetType == PACKET_TYPE_TELEMETRY)
                {
                    free_packet(incoming);
                    continue;
                }
                response = incoming;
                break;
            }
            close_socket(clientSock);
            cleanup_winsock();
            serverThread.join();

            if (response == nullptr)
            {
                Assert::Fail(L"Expected a server response packet.");
            }

            Assert::AreEqual(PACKET_TYPE_ACK_NACK, static_cast<int>(response->packetType));
            Assert::AreEqual(0, std::strcmp(response->aircraftID, "AIRCRAFT1"));
            free_packet(response);

            Assert::AreEqual(PACKET_TYPE_TELEMETRY, receivedType.load());
            Assert::AreEqual(0, std::strcmp(serverReceivedID.c_str(), "AIRCRAFT1"));
        }

        TEST_METHOD(TestCreateAndSendMultiplePacketTypes)
        {
            std::atomic<bool> serverReady(false);
            std::atomic<int> receivedType(0);
            std::string serverReceivedID;

            std::thread serverThread(run_mini_server, 9194, std::ref(serverReady), std::ref(receivedType), std::ref(serverReceivedID));

            int waitCount = 0;
            while (!serverReady && waitCount < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++waitCount;
            }

            bool winsockInitialized = init_winsock();
            if (!winsockInitialized)
            {
                serverThread.join();
                Assert::Fail(L"Client winsock initialization failed.");
            }

            SOCKET clientSock = create_client_socket("127.0.0.1", 9194);
            if (clientSock == INVALID_SOCKET)
            {
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Client socket connection failed.");
            }

            TelemetryPacket* packet = create_packet(PACKET_TYPE_LARGE_DATA, "BIGBIRD01", "GET_LARGE_DATA", 14);
            if (packet != nullptr)
            {
                send_packet(clientSock, packet);
                free_packet(packet);
            }

            TelemetryPacket* response = nullptr;
            while (true)
            {
                TelemetryPacket* incoming = receive_packet(clientSock);
                if (incoming == nullptr) break;
                if (incoming->packetType == PACKET_TYPE_LARGE_DATA)
                {
                    free_packet(incoming);
                    continue;
                }
                response = incoming;
                break;
            }
            close_socket(clientSock);
            cleanup_winsock();
            serverThread.join();

            if (response == nullptr)
            {
                Assert::Fail(L"Expected a server response packet.");
            }

            Assert::AreEqual(PACKET_TYPE_ACK_NACK, static_cast<int>(response->packetType));
            Assert::AreEqual(0, std::strcmp(response->aircraftID, "BIGBIRD01"));
            free_packet(response);

            Assert::AreEqual(PACKET_TYPE_LARGE_DATA, receivedType.load());
            Assert::AreEqual(0, std::strcmp(serverReceivedID.c_str(), "BIGBIRD01"));
        }

        TEST_METHOD(TestTelemetryResponseContainsData)
        {
            std::atomic<bool> serverReady(false);
            std::atomic<int> receivedType(0);
            std::string serverReceivedID;

            std::thread serverThread(run_mini_server, 9195, std::ref(serverReady), std::ref(receivedType), std::ref(serverReceivedID));

            int waitCount = 0;
            while (!serverReady && waitCount < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++waitCount;
            }

            bool winsockInitialized = init_winsock();
            if (!winsockInitialized)
            {
                serverThread.join();
                Assert::Fail(L"Client winsock initialization failed.");
            }

            SOCKET clientSock = create_client_socket("127.0.0.1", 9195);
            if (clientSock == INVALID_SOCKET)
            {
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Client socket connection failed.");
            }

            TelemetryPacket* packet = create_packet(PACKET_TYPE_TELEMETRY, "SENSOR01", "GET_TELEMETRY", 13);
            if (packet != nullptr)
            {
                send_packet(clientSock, packet);
                free_packet(packet);
            }

            TelemetryPacket* response = receive_packet(clientSock);
            if (response == nullptr)
            {
                close_socket(clientSock);
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Expected telemetry response packet.");
            }

            Assert::AreEqual(PACKET_TYPE_TELEMETRY, static_cast<int>(response->packetType));
            Assert::AreEqual(static_cast<int>(sizeof(TelemetryData)), static_cast<int>(response->dataSize));

            const TelemetryData* data = reinterpret_cast<const TelemetryData*>(response->payload);
            Assert::AreEqual(35000.0f, data->altitude_ft);
            Assert::AreEqual(15.0f, data->fuel_level_percent);
            Assert::IsTrue(data->fuel_level_percent < FUEL_LEVEL_MIN_PERCENT);
            free_packet(response);

            TelemetryPacket* ackPacket = receive_packet(clientSock);
            if (ackPacket != nullptr)
            {
                free_packet(ackPacket);
            }

            close_socket(clientSock);
            cleanup_winsock();
            serverThread.join();
        }

        TEST_METHOD(TestLargeDataTransferReceivesChunks)
        {
            std::atomic<bool> serverReady(false);
            std::atomic<int> receivedType(0);
            std::string serverReceivedID;

            std::thread serverThread(run_mini_server, 9196, std::ref(serverReady), std::ref(receivedType), std::ref(serverReceivedID));

            int waitCount = 0;
            while (!serverReady && waitCount < 100)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++waitCount;
            }

            bool winsockInitialized = init_winsock();
            if (!winsockInitialized)
            {
                serverThread.join();
                Assert::Fail(L"Client winsock initialization failed.");
            }

            SOCKET clientSock = create_client_socket("127.0.0.1", 9196);
            if (clientSock == INVALID_SOCKET)
            {
                cleanup_winsock();
                serverThread.join();
                Assert::Fail(L"Client socket connection failed.");
            }

            TelemetryPacket* packet = create_packet(PACKET_TYPE_LARGE_DATA, "BIGDATA1", "GET_LARGE_DATA", 14);
            if (packet != nullptr)
            {
                send_packet(clientSock, packet);
                free_packet(packet);
            }

            int chunkCount = 0;
            int totalBytes = 0;

            while (true)
            {
                TelemetryPacket* incoming = receive_packet(clientSock);
                if (incoming == nullptr)
                {
                    break;
                }

                if (incoming->packetType == PACKET_TYPE_LARGE_DATA)
                {
                    ++chunkCount;
                    totalBytes += incoming->dataSize;
                    free_packet(incoming);
                    continue;
                }

                if (incoming->packetType == PACKET_TYPE_ACK_NACK &&
                    incoming->payload != nullptr &&
                    incoming->dataSize == 3 &&
                    std::memcmp(incoming->payload, "END", 3) == 0)
                {
                    free_packet(incoming);
                    break;
                }

                free_packet(incoming);
            }

            close_socket(clientSock);
            cleanup_winsock();
            serverThread.join();

            Assert::AreEqual(3, chunkCount);
            Assert::AreEqual(300, totalBytes);
        }
    };
}
