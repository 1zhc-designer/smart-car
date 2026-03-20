#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Lightweight in-process DDS-style publish/subscribe bus.
 *
 * This is not a network DDS middleware. It is an in-process topic bus that
 * keeps the same architectural idea the teacher is asking for: components do
 * not talk to a global scheduler, but exchange strongly typed topic messages
 * through callback-based interfaces.
 */
class LocalDdsBus {
public:
    class Subscription {
    public:
        Subscription() = default;
        Subscription(std::function<void()> unsubscribe, std::shared_ptr<std::atomic<bool>> alive)
            : unsubscribe_(std::move(unsubscribe)), alive_(std::move(alive)) {}

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
            : unsubscribe_(std::move(other.unsubscribe_)), alive_(std::move(other.alive_)) {}

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                reset();
                unsubscribe_ = std::move(other.unsubscribe_);
                alive_ = std::move(other.alive_);
            }
            return *this;
        }

        ~Subscription() {
            reset();
        }

        void reset() {
            if (unsubscribe_) {
                unsubscribe_();
                unsubscribe_ = {};
            }
            alive_.reset();
        }

    private:
        std::function<void()> unsubscribe_;
        std::shared_ptr<std::atomic<bool>> alive_;
    };

    template <typename Topic>
    Subscription subscribe(std::function<void(const Topic&)> callback) {
        auto channel = channelFor<Topic>();
        auto alive = std::make_shared<std::atomic<bool>>(true);
        auto holder = std::make_shared<CallbackHolder<Topic>>(std::move(callback), alive);

        std::lock_guard<std::mutex> lock(channel->mutex);
        channel->callbacks.push_back(holder);

        return Subscription(
            [channel, alive]() {
                alive->store(false, std::memory_order_release);
                std::lock_guard<std::mutex> innerLock(channel->mutex);
                auto& callbacks = channel->callbacks;
                callbacks.erase(
                    std::remove_if(callbacks.begin(), callbacks.end(),
                                   [](const std::shared_ptr<ICallbackHolder>& candidate) {
                                       return !candidate || !candidate->alive();
                                   }),
                    callbacks.end());
            },
            alive);
    }

    template <typename Topic>
    void publish(const Topic& topic) {
        auto channel = channelFor<Topic>();

        std::vector<std::shared_ptr<ICallbackHolder>> callbacks;
        {
            std::lock_guard<std::mutex> lock(channel->mutex);
            callbacks = channel->callbacks;
        }

        for (const auto& callback : callbacks) {
            if (!callback || !callback->alive()) {
                continue;
            }
            static_cast<CallbackHolder<Topic>&>(*callback).invoke(topic);
        }
    }

private:
    struct IChannel {
        virtual ~IChannel() = default;
    };

    struct ICallbackHolder {
        virtual ~ICallbackHolder() = default;
        virtual bool alive() const = 0;
    };

    template <typename Topic>
    struct CallbackHolder final : ICallbackHolder {
        CallbackHolder(std::function<void(const Topic&)> callback,
                       std::shared_ptr<std::atomic<bool>> aliveFlag)
            : callback(std::move(callback)), aliveFlag(std::move(aliveFlag)) {}

        void invoke(const Topic& topic) {
            callback(topic);
        }

        bool alive() const override {
            return aliveFlag && aliveFlag->load(std::memory_order_acquire);
        }

        std::function<void(const Topic&)> callback;
        std::shared_ptr<std::atomic<bool>> aliveFlag;
    };

    struct Channel final : IChannel {
        std::mutex mutex;
        std::vector<std::shared_ptr<ICallbackHolder>> callbacks;
    };

    template <typename Topic>
    std::shared_ptr<Channel> channelFor() {
        const std::type_index key{typeid(Topic)};
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(key);
        if (it != channels_.end()) {
            return std::static_pointer_cast<Channel>(it->second);
        }
        auto created = std::make_shared<Channel>();
        channels_[key] = created;
        return created;
    }

    std::mutex mutex_;
    std::unordered_map<std::type_index, std::shared_ptr<IChannel>> channels_;
};
