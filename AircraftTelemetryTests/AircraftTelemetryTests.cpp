#include "pch.h"
#include "CppUnitTest.h"
#include "../Client/thresholds.h"
#include "../Shared/packet.h"
using namespace Thresholds;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace AircraftTelemetryTests
{
    TEST_CLASS(ClientTests)
    {
    public:
        TEST_METHOD(TestThresholdConstants)
        {
            Assert::AreEqual(20.0F, FUEL_LEVEL_MIN_PERCENT);
            Assert::AreEqual(950.0F, ENGINE_TEMP_MAX_CELSIUS);
            Assert::AreEqual(45000.0F, ALTITUDE_MAX_FEET);
            Assert::AreEqual(500.0F, AIRSPEED_MAX_KNOTS);
        }

        TEST_METHOD(TestTelemetryDataStructSize)
        {
            Assert::AreEqual(24u, static_cast<unsigned int>(sizeof(TelemetryData)));
        }

        TEST_METHOD(TestTelemetryDataFieldAssignment)
        {
            TelemetryData data{};
            data.altitude_ft = 35000.0F;
            data.airspeed_knots = 420.0F;
            data.fuel_level_percent = 65.5F;
            data.engine_temp_celsius = 875.0F;
            data.gps_latitude = 37.7749F;
            data.gps_longitude = -122.4194F;

            Assert::AreEqual(35000.0F, data.altitude_ft);
            Assert::AreEqual(420.0F, data.airspeed_knots);
            Assert::AreEqual(65.5f, data.fuel_level_percent);
            Assert::AreEqual(875.0F, data.engine_temp_celsius);
            Assert::AreEqual(37.7749F, data.gps_latitude);
            Assert::AreEqual(-122.4194F, data.gps_longitude);
        }

        TEST_METHOD(TestFuelWarningTriggered)
        {
            TelemetryData data{};
            data.fuel_level_percent = 10.0F;

            Assert::IsTrue(data.fuel_level_percent < FUEL_LEVEL_MIN_PERCENT);
        }

        TEST_METHOD(TestFuelWarningNotTriggered)
        {
            TelemetryData data{};
            data.fuel_level_percent = 50.0F;

            Assert::IsFalse(data.fuel_level_percent < FUEL_LEVEL_MIN_PERCENT);
        }

        TEST_METHOD(TestEngineTempWarningTriggered)
        {
            TelemetryData data{};
            data.engine_temp_celsius = 1000.0F;

            Assert::IsTrue(data.engine_temp_celsius > ENGINE_TEMP_MAX_CELSIUS);
        }

        TEST_METHOD(TestAltitudeWarningTriggered)
        {
            TelemetryData data{};
            data.altitude_ft = 50000.0F;

            Assert::IsTrue(data.altitude_ft > ALTITUDE_MAX_FEET);
        }

        TEST_METHOD(TestAirspeedWarningTriggered)
        {
            TelemetryData data{};
            data.airspeed_knots = 600.0F;

            Assert::IsTrue(data.airspeed_knots > AIRSPEED_MAX_KNOTS);
        }
    };

    TEST_CLASS(ServerTests)
    {
    public:
        TEST_METHOD(TestPacketTypeConstants)
        {
            Assert::AreEqual(1, PACKET_TYPE_HANDSHAKE);
            Assert::AreEqual(2, PACKET_TYPE_TELEMETRY);
            Assert::AreEqual(3, PACKET_TYPE_LARGE_DATA);
            Assert::AreEqual(4, PACKET_TYPE_CMD_RESPONSE);
            Assert::AreEqual(5, PACKET_TYPE_ACK_NACK);
        }

        TEST_METHOD(TestCreatePacketNotNull)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "SERVER001", "AUTH", 4);

            Assert::IsNotNull(packet);
            free_packet(packet);
        }

        TEST_METHOD(TestCreatePacketType)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_TELEMETRY, "SERVER001", "DATA", 4);

            Assert::IsNotNull(packet);
            Assert::AreEqual(PACKET_TYPE_TELEMETRY, packet->packetType);
            free_packet(packet);
        }

        TEST_METHOD(TestCreatePacketDataSize)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "SERVER001", "AUTH", 4);

            Assert::IsNotNull(packet);
            Assert::AreEqual(4, packet->dataSize);
            free_packet(packet);
        }

        TEST_METHOD(TestCreatePacketAircraftID)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "SERVER001", "AUTH", 4);

            Assert::IsNotNull(packet);
            Assert::AreEqual(0, strcmp(packet->aircraftID, "SERVER001"));
            free_packet(packet);
        }

        TEST_METHOD(TestCreatePacketPayloadContent)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "SERVER001", "AUTH", 4);

            Assert::IsNotNull(packet);
            Assert::AreEqual(0, memcmp(packet->payload, "AUTH", 4));
            free_packet(packet);
        }

        TEST_METHOD(TestHandshakeAuthValidation)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "SERVER001", "AUTH", 4);
            Assert::IsNotNull(packet);

            bool valid = (packet->dataSize == 4 &&
                packet->payload != nullptr &&
                memcmp(packet->payload, "AUTH", 4) == 0);

            Assert::IsTrue(valid);
            free_packet(packet);
        }

        TEST_METHOD(TestHandshakeInvalidPayload)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_HANDSHAKE, "SERVER001", "BADAUTH", 7);
            Assert::IsNotNull(packet);

            bool valid = (packet->dataSize == 4 &&
                packet->payload != nullptr &&
                memcmp(packet->payload, "AUTH", 4) == 0);

            Assert::IsFalse(valid);
            free_packet(packet);
        }

        TEST_METHOD(TestFreePacketNullPayload)
        {
            TelemetryPacket* packet = create_packet(PACKET_TYPE_ACK_NACK, "SERVER001", nullptr, 0);

            Assert::IsNotNull(packet);
            free_packet(packet);
        }

        TEST_METHOD(TestAircraftIDSizeConstant)
        {
            Assert::AreEqual(16, AIRCRAFT_ID_SIZE);
        }
    };
}
