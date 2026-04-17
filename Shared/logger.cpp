#include "logger.h"

#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

// MISRA-CPP-2008-7-3-1: wrap in namespace
namespace Logger
{

static std::ofstream s_logFile;

static std::string current_timestamp()
{
    time_t now = time(nullptr);
    struct tm timeinfo = {};

    // MISRA-CPP-2008-0-1-7: use return value
    (void)localtime_s(&timeinfo, &now);

    char buf[12];
    // MISRA-CPP-2008-0-1-7: use return value of strftime
    (void)strftime(buf, sizeof(buf), "[%H:%M:%S]", &timeinfo);
    return std::string(buf);
}

void init_logger(const std::string& logFileName)
{
    if (s_logFile.is_open())
    {
        s_logFile.close();
    }
    s_logFile.open(logFileName, std::ios::out | std::ios::app);
    if (!s_logFile.is_open())
    {
        std::cerr << "WARNING: Could not open log file: " << logFileName << "\n";
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
    std::string ts   = current_timestamp();
    std::string line = ts + " " + message;

    if (s_logFile.is_open())
    {
        s_logFile << line << "\n";
        // MISRA-CPP-2008-0-1-7: use return value of flush
        (void)s_logFile.flush();
    }
    std::cout << line << "\n";
}

void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID)
{
    const std::string dir = isTX ? "[TX]" : "[RX]";

    std::ostringstream oss;
    oss << dir
        << " PacketType=" << packetType
        << " DataSize="   << dataSize
        << " AircraftID=" << ((aircraftID != nullptr) ? aircraftID : "");

    log_event(oss.str());
}

} // namespace Logger

// Pull names into global scope so existing call-sites still compile
void init_logger(const std::string& logFileName) { Logger::init_logger(logFileName); }
void close_logger()                               { Logger::close_logger(); }
void log_event(const std::string& message)        { Logger::log_event(message); }
void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID)
{
    Logger::log_packet(isTX, packetType, dataSize, aircraftID);
}
