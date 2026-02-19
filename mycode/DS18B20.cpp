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
}