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

        // The family code of DS18B20 devices starts with "28-"
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
    // The file format looks like:
    // 3c 01 4b 46 7f ff 0c 10 5e : crc=5e YES
    // 3c 01 4b 46 7f ff 0c 10 5e t=19875

    // Verify CRC check passed
    if (content.find("YES") == std::string::npos)
        return false;

    // Find temperature value after "t="
    auto pos = content.find("t=");
    if (pos == std::string::npos)
        return false;

    std::string temp_str = content.substr(pos + 2);
    try
    {
        int milliC = std::stoi(temp_str);
        tempC = milliC / 1000.0;  // Convert from millidegree Celsius to Celsius
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
}