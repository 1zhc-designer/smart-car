#include <stdio.h>
#include <wiringPi.h>

const int makerobo_motorPin[] = {1, 4, 5, 6};   // 定义步进电机管脚
int makerobo_rolePerMinute = 15;                // 每分钟转数
int makerobo_stepsPerRevolution = 2048;                  // 每转一圈的步数
int makerobo_stepSpeed = 0;

// 步进电机旋转
void makerobo_rotary(char direction){
    if(direction == 'a'){                // 逆时针旋转 
        for(int j=0;j<4;j++){
            for(int i=0;i<4;i++)
            {
            digitalWrite(makerobo_motorPin[i],0x99>>j & (0x08>>i));}
            delayMicroseconds(makerobo_stepSpeed);
        }        
    }
    else if(direction =='c'){           // 顺时针旋转  
        for(int j=0;j<4;j++){
            for(int i=0;i<4;i++)
                {digitalWrite(makerobo_motorPin[i],0x99<<j & (0x80>>i));}
            delayMicroseconds(makerobo_stepSpeed);
        }   
    }
}
//  循环函数
void makerobo_loop()
{
    char makerobo_direction = '0';
    while (1)
    {       
        printf("Makerobo select motor direction a=anticlockwise, c=clockwise: ");
        makerobo_direction=getchar();
        if (makerobo_direction == 'c')    // 顺时针旋转
        {
            printf("Makerobo motor running clockwise\n"); 
            break;
        }
        else if (makerobo_direction == 'a') // 逆时针旋转
        {
            printf("Makerobo motor running anti-clockwise\n");
            break;
        }
        else
        {
            printf("Makerobo input error, please try again!\n"); // 输入错误，再次输入
        }
    }
    while(1)
    {
        makerobo_rotary(makerobo_direction);  // 让步进电机旋转
    }
}
// 主函数
int main(void)
{
    //初始化连接失败时，将消息打印到屏幕
    if (wiringPiSetup() == -1)
    {
        printf("setup wiringPi failed !");
        return 0;
    }
    for (int i = 0; i < 4; i++)
    {
        pinMode(makerobo_motorPin[i], OUTPUT);
    }
    makerobo_stepSpeed = (60000000 / makerobo_rolePerMinute) / makerobo_stepsPerRevolution; // 每一步所用的时间
    makerobo_loop();    //  循环函数  
    return 0;
}
