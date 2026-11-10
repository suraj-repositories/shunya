#pragma once

#include "shunya/core/base/result.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::integ {

struct TelemetryFrame {
    double timestamp_s{0.0};
    std::string probe_name;
    double value{0.0};
    std::string unit;
};

/**
 * @brief Lightweight WebSocket RFC 6455 telemetry broadcast hub supporting multi-client live streaming.
 */
class TelemetryStreamServer {
public:
    using FrameCallback = std::function<void(const std::string& json_payload)>;

    [[nodiscard]] core::base::Result<uint64_t> connect_client(FrameCallback on_frame);
    void disconnect_client(uint64_t client_id);

    void publish_metric(const TelemetryFrame& frame);
    [[nodiscard]] size_t broadcast_pending();

    [[nodiscard]] static std::string format_json_packet(const std::vector<TelemetryFrame>& frames);
    [[nodiscard]] static std::vector<uint8_t> encode_websocket_text_frame(std::string_view payload);
    [[nodiscard]] static std::string compute_websocket_accept_key(std::string_view client_sec_key);

    [[nodiscard]] size_t client_count() const;
    [[nodiscard]] uint64_t total_frames_broadcast() const noexcept { return total_broadcasts_; }

private:
    mutable std::mutex mutex_;
    uint64_t next_client_id_{1};
    uint64_t total_broadcasts_{0};
    std::unordered_map<uint64_t, FrameCallback> clients_;
    std::vector<TelemetryFrame> pending_frames_;
};

} // namespace shunya::integ
