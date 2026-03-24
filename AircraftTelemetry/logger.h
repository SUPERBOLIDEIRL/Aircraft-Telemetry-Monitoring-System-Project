#pragma once

#include <string>

void init_logger(const std::string& logFileName);
void close_logger();
void log_event(const std::string& message);
void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID);