#include "logger.h"

#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::ofstream s_logFile;

static std::string current_timestamp()
{
    time_t now = time(nullptr);
    struct tm timeinfo = {};
    localtime_s(&timeinfo, &now);

    char buf[12];
    strftime(buf, sizeof(buf), "[%H:%M:%S]", &timeinfo);
    return std::string(buf);
}

void init_logger(const std::string& logFileName)
{
    if (s_logFile.is_open()) s_logFile.close(); 
    s_logFile.open(logFileName, std::ios::out | std::ios::app);
    if (!s_logFile.is_open())
    {
        std::cerr << "WARNING: Could not open log file: " << logFileName << std::endl;
    }
}

void close_logger()
{
    if (s_logFile.is_open())
    {
        s_logFile.close();
    }
}

void log_event(const std::string& message)
{
    std::string ts = current_timestamp();
    std::string line = ts + " " + message;

    if (s_logFile.is_open())
    {
        s_logFile << line << "\n";
        s_logFile.flush();
    }
    std::cout << line << "\n";
}

void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID)
{
    std::string dir = isTX ? "[TX]" : "[RX]";

    std::ostringstream oss;
    oss << dir
        << " PacketType=" << packetType
        << " DataSize=" << dataSize
        << " AircraftID=" << (aircraftID ? aircraftID : "");

    log_event(oss.str());
}
