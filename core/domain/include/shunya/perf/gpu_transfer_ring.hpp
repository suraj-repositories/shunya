#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/perf/timeline_semaphore.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace shunya::perf {

struct TransferRingStats {
    uint64_t total_uploads{0};
    uint64_t total_bytes_transferred{0};
    uint64_t cpu_stall_events{0};
    size_t buffer_count{0};
};

/**
 * @brief Double/triple-buffered host-visible staging ring eliminating CPU-GPU pipeline stalls via timeline semaphores.
 */
class GpuTransferRing {
public:
    GpuTransferRing(size_t slot_count, size_t bytes_per_slot);

    [[nodiscard]] core::base::Result<size_t> begin_async_upload(
        uint64_t frame_id,
        std::span<const uint8_t> host_payload);

    [[nodiscard]] bool is_frame_device_ready(uint64_t frame_id) const noexcept;
    void complete_dma_transfer(uint64_t frame_id) noexcept;

    [[nodiscard]] std::span<const uint8_t> device_slot_view(uint64_t frame_id) const noexcept;
    [[nodiscard]] TransferRingStats stats() const noexcept { return stats_; }

private:
    struct StagingSlot {
        std::vector<uint8_t> host_pinned_buffer;
        std::vector<uint8_t> device_local_mirror;
        uint64_t pending_signal_value{0};
    };

    size_t slot_bytes_{0};
    std::vector<StagingSlot> slots_;
    TimelineSemaphore dma_timeline_{0};
    TransferRingStats stats_;
};

} // namespace shunya::perf
