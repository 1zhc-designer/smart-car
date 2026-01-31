#include <wiringPi.h>
#include <cstdio>    // 代替 stdio.h
#include <cstdlib>   // 用于 exit()

// 左电机引脚 (wiringPi 编号)
#define L_PWM 1
#define L_IN1 3
#define L_IN2 4

// 右电机引脚 (wiringPi 编号)
#define R_PWM 24
#define R_IN1 5
#define R_IN2 6

// 初始化硬件
void setup() {
    if (wiringPiSetup() == -1) {
        printf("[Error] WiringPi initialization failed!\n");
        exit(1);
    }

    // 设置所有引脚为输出模式 [cite: 385]
    pinMode(L_PWM, OUTPUT);
    pinMode(L_IN1, OUTPUT);
    pinMode(L_IN2, OUTPUT);
    pinMode(R_PWM, OUTPUT);
    pinMode(R_IN1, OUTPUT);
    pinMode(R_IN2, OUTPUT);

    printf("[System] Hardware initialization is complete and ready.\n");
}

//移动控制
//parameter l_speed 左轮速度 (0-255)
//parameter r_speed 右轮速度 (0-255)
void moveForward(int l_speed, int r_speed) {
    // 左电机前进
    digitalWrite(L_IN1, HIGH);
    digitalWrite(L_IN2, LOW);
    analogWrite(L_PWM, l_speed);

    // 右电机前进
    digitalWrite(R_IN1, HIGH);
    digitalWrite(R_IN2, LOW);
    analogWrite(R_PWM, r_speed);

    printf("[Action] Car moves forward: Left speed=%d, Right speed=%d\n", l_speed, r_speed);
}

void stopCar() {
    digitalWrite(L_IN1, LOW);
    digitalWrite(L_IN2, LOW);
    digitalWrite(R_IN1, LOW);
    digitalWrite(R_IN2, LOW);
    analogWrite(L_PWM, 0);
    analogWrite(R_PWM, 0);
    printf("[Action] Car stops.\n");
}

int main() {
    setup();

    moveForward(150, 150);
    delay(2000); // 持续 2 秒 [cite: 500]

    stopCar();

    return 0;
}