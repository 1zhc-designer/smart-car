/**********************************************
# -------- Hunan Makerobo Intelligent Tech --------
# File Name: 37_rain.c
# Version: V1.0
# Author: Makerobo
# Description: Rain Sensor Detection Test
**********************************************/

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pcf8591.h>

#define makerobo_DOpin 0       // Digital output pin (wiringPi numbering)
#define makerobo_PCF 120       // PCF8591 base pin number

int main(void)
{
    int analogVal;
    int digitalVal;
    int lastDigitalVal = HIGH;

    if (wiringPiSetup() == -1)
        return 1;

    // Initialize PCF8591 at I2C address 0x48
    pcf8591Setup(makerobo_PCF, 0x48);

    pinMode(makerobo_DOpin, INPUT);

    while (1)
    {
        // Read analog value from AIN0
        analogVal = analogRead(makerobo_PCF + 0);

        // Read digital output
        digitalVal = digitalRead(makerobo_DOpin);

        printf("Analog Value: %d\n", analogVal);

        // Print message only when state changes
        if (digitalVal != lastDigitalVal)
        {
            if (digitalVal == LOW)
                printf("Rain detected!\n");
            else
                printf("No rain detected.\n");

            lastDigitalVal = digitalVal;
        }

        delay(200);
    }

    return 0;
}