#pragma once

#include "shunya/core/base/result.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace shunya::integ {

struct TelemetryIpcPacket {
    uint64_t sequence_id{0};
    double sim_time_s{0.0};
    uint32_t channel_id{0};
    uint32_t value_count{0};
    double values[8]{};
};

/**
 * @brief Lock-free SPSC OS named shared-memory ring buffer for sub-microsecond HIL telemetry IPC.
 */
class SharedMemoryBus {
public:
    static constexpr size_t kDefaultCapacity = 1024; // Must be power of 2

    SharedMemoryBus() = default;
    ~SharedMemoryBus();

    SharedMemoryBus(const SharedMemoryBus&) = delete;
    SharedMemoryBus& operator=(const SharedMemoryBus&) = delete;

    [[nodiscard]] core::base::Result<void> create(
        const std::string& channel_name,
        size_t slot_capacity = kDefaultCapacity);

    [[nodiscard]] core::base::Result<void> open_existing(const std::string& channel_name);
    void close() noexcept;

    [[nodiscard]] bool try_push(const TelemetryIpcPacket& packet) noexcept;
    [[nodiscard]] bool try_pop(TelemetryIpcPacket& out_packet) noexcept;

    [[nodiscard]] size_t available_to_read() const noexcept;
    [[nodiscard]] bool is_open() const noexcept { return mapped_ptr_ != nullptr; }

private:
    struct alignas(64) SharedRingHeader {
        uint64_t magic{0x5348554E59414950ULL}; // "SHUNYAIP"
        uint32_t capacity{1024};
        uint32_t mask{1023};
        alignas(64) std::atomic<uint64_t> write_seq{0};
        alignas(64) std::atomic<uint64_t> read_seq{0};
    };

    void* handle_{nullptr};
    void* mapped_ptr_{nullptr};
    size_t mapped_bytes_{0};
    bool is_creator_{false};
    std::string posix_name_;
};

} // namespace shunya::integ
