/**********************************************
# -------- Hunan Makerobo Intelligent Tech --------
# File Name: 27_humiture.c
# Version: V2.0
# Author: zhulin
# Description: DHT11 Temperature and Humidity Test
**********************************************/

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAXTIMINGS 85          // Maximum timing count
#define makerobo_DHTPIN 0      // DHT11 data pin (wiringPi numbering)

int makerobo_dht11_dat[5] = {0, 0, 0, 0, 0};  // Data buffer

// Function to read DHT11 sensor data
void makerobo_read_dht11_dat(void)
{
    uint8_t laststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;
    float fahrenheit;

    // Reset data buffer
    makerobo_dht11_dat[0] = makerobo_dht11_dat[1] =
    makerobo_dht11_dat[2] = makerobo_dht11_dat[3] =
    makerobo_dht11_dat[4] = 0;

    // Pull pin low for at least 18ms to start communication
    pinMode(makerobo_DHTPIN, OUTPUT);
    digitalWrite(makerobo_DHTPIN, LOW);
    delay(18);

    // Pull pin high for 40 microseconds
    digitalWrite(makerobo_DHTPIN, HIGH);
    delayMicroseconds(40);

    // Set pin to input mode to read data
    pinMode(makerobo_DHTPIN, INPUT);

    // Read data stream
    for (i = 0; i < MAXTIMINGS; i++)
    {
        counter = 0;
        while (digitalRead(makerobo_DHTPIN) == laststate)
        {
            counter++;
            delayMicroseconds(1);
            if (counter == 255)
                break;
        }

        laststate = digitalRead(makerobo_DHTPIN);

        if (counter == 255)
            break;

        // Ignore first 3 transitions
        if ((i >= 4) && (i % 2 == 0))
        {
            makerobo_dht11_dat[j / 8] <<= 1;
            if (counter > 16)
                makerobo_dht11_dat[j / 8] |= 1;
            j++;
        }
    }

    // Verify 40 bits received and checksum matches
    if ((j >= 40) &&
        (makerobo_dht11_dat[4] ==
         ((makerobo_dht11_dat[0] +
           makerobo_dht11_dat[1] +
           makerobo_dht11_dat[2] +
           makerobo_dht11_dat[3]) & 0xFF)))
    {
        fahrenheit = makerobo_dht11_dat[2] * 9. / 5. + 32;

        printf("Humidity = %d.%d %% Temperature = %d.%d °C (%.1f °F)\n",
               makerobo_dht11_dat[0],
               makerobo_dht11_dat[1],
               makerobo_dht11_dat[2],
               makerobo_dht11_dat[3],
               fahrenheit);
    }
}

// Main function
int main(void)
{
    printf("Makerobo Raspberry Pi wiringPi DHT11 Test Program\n");

    if (wiringPiSetup() == -1)
        exit(1);

    while (1)
    {
        makerobo_read_dht11_dat();
        delay(1000);  // Refresh every 1 second
    }

    return 0;
}