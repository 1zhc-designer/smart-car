#ifndef DS18B20_HPP
#define DS18B20_HPP

#include <string>
#include <optional>

class DS18B20
{
public:
    // 如果只有一个 DS18B20，可以不传 id 自动查找
    explicit DS18B20(const std::string& device_id = "");

    // 读取摄氏温度
    // 成功返回温度值（°C）
    // 失败返回 std::nullopt
    std::optional<double> readTemperatureC();

    std::string getDevicePath() const;

private:
    std::string device_id_;
    std::string device_file_;

    bool discoverDevice();
    bool parseTemperature(const std::string& content, double& tempC);
};

#endif