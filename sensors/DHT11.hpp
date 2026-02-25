#ifndef DHT11_HPP
#define DHT11_HPP

#include <cstdint>

class DHT11
{
public:
    explicit DHT11(int pin);

    bool read();                 // Read data from the sensor
    int getHumidityInt() const;  // Integer part of humidity
    int getHumidityDec() const;  // Decimal part of humidity
    int getTempInt() const;      // Integer part of temperature
    int getTempDec() const;      // Decimal part of temperature
    float getTempF() const;      // Temperature in Fahrenheit

private:
    static constexpr int MAXTIMINGS = 85;

    int m_pin;
    int m_data[5];

    void resetData();            // Reset internal data buffer
};

#endif