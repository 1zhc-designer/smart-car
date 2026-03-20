#pragma once

/**
 * @brief Abstract motor driver interface.
 */
class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;
    virtual void setLeft(int speed, bool forward) = 0;
    virtual void setRight(int speed, bool forward) = 0;
    virtual void stopAll() = 0;
};
