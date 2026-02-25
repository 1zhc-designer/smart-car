#ifndef RAIN_SENSOR_HPP
#define RAIN_SENSOR_HPP

#include <cstdint>
#include <optional>

/**
 * Rain Drop Sensor (PCF8591 A/D + module DO digital output)
 *
 * - Analog output: PCF8591 AINx (default AIN0), range 0~255
 *   (Higher or lower values when wet depend on module wiring and potentiometer adjustment.)
 * - Digital output: module DO pin (default wiringPi pin 0)
 *
 * Notes:
 * 1) wiringPiSetup() must be called in the main program before using this class.
 * 2) PCF8591 requires I2C to be properly enabled and configured (e.g., enabled on Raspberry Pi).
 */
class RainSensor
{
public:
    struct Reading {
        int analog;      // 0~255
        int digital;     // 0 or 1
        bool raining;    // Inferred from digital value (can be modified to use analog threshold)
    };

    // pcfBase: Base address used by wiringPi for pcf8591 (example: 120)
    // i2cAddr: I2C address of PCF8591 (commonly 0x48)
    // analogChannel: 0~3 corresponding to AIN0~AIN3 (example uses 0)
    // doPin: wiringPi pin number connected to the module DO pin (example uses 0)
    RainSensor(int pcfBase = 120, int i2cAddr = 0x48, int analogChannel = 0, int doPin = 0);

    // Initialize: calls pcf8591Setup + configures DO pin as input
    // Returns true on success, false on failure
    // (e.g., wiringPi not initialized or I2C unavailable causing setup failure)
    bool init();

    // Read once (analog + digital + raining determination)
    // Must be called after successful init(); returns nullopt if not initialized
    std::optional<Reading> read();

    // Read analog value only (0~255)
    std::optional<int> readAnalog();

    // Read DO digital pin only (0/1)
    std::optional<int> readDigital();

    // Common modules: DO = 0 means triggered (wet/raining),
    // DO = 1 means not triggered (dry/no rain).
    // If your hardware behaves oppositely, modify logic to (digitalValue == 1).
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

    int analogPinIndex() const; // Returns pcfBase_ + analogChannel_
};

#endif