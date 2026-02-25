#ifndef DS18B20_HPP
#define DS18B20_HPP

#include <string>
#include <optional>

class DS18B20
{
public:
    // If there is only one DS18B20 device, the ID can be omitted
    // and it will be discovered automatically
    explicit DS18B20(const std::string& device_id = "");

    // Read temperature in Celsius
    // Returns temperature value (°C) on success
    // Returns std::nullopt on failure
    std::optional<double> readTemperatureC();

    std::string getDevicePath() const;

private:
    std::string device_id_;
    std::string device_file_;

    bool discoverDevice();
    bool parseTemperature(const std::string& content, double& tempC);
};

#endif