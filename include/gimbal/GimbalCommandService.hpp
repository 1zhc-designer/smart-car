#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "gimbal/GimbalService.hpp"

/**
 * @brief Executes gimbal commands received through DDS-style callbacks.
 */
class GimbalCommandService final {
public:
    GimbalCommandService(LocalDdsBus& bus, GimbalService& gimbal)
        : bus_(bus), gimbal_(gimbal) {}

    ~GimbalCommandService() {
        stop();
    }

    GimbalCommandService(const GimbalCommandService&) = delete;
    GimbalCommandService& operator=(const GimbalCommandService&) = delete;

    void start() {
        subscription_ = bus_.subscribe<GimbalCommandTopic>(
            [this](const GimbalCommandTopic& topic) {
                switch (topic.command) {
                case GimbalCommand::TiltUp:
                    gimbal_.tiltUp();
                    break;
                case GimbalCommand::TiltDown:
                    gimbal_.tiltDown();
                    break;
                case GimbalCommand::PanLeft:
                    gimbal_.panLeft();
                    break;
                case GimbalCommand::PanRight:
                    gimbal_.panRight();
                    break;
                case GimbalCommand::Reset:
                default:
                    gimbal_.reset();
                    break;
                }
            });
    }

    void stop() {
        subscription_.reset();
    }

private:
    LocalDdsBus& bus_;
    GimbalService& gimbal_;
    LocalDdsBus::Subscription subscription_{};
};
