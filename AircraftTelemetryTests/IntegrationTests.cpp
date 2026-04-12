#include "pch.h"
#include "CppUnitTest.h"
#include "../Shared/packet.h"
#include "../Shared/socket.h"
#include "../Shared/logger.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

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

            bool isValidHandshake =
                packet->packetType == PACKET_TYPE_HANDSHAKE &&
                packet->payload != nullptr &&
                packet->dataSize == 4 &&
                std::memcmp(packet->payload, "AUTH", 4) == 0;

            const char* responsePayload = isValidHandshake ? "ACK" : "NACK";
            int responseSize = isValidHandshake ? 3 : 4;

            TelemetryPacket* response = create_packet(
                PACKET_TYPE_ACK_NACK,
                packet->aircraftID,
                responsePayload,
                responseSize);

            if (response != nullptr)
            {
                send_packet(clientSock, response);
                free_packet(response);
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

            TelemetryPacket* response = receive_packet(clientSock);
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

            TelemetryPacket* response = receive_packet(clientSock);
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

            TelemetryPacket* response = receive_packet(clientSock);
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
    };
}
