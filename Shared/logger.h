#pragma once

#include <string>

// MISRA-CPP-2008-7-3-1: declarations belong in a namespace
namespace Logger
{
    void init_logger(const std::string& logFileName);
    void close_logger();
    void log_event(const std::string& message);
    void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID);
}

// Convenience wrappers so existing code compiles without change
void init_logger(const std::string& logFileName);
void close_logger();
void log_event(const std::string& message);
void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID);
