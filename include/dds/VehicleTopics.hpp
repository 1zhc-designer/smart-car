#pragma once

#include "motion/MotionController.hpp"

#include <chrono>
#include <string>

/**
 * @brief Topic carrying a motion command.
 *
 * This is intentionally similar to a DDS topic payload: it is a plain data
 * structure with no ownership of the execution logic.
 */
struct MotionCommandTopic {
    Motion motion{Motion::Stop};
    int speed{0};
    std::chrono::milliseconds duration{0};
    std::string source{"unknown"};
};

/**
 * @brief Topic carrying an immediate gimbal command.
 */
enum class GimbalCommand {
    TiltUp,
    TiltDown,
    PanLeft,
    PanRight,
    Reset
};

struct GimbalCommandTopic {
    GimbalCommand command{GimbalCommand::Reset};
    std::string source{"unknown"};
};

/**
 * @brief Topic used for optional status/logging updates.
 */
struct SystemStatusTopic {
    std::string message;
};
