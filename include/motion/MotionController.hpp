#pragma once

#include "motor/IMotorDriver.hpp"

/**
 * @brief Motion command for the differential-drive vehicle.
 */
enum class Motion {
    Up,
    Down,
    Left,
    Right,
    Stop
};

/**
 * @brief Maps high-level motion commands to low-level motor outputs.
 */
class MotionController {
public:
    explicit MotionController(IMotorDriver& driver) : driver_(driver) {}

    /**
     * @brief Apply the requested motion.
     * @param m Motion mode.
     * @param speed Duty cycle in percent.
     */
    void apply(Motion m, int speed);

private:
    IMotorDriver& driver_;
};
