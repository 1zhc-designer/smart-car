/*************************************************
 * Company: Hunan Chuangyue Intelligent Technology Co., Ltd.
 * File name: 25_ds18b20.c
 * Version: V2.0
 * Author: zhulin
 * Description: DS18B20 Temperature Sensor Example
 *************************************************/

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    DIR *dir;
    struct dirent *dirent;
    char dev[16];               // Device ID
    char devPath[128];          // Device file path
    char buf[256];              // Raw device data buffer
    char tmpData[6];            // Temperature string buffer (Temp * 1000)
    char path[] = "/sys/bus/w1/devices/";   // 1-Wire device directory
    ssize_t numRead;

    /* Open device directory */
    dir = opendir(path);
    if (dir != NULL)
    {
        /* Search for device folder starting with "28-" (DS18B20 family code) */
        while ((dirent = readdir(dir)) != NULL)
        {
            if (dirent->d_type == DT_LNK &&
                strstr(dirent->d_name, "28-") != NULL)
            {
                strcpy(dev, dirent->d_name);
                printf("\nDevice: %s\n", dev);
            }
        }
        closedir(dir);   // Close directory
    }
    else
    {
        perror("Couldn't open the w1 devices directory");
        return 1;
    }

    /* Construct device file path */
    sprintf(devPath, "%s%s/w1_slave", path, dev);

    /* Continuously read temperature */
    while (1)
    {
        int fd = open(devPath, O_RDONLY);   // Open device file
        if (fd == -1)
        {
            perror("Couldn't open the w1 device");
            return 1;
        }

        /* Read sensor data */
        while ((numRead = read(fd, buf, sizeof(buf))) > 0)
        {
            buf[numRead] = '\0';

            /* Extract temperature value after "t=" */
            char *tempPos = strstr(buf, "t=");
            if (tempPos != NULL)
            {
                strncpy(tmpData, tempPos + 2, 5);
                tmpData[5] = '\0';

                float tempC = strtof(tmpData, NULL) / 1000.0;   // Convert to Celsius
                float tempF = tempC * 9.0 / 5.0 + 32.0;         // Convert to Fahrenheit

                printf("Temp: %.3f °C  |  %.3f °F\n", tempC, tempF);
            }
        }

        close(fd);   // Close device file
        sleep(1);    // Read every 1 second
    }

    return 0;
}