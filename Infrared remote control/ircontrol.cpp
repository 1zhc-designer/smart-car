#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <wiringPi.h>
#include <softPwm.h>
#include <lirc/lirc_client.h>

const int PWMA = 1;
const int AIN2 = 2;
const int AIN1 = 3;
const int PWMB = 4;
const int BIN2 = 5;
const int BIN1 = 6;

void t_up(int speed) {
    digitalWrite(AIN2, 0); 
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, speed);
    digitalWrite(BIN2, 0); 
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, speed);
}

void t_down(int speed) {
    digitalWrite(AIN2, 1); 
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, speed);
    digitalWrite(BIN2, 1); 
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, speed);
}

void t_left(int speed) {
    digitalWrite(AIN2, 1); 
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, speed);
    digitalWrite(BIN2, 0); 
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, speed);
}

void t_right(int speed) {
    digitalWrite(AIN2, 0); 
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, speed);
    digitalWrite(BIN2, 1); 
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, speed);
}

void t_stop() {
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
    digitalWrite(AIN2, 0); 
    digitalWrite(AIN1, 0);
    digitalWrite(BIN2, 0); 
    digitalWrite(BIN1, 0);
}

void handle_ir_command(char *code) {
    if (strstr(code, "KEY_CHANNEL")) {
        t_up(50);
        printf("Command: Forward\n");
    } 
    else if (strstr(code, "KEY_VOLUMEUP")) {
        t_down(50);
        printf("Command: Backward\n");
    } 
    else if (strstr(code, "KEY_PREVIOUS")) {
        t_left(50);
        printf("Command: Turn Left\n");
    }
    else if (strstr(code, "KEY_PLAYPAUSE")) {
        t_right(50);
        printf("Command: Turn Right\n");
    }
    else if (strstr(code, "KEY_NEXT")) {
        t_stop();
        printf("Command: Stop\n");
    }
}

int main() {
    if (wiringPiSetup() == -1) return 1;

    int pins[] = {PWMA, AIN2, AIN1, PWMB, BIN2, BIN1};
    for (int p : pins) pinMode(p, OUTPUT);
    
    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);

    struct lirc_config *config;
    if (lirc_init("ircontrol", 1) == -1) {
        printf("LIRC initialization failed!\n");
        exit(EXIT_FAILURE);
    }

    char *code;
    unsigned int lastButtonTime = 0;

    printf("IR Robot Control Ready...\n");

    if (lirc_readconfig(NULL, &config, NULL) == 0) {
        while (lirc_nextcode(&code) == 0) {
            if (code == NULL) continue;
            
            if (millis() - lastButtonTime > 400) {
                handle_ir_command(code);
                lastButtonTime = millis();
            }
            free(code);
        }
        lirc_freeconfig(config);
    }

    lirc_deinit();
    return 0;
}