#pragma once

#include <typeindex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <cstdint>
#include <span>
#include <optional>
#include "shunya/core/base/ring_buffer.hpp"

namespace shunya::core::base {

using SubscriptionId = uint64_t;

/**
 * @brief Telemetry metric packet for wait-free high-frequency streaming.
 */
struct TelemetryMetric {
    uint64_t timestamp_ns{0};
    uint32_t channel_id{0};
    double value{0.0};
};

class EventBus {
private:
    struct IHandlerWrapper {
        virtual ~IHandlerWrapper() = default;
        SubscriptionId id = 0;
    };

    template <typename EventT>
    struct HandlerWrapper : public IHandlerWrapper {
        std::function<void(const EventT&)> callback;
        explicit HandlerWrapper(std::function<void(const EventT&)> cb) : callback(std::move(cb)) {}
    };

    struct IQueuedEvent {
        virtual ~IQueuedEvent() = default;
        virtual void dispatch(EventBus& bus) = 0;
    };

    template <typename EventT>
    struct QueuedEvent : public IQueuedEvent {
        EventT event_data;
        explicit QueuedEvent(EventT data) : event_data(std::move(data)) {}
        void dispatch(EventBus& bus) override {
            bus.publish_immediate(event_data);
        }
    };

public:
    static EventBus& instance() {
        static EventBus inst;
        return inst;
    }

    EventBus() = default;
    ~EventBus() = default;

    template <typename EventT>
    SubscriptionId subscribe(std::function<void(const EventT&)> handler) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        SubscriptionId id = next_id_++;
        auto wrapper = std::make_shared<HandlerWrapper<EventT>>(std::move(handler));
        wrapper->id = id;
        handlers_[std::type_index(typeid(EventT))].push_back(wrapper);
        id_to_type_.insert_or_assign(id, std::type_index(typeid(EventT)));
        return id;
    }

    bool unsubscribe(SubscriptionId id) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        auto type_it = id_to_type_.find(id);
        if (type_it == id_to_type_.end()) {
            return false;
        }

        auto handlers_it = handlers_.find(type_it->second);
        if (handlers_it != handlers_.end()) {
            auto& list = handlers_it->second;
            for (auto it = list.begin(); it != list.end(); ++it) {
                if ((*it)->id == id) {
                    list.erase(it);
                    break;
                }
            }
        }

        id_to_type_.erase(type_it);
        return true;
    }

    template <typename EventT>
    void publish_immediate(const EventT& event) {
        std::vector<std::shared_ptr<HandlerWrapper<EventT>>> targets;
        {
            std::shared_lock<std::shared_mutex> lock(rw_mutex_);
            auto it = handlers_.find(std::type_index(typeid(EventT)));
            if (it != handlers_.end()) {
                targets.reserve(it->second.size());
                for (const auto& w : it->second) {
                    targets.push_back(std::static_pointer_cast<HandlerWrapper<EventT>>(w));
                }
            }
        }

        // Deliver outside the lock to allow handlers to re-enter the event bus
        for (const auto& target : targets) {
            if (target && target->callback) {
                target->callback(event);
            }
        }
    }

    template <typename EventT>
    void enqueue(EventT event) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::make_unique<QueuedEvent<EventT>>(std::move(event)));
    }

    std::size_t flush_queue() {
        std::vector<std::unique_ptr<IQueuedEvent>> batch;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            batch.reserve(queue_.size());
            while (!queue_.empty()) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop();
            }
        }

        for (auto& item : batch) {
            if (item) {
                item->dispatch(*this);
            }
        }
        return batch.size();
    }

    [[nodiscard]] std::size_t queued_count() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return queue_.size();
    }

    // --- High-throughput Wait-Free Lockless Telemetry (PH4-02.02.004) ---

    void emit_telemetry(uint32_t channel, double value, uint64_t timestamp_ns = 0) noexcept {
        telemetry_queue_.push_overwrite({timestamp_ns, channel, value});
    }

    bool try_emit_telemetry(const TelemetryMetric& metric) noexcept {
        return telemetry_queue_.push(metric);
    }

    std::optional<TelemetryMetric> poll_telemetry() noexcept {
        return telemetry_queue_.pop();
    }

    size_t flush_telemetry(std::span<TelemetryMetric> out_batch) noexcept {
        size_t count = 0;
        while (count < out_batch.size()) {
            auto item = telemetry_queue_.pop();
            if (!item.has_value()) break;
            out_batch[count++] = *item;
        }
        return count;
    }

    [[nodiscard]] size_t telemetry_queued_count() const noexcept {
        return telemetry_queue_.size();
    }

    void clear() {
        {
            std::unique_lock<std::shared_mutex> lock(rw_mutex_);
            handlers_.clear();
            id_to_type_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            while (!queue_.empty()) {
                queue_.pop();
            }
        }
        while (telemetry_queue_.pop().has_value()) {}
    }

private:
    mutable std::shared_mutex rw_mutex_;
    mutable std::mutex queue_mutex_;
    SubscriptionId next_id_ = 1;
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<IHandlerWrapper>>> handlers_;
    std::unordered_map<SubscriptionId, std::type_index> id_to_type_;
    std::queue<std::unique_ptr<IQueuedEvent>> queue_;
    RingBuffer<TelemetryMetric> telemetry_queue_{8192};
};

} // namespace shunya::core::base
