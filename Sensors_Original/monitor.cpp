#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pcf8591.h>
#include <math.h>

// ================= GPIO (BCM numbering) =================
#define LED_R   20   // BCM20 (Pin 38)
#define LED_G   21   // BCM21 (Pin 40)

#define BUZZER  17   // BCM17 (Pin 36)

#define PCF_BASE 120

// PCF channels
#define AIN_Y   (PCF_BASE + 0)
#define AIN_X   (PCF_BASE + 1)
#define AIN_SW  (PCF_BASE + 2)
#define AIN_NTC (PCF_BASE + 3)

typedef unsigned char uchar;

// ================= Buzzer =================
void buzzerInit()
{
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, HIGH);  // OFF (LOW active)
}

void buzzerOn()  { digitalWrite(BUZZER, LOW); }
void buzzerOff() { digitalWrite(BUZZER, HIGH); }

void buzzerBeep(int t)
{
    buzzerOn();
    delay(t);
    buzzerOff();
    delay(t);
}

// ================= LED =================
// LOW = ON (common-anode style, same as your original logic)
void ledInit()
{
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
}

void setLED(int r_state, int g_state)
{
    digitalWrite(LED_R, r_state);
    digitalWrite(LED_G, g_state);
}

// ================= Joystick =================
uchar readJoystick()
{
    uchar js = 0;
    uchar x = (uchar)analogRead(AIN_X);
    uchar y = (uchar)analogRead(AIN_Y);
    uchar sw = (uchar)analogRead(AIN_SW);

    if (x >= 250) js = 2;
    if (x <= 5)   js = 1;
    if (y >= 250) js = 4;
    if (y <= 5)   js = 3;

    if ((int)x - 127 < 30 && (int)x - 127 > -30 &&
        (int)y - 127 < 30 && (int)y - 127 > -30 &&
        sw > 127)
    {
        js = 0;
    }

    return js;
}

// ================= NTC =================
double readNTC()
{
    const double Vref = 5.0;
    const double R0   = 10000.0;
    const double B    = 3950.0;
    const double T0   = 298.15;
    const double Rser = 10000.0;

    unsigned char adc = (unsigned char)analogRead(AIN_NTC);

    double Vr = Vref * adc / 255.0;
    if (Vr <= 0.000001) Vr = 0.000001;
    if (Vr >= Vref - 0.000001) Vr = Vref - 0.000001;

    double Rt = Rser * Vr / (Vref - Vr);
    double Tk = 1.0 / ((log(Rt/R0)/B) + (1.0/T0));

    return Tk - 273.15;
}

// ================= Main =================
int main(void)
{
    int i;
    uchar joy = 0;
    double temp = 0.0;

    uchar low_limit  = 26;
    uchar high_limit = 30;

    if (wiringPiSetupGpio() == -1)
        return 1;

    pcf8591Setup(PCF_BASE, 0x48);

    ledInit();
    buzzerInit();

    printf("System running...\n");
    printf("LED: High=RED, Low=YELLOW, Normal=GREEN\n");
    printf("Joystick press disabled (no exit)\n");

    while (1)
    {
    flag:
        joy = readJoystick();

        switch (joy)
        {
            case 1: --low_limit;  break;
            case 2: ++low_limit;  break;
            case 3: ++high_limit; break;
            case 4: --high_limit; break;
            default: break;
        }

        if (low_limit >= high_limit)
        {
            printf("Lower limit must be less than upper limit\n");
            goto flag;
        }

        temp = readNTC();

        printf("Temp: %.2f°C  Low:%d  High:%d\n",
               temp, low_limit, high_limit);

        // ===== LED & Alarm logic =====
        if (temp < low_limit)
        {
            // LOW -> Yellow (R+G)
            setLED(HIGH, HIGH);
            for (i = 0; i < 3; i++) buzzerBeep(400);
        }
        else if (temp >= high_limit)
        {
            // HIGH -> Red
            setLED(HIGH, LOW);
            for (i = 0; i < 3; i++) buzzerBeep(80);
        }
        else
        {
            // NORMAL -> Green
            setLED(LOW, HIGH);
            buzzerOff();
        }

        delay(200);
    }

    return 0;
}