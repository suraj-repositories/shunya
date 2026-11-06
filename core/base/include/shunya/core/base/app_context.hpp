#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/config.hpp"
#include "shunya/core/base/logging.hpp"
#include "shunya/core/base/event_bus.hpp"
#include "shunya/core/base/thread_pool.hpp"

#include <typeindex>
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

namespace shunya::core::base {

enum class AppLifecycleState : uint8_t {
    Uninitialized = 0,
    Initializing = 1,
    Running = 2,
    ShuttingDown = 3,
    Stopped = 4
};

[[nodiscard]] constexpr std::string_view lifecycle_state_to_string(AppLifecycleState s) noexcept {
    switch (s) {
        case AppLifecycleState::Uninitialized: return "Uninitialized";
        case AppLifecycleState::Initializing: return "Initializing";
        case AppLifecycleState::Running: return "Running";
        case AppLifecycleState::ShuttingDown: return "ShuttingDown";
        case AppLifecycleState::Stopped: return "Stopped";
    }
    return "Unknown";
}

class ServiceRegistry {
public:
    template <typename ServiceT>
    void register_service(std::shared_ptr<ServiceT> service) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.insert_or_assign(std::type_index(typeid(ServiceT)), std::static_pointer_cast<void>(service));
    }

    template <typename ServiceT>
    [[nodiscard]] std::shared_ptr<ServiceT> get_service() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(std::type_index(typeid(ServiceT)));
        if (it == services_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<ServiceT>(it->second);
    }

    template <typename ServiceT>
    [[nodiscard]] bool has_service() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.find(std::type_index(typeid(ServiceT))) != services_.end();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
};

struct AppInitOptions {
    std::string app_name = "Shunya Platform";
    std::string config_text;
    LogLevel min_log_level = LogLevel::Info;
    std::size_t worker_threads = 0; // 0 = hardware concurrency
};

class AppContext {
public:
    static AppContext& instance();

    Result<void> initialize(const AppInitOptions& options = {});
    Result<void> shutdown();

    [[nodiscard]] AppLifecycleState state() const noexcept;
    [[nodiscard]] bool is_running() const noexcept { return state() == AppLifecycleState::Running; }

    Configuration& config() noexcept { return config_; }
    [[nodiscard]] const Configuration& config() const noexcept { return config_; }

    Logger& logger() noexcept { return Logger::instance(); }
    EventBus& event_bus() noexcept { return EventBus::instance(); }

    ThreadPool& thread_pool();
    ServiceRegistry& services() noexcept { return services_; }

private:
    AppContext() = default;
    ~AppContext();

    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;

    mutable std::mutex mutex_;
    AppLifecycleState state_{AppLifecycleState::Uninitialized};
    Configuration config_;
    ServiceRegistry services_;
    std::unique_ptr<ThreadPool> thread_pool_;
};

} // namespace shunya::core::base
