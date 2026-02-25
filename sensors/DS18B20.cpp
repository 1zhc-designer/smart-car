<<<<<<< Updated upstream
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <dirent.h>
#include <cstring>
#include <unistd.h>

// Read CPU temperature (returns Celsius, -1000.0 on failure)
double readCPUTemperature() {
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (!file.is_open()) {
        std::cerr << "Unable to open CPU temperature file" << std::endl;
        return -1000.0;
    }
    int rawTemp;
    file >> rawTemp;
    file.close();
    return rawTemp / 1000.0;
}

// Scan and return the temperature from the first DS18B20 sensor (Celsius)
// Returns -1000.0 if no sensor found or read fails
double readDS18B20Temperature() {
    const char* baseDir = "/sys/bus/w1/devices/";
    DIR* dir = opendir(baseDir);
    if (!dir) {
        std::cerr << "Unable to open 1-Wire device directory" << std::endl;
        return -1000.0;
    }

    struct dirent* entry;
    std::string sensorFolder;
    bool found = false;

    // Look for a folder starting with "28-" (DS18B20 device prefix)
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "28-", 3) == 0) {
            sensorFolder = entry->d_name;
            found = true;
            break;
        }
    }
    closedir(dir);

    if (!found) {
        std::cerr << "No DS18B20 sensor found" << std::endl;
        return -1000.0;
    }

    // Read the sensor data file
    std::string filePath = std::string(baseDir) + sensorFolder + "/w1_slave";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Unable to open sensor data file" << std::endl;
        return -1000.0;
    }

    std::string line;
    bool crcValid = false;
    int tempRaw = 0;

    // Parse the two lines
    while (std::getline(file, line)) {
        // First line should end with "YES" indicating CRC passed
        if (line.find("YES") != std::string::npos) {
            crcValid = true;
        }
        // Second line contains "t=" followed by the temperature value
        size_t pos = line.find("t=");
        if (pos != std::string::npos) {
            tempRaw = std::stoi(line.substr(pos + 2));
        }
    }
    file.close();

    if (!crcValid || tempRaw == 0) {
        std::cerr << "Sensor data CRC check failed" << std::endl;
        return -1000.0;
    }

    // Temperature value is in 0.001 degree Celsius units
    return tempRaw / 1000.0;
}

int main() {
    std::cout << "=== Raspberry Pi Temperature Reading Example ===" << std::endl;

    // Read CPU temperature
    double cpuTemp = readCPUTemperature();
    if (cpuTemp > -100) {
        std::cout << "CPU temperature: " << cpuTemp << " °C" << std::endl;
    }

    // Read DS18B20 temperature
    double sensorTemp = readDS18B20Temperature();
    if (sensorTemp > -100) {
        std::cout << "DS18B20 temperature: " << sensorTemp << " °C" << std::endl;
    }

    return 0;
=======
#include "DS18B20.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

static const std::string W1_BASE_PATH = "/sys/bus/w1/devices/";

DS18B20::DS18B20(const std::string& device_id)
    : device_id_(device_id)
{
    if (device_id_.empty())
    {
        if (!discoverDevice())
        {
            std::cerr << "DS18B20 not found\n";
        }
    }

    device_file_ = W1_BASE_PATH + device_id_ + "/w1_slave";
}

bool DS18B20::discoverDevice()
{
    for (const auto& entry : fs::directory_iterator(W1_BASE_PATH))
    {
        std::string name = entry.path().filename().string();

        // DS18B20 的 family code 是 28-
        if (name.rfind("28-", 0) == 0)
        {
            device_id_ = name;
            return true;
        }
    }
    return false;
}

std::optional<double> DS18B20::readTemperatureC()
{
    std::ifstream file(device_file_);
    if (!file.is_open())
    {
        std::cerr << "Failed to open " << device_file_ << "\n";
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    double tempC;
    if (!parseTemperature(content, tempC))
        return std::nullopt;

    return tempC;
}

bool DS18B20::parseTemperature(const std::string& content, double& tempC)
{
    // 文件格式类似：
    // 3c 01 4b 46 7f ff 0c 10 5e : crc=5e YES
    // 3c 01 4b 46 7f ff 0c 10 5e t=19875

    if (content.find("YES") == std::string::npos)
        return false;

    auto pos = content.find("t=");
    if (pos == std::string::npos)
        return false;

    std::string temp_str = content.substr(pos + 2);
    try
    {
        int milliC = std::stoi(temp_str);
        tempC = milliC / 1000.0;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::string DS18B20::getDevicePath() const
{
    return device_file_;
>>>>>>> Stashed changes
}