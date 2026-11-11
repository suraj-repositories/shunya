#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace shunya::perf {

/**
 * @brief Cache-line / SIMD aligned contiguous buffer guaranteeing Alignment-byte base alignment.
 */
template <typename T, size_t Alignment = 64>
class AlignedBuffer {
    static_assert(std::is_trivially_copyable_v<T>, "AlignedBuffer requires trivially copyable element types");
    static_assert((Alignment & (Alignment - 1)) == 0 && Alignment >= alignof(T), "Alignment must be power of 2");

public:
    AlignedBuffer() = default;

    explicit AlignedBuffer(size_t count, const T& init = T{}) {
        resize(count, init);
    }

    ~AlignedBuffer() { release(); }

    AlignedBuffer(const AlignedBuffer& other) {
        if (other.size_ > 0) {
            reserve(other.size_);
            std::copy_n(other.data_, other.size_, data_);
            size_ = other.size_;
        }
    }

    AlignedBuffer& operator=(const AlignedBuffer& other) {
        if (this != &other) {
            AlignedBuffer tmp(other);
            swap(tmp);
        }
        return *this;
    }

    AlignedBuffer(AlignedBuffer&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept {
        if (this != &other) {
            release();
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void swap(AlignedBuffer& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    void reserve(size_t new_cap) {
        if (new_cap <= capacity_) return;
        T* fresh = static_cast<T*>(::operator new(new_cap * sizeof(T), std::align_val_t{Alignment}));
        if (data_) {
            if (size_ > 0) std::copy_n(data_, size_, fresh);
            ::operator delete(data_, std::align_val_t{Alignment});
        }
        data_ = fresh;
        capacity_ = new_cap;
    }

    void resize(size_t new_size, const T& val = T{}) {
        if (new_size > capacity_) {
            reserve(std::max(new_size, capacity_ * 2));
        }
        if (new_size > size_) {
            std::fill(data_ + size_, data_ + new_size, val);
        }
        size_ = new_size;
    }

    void push_back(const T& val) {
        if (size_ == capacity_) {
            reserve(capacity_ == 0 ? 16 : capacity_ * 2);
        }
        data_[size_++] = val;
    }

    void clear() noexcept { size_ = 0; }

    [[nodiscard]] T* data() noexcept { return data_; }
    [[nodiscard]] const T* data() const noexcept { return data_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::span<T> span() noexcept { return {data_, size_}; }
    [[nodiscard]] std::span<const T> span() const noexcept { return {data_, size_}; }

    [[nodiscard]] T& operator[](size_t i) noexcept { return data_[i]; }
    [[nodiscard]] const T& operator[](size_t i) const noexcept { return data_[i]; }

    [[nodiscard]] bool is_aligned() const noexcept {
        return !data_ || (reinterpret_cast<uintptr_t>(data_) % Alignment == 0);
    }

private:
    void release() noexcept {
        if (data_) {
            ::operator delete(data_, std::align_val_t{Alignment});
            data_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }
    }

    T* data_{nullptr};
    size_t size_{0};
    size_t capacity_{0};
};

/**
 * @brief Structure-of-Arrays (SoA) 64-byte cache-aligned particle container eliminating AoS stride thrashing.
 */
struct SoAParticleContainer {
    AlignedBuffer<float, 64> px;
    AlignedBuffer<float, 64> py;
    AlignedBuffer<float, 64> pz;
    AlignedBuffer<float, 64> vx;
    AlignedBuffer<float, 64> vy;
    AlignedBuffer<float, 64> vz;
    AlignedBuffer<float, 64> mass;
    AlignedBuffer<float, 64> density;

    void reserve(size_t n) {
        px.reserve(n); py.reserve(n); pz.reserve(n);
        vx.reserve(n); vy.reserve(n); vz.reserve(n);
        mass.reserve(n); density.reserve(n);
    }

    void resize(size_t n) {
        px.resize(n, 0.0f); py.resize(n, 0.0f); pz.resize(n, 0.0f);
        vx.resize(n, 0.0f); vy.resize(n, 0.0f); vz.resize(n, 0.0f);
        mass.resize(n, 1e-3f); density.resize(n, 1000.0f);
    }

    void push_back(float x, float y, float z, float u, float v, float w, float m = 1e-3f, float rho = 1000.0f) {
        px.push_back(x); py.push_back(y); pz.push_back(z);
        vx.push_back(u); vy.push_back(v); vz.push_back(w);
        mass.push_back(m); density.push_back(rho);
    }

    void clear() noexcept {
        px.clear(); py.clear(); pz.clear();
        vx.clear(); vy.clear(); vz.clear();
        mass.clear(); density.clear();
    }

    [[nodiscard]] size_t size() const noexcept { return px.size(); }

    [[nodiscard]] bool all_planes_64byte_aligned() const noexcept {
        return px.is_aligned() && py.is_aligned() && pz.is_aligned() &&
               vx.is_aligned() && vy.is_aligned() && vz.is_aligned() &&
               mass.is_aligned() && density.is_aligned();
    }
};

} // namespace shunya::perf
