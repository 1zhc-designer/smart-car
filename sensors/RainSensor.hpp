#ifndef RAIN_SENSOR_HPP
#define RAIN_SENSOR_HPP

#include <cstdint>
#include <optional>

/**
 * 雨滴传感器（PCF8591 A/D + 模块 DO 数字输出）
 *
 * - 模拟量：PCF8591 AINx (默认 AIN0)，0~255（越湿通常数值越小或越大取决于模块接法/电位器）
 * - 数字量：模块 DO 输出（默认 wiringPi pin 0）
 *
 * 注意：
 * 1) 需要在主程序中先调用 wiringPiSetup()。
 * 2) PCF8591 需要 I2C 正常工作（树莓派启用 I2C）。
 */
class RainSensor
{
public:
    struct Reading {
        int analog;      // 0~255
        int digital;     // 0 or 1
        bool raining;    // 根据 digital 推断（可选用 analog 阈值改造）
    };

    // pcfBase: wiringPi pcf8591 的基址（示例代码用 120）
    // i2cAddr: PCF8591 I2C 地址（常见 0x48）
    // analogChannel: 0~3 对应 AIN0~AIN3（示例用 0）
    // doPin: 雨滴模块 DO 的 wiringPi 引脚号（示例用 0）
    RainSensor(int pcfBase = 120, int i2cAddr = 0x48, int analogChannel = 0, int doPin = 0);

    // 初始化：pcf8591Setup + 配置 DO 引脚为输入
    // 成功返回 true，失败返回 false（例如 wiringPi 尚未初始化/或 i2c 不可用导致 setup 异常情况）
    bool init();

    // 读取一次（模拟量 + 数字量 + raining 判定）
    // 需要 init() 成功后调用；若未 init 成功，返回 nullopt
    std::optional<Reading> read();

    // 仅读取模拟量（0~255）
    std::optional<int> readAnalog();

    // 仅读取 DO 数字口（0/1）
    std::optional<int> readDigital();

    // 常见模块：DO=0 表示触发（有雨/湿），DO=1 表示未触发（无雨/干）
    // 如果你实测相反，可把该函数逻辑改为 (digital==1)
    static bool digitalMeansRaining(int digitalValue);

    int pcfBase() const { return pcfBase_; }
    int i2cAddr() const { return i2cAddr_; }
    int analogChannel() const { return analogChannel_; }
    int doPin() const { return doPin_; }

private:
    int pcfBase_;
    int i2cAddr_;
    int analogChannel_;
    int doPin_;
    bool inited_{false};

    int analogPinIndex() const; // pcfBase_ + analogChannel_
};

#endif