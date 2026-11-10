#include "shunya/integ/shared_memory_bus.hpp"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace shunya::integ {

namespace {

size_t round_up_power_of_two(size_t v) noexcept {
    if (v <= 2) return 2;
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

} // namespace

SharedMemoryBus::~SharedMemoryBus() {
    close();
}

core::base::Result<void> SharedMemoryBus::create(
    const std::string& channel_name,
    size_t slot_capacity) {
    close();
    if (channel_name.empty()) {
        return core::base::Error{481, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Shared memory channel name cannot be empty", ""};
    }

    const size_t cap = round_up_power_of_two(slot_capacity);
    const size_t total_bytes = sizeof(SharedRingHeader) + cap * sizeof(TelemetryIpcPacket);

#ifdef _WIN32
    const std::string map_name = "Local\\ShunyaIpc_" + channel_name;
    HANDLE h_map = ::CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        static_cast<DWORD>((total_bytes >> 32) & 0xFFFFFFFFu),
        static_cast<DWORD>(total_bytes & 0xFFFFFFFFu),
        map_name.c_str());
    if (!h_map) {
        return core::base::Error{482, core::base::ErrorDomain::IO, core::base::Severity::Error, "CreateFileMappingA failed", map_name};
    }
    void* view = ::MapViewOfFile(h_map, FILE_MAP_ALL_ACCESS, 0, 0, total_bytes);
    if (!view) {
        ::CloseHandle(h_map);
        return core::base::Error{483, core::base::ErrorDomain::IO, core::base::Severity::Error, "MapViewOfFile failed", map_name};
    }
    handle_ = static_cast<void*>(h_map);
    mapped_ptr_ = view;
#else
    posix_name_ = "/shunya_ipc_" + channel_name;
    int fd = ::shm_open(posix_name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        return core::base::Error{482, core::base::ErrorDomain::IO, core::base::Severity::Error, "shm_open failed", posix_name_};
    }
    if (::ftruncate(fd, static_cast<off_t>(total_bytes)) != 0) {
        ::close(fd);
        return core::base::Error{483, core::base::ErrorDomain::IO, core::base::Severity::Error, "ftruncate failed", posix_name_};
    }
    void* view = ::mmap(nullptr, total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (view == MAP_FAILED) {
        return core::base::Error{483, core::base::ErrorDomain::IO, core::base::Severity::Error, "mmap failed", posix_name_};
    }
    mapped_ptr_ = view;
#endif

    mapped_bytes_ = total_bytes;
    is_creator_ = true;

    auto* hdr = static_cast<SharedRingHeader*>(mapped_ptr_);
    hdr->magic = 0x5348554E59414950ULL;
    hdr->capacity = static_cast<uint32_t>(cap);
    hdr->mask = static_cast<uint32_t>(cap - 1);
    hdr->write_seq.store(0, std::memory_order_relaxed);
    hdr->read_seq.store(0, std::memory_order_release);
    return core::base::Result<void>::ok();
}

core::base::Result<void> SharedMemoryBus::open_existing(const std::string& channel_name) {
    close();
    if (channel_name.empty()) {
        return core::base::Error{484, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Channel name cannot be empty", ""};
    }

#ifdef _WIN32
    const std::string map_name = "Local\\ShunyaIpc_" + channel_name;
    HANDLE h_map = ::OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, map_name.c_str());
    if (!h_map) {
        return core::base::Error{485, core::base::ErrorDomain::IO, core::base::Severity::Error, "OpenFileMappingA failed", map_name};
    }
    void* view_hdr = ::MapViewOfFile(h_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedRingHeader));
    if (!view_hdr) {
        ::CloseHandle(h_map);
        return core::base::Error{486, core::base::ErrorDomain::IO, core::base::Severity::Error, "MapViewOfFile header failed", map_name};
    }
    const auto* hdr = static_cast<const SharedRingHeader*>(view_hdr);
    const size_t total_bytes = sizeof(SharedRingHeader) +
                               static_cast<size_t>(hdr->capacity) * sizeof(TelemetryIpcPacket);
    ::UnmapViewOfFile(view_hdr);

    void* view = ::MapViewOfFile(h_map, FILE_MAP_ALL_ACCESS, 0, 0, total_bytes);
    if (!view) {
        ::CloseHandle(h_map);
        return core::base::Error{486, core::base::ErrorDomain::IO, core::base::Severity::Error, "MapViewOfFile full failed", map_name};
    }
    handle_ = static_cast<void*>(h_map);
    mapped_ptr_ = view;
    mapped_bytes_ = total_bytes;
#else
    posix_name_ = "/shunya_ipc_" + channel_name;
    int fd = ::shm_open(posix_name_.c_str(), O_RDWR, 0666);
    if (fd < 0) {
        return core::base::Error{485, core::base::ErrorDomain::IO, core::base::Severity::Error, "shm_open existing failed", posix_name_};
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0 || st.st_size < static_cast<off_t>(sizeof(SharedRingHeader))) {
        ::close(fd);
        return core::base::Error{486, core::base::ErrorDomain::IO, core::base::Severity::Error, "fstat shared memory failed", posix_name_};
    }
    const size_t total_bytes = static_cast<size_t>(st.st_size);
    void* view = ::mmap(nullptr, total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (view == MAP_FAILED) {
        return core::base::Error{486, core::base::ErrorDomain::IO, core::base::Severity::Error, "mmap failed", posix_name_};
    }
    mapped_ptr_ = view;
    mapped_bytes_ = total_bytes;
#endif

    is_creator_ = false;
    return core::base::Result<void>::ok();
}

void SharedMemoryBus::close() noexcept {
    if (mapped_ptr_) {
#ifdef _WIN32
        ::UnmapViewOfFile(mapped_ptr_);
        if (handle_) ::CloseHandle(static_cast<HANDLE>(handle_));
#else
        ::munmap(mapped_ptr_, mapped_bytes_);
        if (is_creator_ && !posix_name_.empty()) {
            ::shm_unlink(posix_name_.c_str());
        }
#endif
        mapped_ptr_ = nullptr;
        handle_ = nullptr;
        mapped_bytes_ = 0;
    }
}

bool SharedMemoryBus::try_push(const TelemetryIpcPacket& packet) noexcept {
    if (!mapped_ptr_) return false;
    auto* hdr = static_cast<SharedRingHeader*>(mapped_ptr_);
    const uint64_t w = hdr->write_seq.load(std::memory_order_relaxed);
    const uint64_t r = hdr->read_seq.load(std::memory_order_acquire);
    if ((w - r) >= hdr->capacity) {
        return false;
    }

    auto* slots = reinterpret_cast<TelemetryIpcPacket*>(
        static_cast<uint8_t*>(mapped_ptr_) + sizeof(SharedRingHeader));
    slots[w & hdr->mask] = packet;
    hdr->write_seq.store(w + 1, std::memory_order_release);
    return true;
}

bool SharedMemoryBus::try_pop(TelemetryIpcPacket& out_packet) noexcept {
    if (!mapped_ptr_) return false;
    auto* hdr = static_cast<SharedRingHeader*>(mapped_ptr_);
    const uint64_t r = hdr->read_seq.load(std::memory_order_relaxed);
    const uint64_t w = hdr->write_seq.load(std::memory_order_acquire);
    if (r == w) {
        return false;
    }

    const auto* slots = reinterpret_cast<const TelemetryIpcPacket*>(
        static_cast<const uint8_t*>(mapped_ptr_) + sizeof(SharedRingHeader));
    out_packet = slots[r & hdr->mask];
    hdr->read_seq.store(r + 1, std::memory_order_release);
    return true;
}

size_t SharedMemoryBus::available_to_read() const noexcept {
    if (!mapped_ptr_) return 0;
    const auto* hdr = static_cast<const SharedRingHeader*>(mapped_ptr_);
    const uint64_t w = hdr->write_seq.load(std::memory_order_acquire);
    const uint64_t r = hdr->read_seq.load(std::memory_order_acquire);
    return (w >= r) ? static_cast<size_t>(w - r) : 0;
}

} // namespace shunya::integ
