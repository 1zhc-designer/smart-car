#include "DHT11.hpp"
#include <wiringPi.h>
#include <cstdio>

DHT11::DHT11(int pin)
    : m_pin(pin)
{
    resetData();
}

void DHT11::resetData()
{
    for (int & i : m_data)
        i = 0;
}

bool DHT11::read()
{
    uint8_t lastState = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0;

    resetData();

    pinMode(m_pin, OUTPUT);
    digitalWrite(m_pin, LOW);
    delay(18);

    digitalWrite(m_pin, HIGH);
    delayMicroseconds(40);

    pinMode(m_pin, INPUT);

    for (int i = 0; i < MAXTIMINGS; ++i)
    {
        counter = 0;

        while (digitalRead(m_pin) == lastState)
        {
            counter++;
            delayMicroseconds(1);
            if (counter == 255)
                break;
        }

        lastState = digitalRead(m_pin);

        if (counter == 255)
            break;

        if ((i >= 4) && (i % 2 == 0))
        {
            m_data[j / 8] <<= 1;
            if (counter > 16)
                m_data[j / 8] |= 1;
            j++;
        }
    }

    if ((j >= 40) &&
        (m_data[4] ==
         ((m_data[0] + m_data[1] + m_data[2] + m_data[3]) & 0xFF)))
    {
        return true;
    }

    return false;
}

int DHT11::getHumidityInt() const
{
    return m_data[0];
}

int DHT11::getHumidityDec() const
{
    return m_data[1];
}

int DHT11::getTempInt() const
{
    return m_data[2];
}

int DHT11::getTempDec() const
{
    return m_data[3];
}

float DHT11::getTempF() const
{
    return m_data[2] * 9.0f / 5.0f + 32.0f;
}