#include "shunya/perf/gpu_transfer_ring.hpp"

#include <algorithm>
#include <cstring>

namespace shunya::perf {

GpuTransferRing::GpuTransferRing(size_t slot_count, size_t bytes_per_slot)
    : slot_bytes_(bytes_per_slot) {
    const size_t safe_slots = std::max<size_t>(2, slot_count);
    slots_.resize(safe_slots);
    for (auto& s : slots_) {
        s.host_pinned_buffer.assign(bytes_per_slot, 0);
        s.device_local_mirror.assign(bytes_per_slot, 0);
    }
    stats_.buffer_count = safe_slots;
}

core::base::Result<size_t> GpuTransferRing::begin_async_upload(
    uint64_t frame_id,
    std::span<const uint8_t> host_payload) {
    if (host_payload.size() > slot_bytes_) {
        return core::base::Error{
            521, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Host payload exceeds staging ring slot capacity", ""
        };
    }

    const size_t idx = static_cast<size_t>(frame_id % slots_.size());
    auto& slot = slots_[idx];

    // If previous transfer reusing this ring index hasn't retired, record a pipeline stall
    if (slot.pending_signal_value > 0 &&
        !dma_timeline_.poll(slot.pending_signal_value)) {
        ++stats_.cpu_stall_events;
        dma_timeline_.wait(slot.pending_signal_value);
    }

    if (!host_payload.empty()) {
        std::memcpy(slot.host_pinned_buffer.data(), host_payload.data(), host_payload.size());
    }
    slot.pending_signal_value = frame_id + 1;
    ++stats_.total_uploads;
    stats_.total_bytes_transferred += host_payload.size();
    return idx;
}

bool GpuTransferRing::is_frame_device_ready(uint64_t frame_id) const noexcept {
    return dma_timeline_.poll(frame_id + 1);
}

void GpuTransferRing::complete_dma_transfer(uint64_t frame_id) noexcept {
    const size_t idx = static_cast<size_t>(frame_id % slots_.size());
    auto& slot = slots_[idx];
    slot.device_local_mirror = slot.host_pinned_buffer;
    dma_timeline_.signal(frame_id + 1);
}

std::span<const uint8_t> GpuTransferRing::device_slot_view(uint64_t frame_id) const noexcept {
    const size_t idx = static_cast<size_t>(frame_id % slots_.size());
    return slots_[idx].device_local_mirror;
}

} // namespace shunya::perf
