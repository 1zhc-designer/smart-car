#pragma once

/**
 * @brief Abstract motor driver interface.
 */
class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;

    /**
     * @brief Drive the left motor.
     * @param speed Duty cycle in percent in the range [0, 100].
     * @param forward True for forward direction, false for reverse.
     */
    virtual void setLeft(int speed, bool forward) = 0;

    /**
     * @brief Drive the right motor.
     * @param speed Duty cycle in percent in the range [0, 100].
     * @param forward True for forward direction, false for reverse.
     */
    virtual void setRight(int speed, bool forward) = 0;

    /**
     * @brief Stop all motors immediately.
     */
    virtual void stopAll() = 0;
};
