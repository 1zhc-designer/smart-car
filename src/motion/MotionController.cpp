#include "motion/MotionController.hpp"

void MotionController::apply(Motion motion, int speed) {
    switch (motion) {
    case Motion::Up:
        driver_.setLeft(speed, true);
        driver_.setRight(speed, true);
        break;
    case Motion::Down:
        driver_.setLeft(speed, false);
        driver_.setRight(speed, false);
        break;
    case Motion::Left:
        driver_.setLeft(speed, false);
        driver_.setRight(speed, true);
        break;
    case Motion::Right:
        driver_.setLeft(speed, true);
        driver_.setRight(speed, false);
        break;
    case Motion::Stop:
    default:
        driver_.stopAll();
        break;
    }
}
