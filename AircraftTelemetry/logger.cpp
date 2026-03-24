#include "logger.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>

static std::ofstream logFile;
static std::string logFileName;

void init_logger(const std::string& fileName) {
    logFileName = fileName;
    logFile.open(fileName, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << fileName << std::endl;
    }
}

void close_logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void log_event(const std::string& message) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);
    if (logFile.is_open()) {
        logFile << "[" << timeBuf << "] " << message << std::endl;
    }
    std::cout << "[" << timeBuf << "] " << message << std::endl;
}

void log_packet(bool isTX, int packetType, int dataSize, const char* aircraftID) {
    std::string dir = isTX ? "TX" : "RX";
    std::string msg = dir + " PacketType=" + std::to_string(packetType) +
        " DataSize=" + std::to_string(dataSize) +
        " AircraftID=" + std::string(aircraftID);
    log_event(msg);
}