#ifndef DHT11_HPP
#define DHT11_HPP

#include <cstdint>

class DHT11
{
public:
    explicit DHT11(int pin);

    bool read();                 // 读取数据
    int getHumidityInt() const;  // 湿度整数部分
    int getHumidityDec() const;  // 湿度小数部分
    int getTempInt() const;      // 温度整数部分
    int getTempDec() const;      // 温度小数部分
    float getTempF() const;      // 华氏温度

private:
    static constexpr int MAXTIMINGS = 85;

    int m_pin;
    int m_data[5];

    void resetData();
};

#endif