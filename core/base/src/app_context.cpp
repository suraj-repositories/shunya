#include "shunya/core/base/app_context.hpp"
#include "shunya/core/base/version.hpp"

namespace shunya::core::base {

AppContext& AppContext::instance() {
    static AppContext inst;
    return inst;
}

AppContext::~AppContext() {
    shutdown();
}

Result<void> AppContext::initialize(const AppInitOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == AppLifecycleState::Running) {
        return Result<void>::err({
            .code = 1001,
            .domain = ErrorDomain::System,
            .severity = Severity::Warning,
            .message = "AppContext is already running",
            .details = ""
        });
    }

    state_ = AppLifecycleState::Initializing;

    // 1. Configure logger
    auto& log = Logger::instance();
    log.set_level(options.min_log_level);

    // 2. Set default config values
    config_.set("app.name", options.app_name);
    config_.set("app.version", std::string(CURRENT_VERSION.as_string()));

    if (!options.config_text.empty()) {
        auto load_res = config_.load_from_text(options.config_text);
        if (load_res.is_err()) {
            state_ = AppLifecycleState::Uninitialized;
            return load_res;
        }
    }

    // 3. Initialize ThreadPool
    std::size_t threads = options.worker_threads;
    if (threads == 0) {
        threads = static_cast<std::size_t>(config_.get_int_or("app.threads", 0));
    }
    thread_pool_ = std::make_unique<ThreadPool>(threads);

    state_ = AppLifecycleState::Running;
    SHUNYA_LOG_INFO("Bootstrap", "AppContext initialized successfully for " + options.app_name);

    return Result<void>::ok();
}

Result<void> AppContext::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == AppLifecycleState::Stopped || state_ == AppLifecycleState::Uninitialized) {
        return Result<void>::ok();
    }

    state_ = AppLifecycleState::ShuttingDown;
    SHUNYA_LOG_INFO("Bootstrap", "AppContext shutting down...");

    // Drain thread pool
    if (thread_pool_) {
        thread_pool_->shutdown(true);
        thread_pool_.reset();
    }

    // Clear service registry
    services_.clear();

    state_ = AppLifecycleState::Stopped;
    SHUNYA_LOG_INFO("Bootstrap", "AppContext shutdown complete.");

    return Result<void>::ok();
}

AppLifecycleState AppContext::state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

ThreadPool& AppContext::thread_pool() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!thread_pool_) {
        throw std::runtime_error("ThreadPool accessed on uninitialized AppContext");
    }
    return *thread_pool_;
}

} // namespace shunya::core::base
