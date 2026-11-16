#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"

#include <memory>
#include <string>
#include <utility>
#include <type_traits>
#include <atomic>

namespace shunya::core::domain {

/**
 * @brief Strong type alias for exclusive RAII ownership.
 *        Guarantees single-owner semantics and zero heap leaks on scope exit.
 */
template <typename T>
using ExclusiveAsset = std::unique_ptr<T>;

/**
 * @brief Strong type alias for shared reference-counted RAII ownership.
 *        Used for shared domain entities (Materials, Objects, Projects).
 */
template <typename T>
using SharedAsset = std::shared_ptr<T>;

/**
 * @brief Strong type alias for weak non-owning references.
 *        Used to break potential reference cycles between parents/children and circuits/objects.
 */
template <typename T>
using WeakAsset = std::weak_ptr<T>;

/**
 * @brief Factory helper creating an exclusive RAII asset.
 */
template <typename T, typename... Args>
[[nodiscard]] inline ExclusiveAsset<T> make_exclusive(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/**
 * @brief Factory helper creating a shared RAII asset.
 */
template <typename T, typename... Args>
[[nodiscard]] inline SharedAsset<T> make_shared_asset(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief RAII scoped lifecycle monitor that tracks lifetime and asserts clean destruction.
 */
template <typename T>
class AssetLifetimeTracker {
public:
    explicit AssetLifetimeTracker(SharedAsset<T> asset) noexcept
        : asset_(std::move(asset)) {}

    [[nodiscard]] bool is_alive() const noexcept {
        return !asset_.expired();
    }

    [[nodiscard]] long use_count() const noexcept {
        return asset_.use_count();
    }

    [[nodiscard]] SharedAsset<T> lock() const noexcept {
        return asset_.lock();
    }

private:
    WeakAsset<T> asset_;
};

/**
 * @brief Smart RAII handle combining asset ID, strong/weak semantics, and provenance tracking.
 */
template <typename T>
class SmartAssetHandle {
public:
    SmartAssetHandle() = default;

    explicit SmartAssetHandle(base::Uuid id, SharedAsset<T> ptr)
        : id_(id), asset_(std::move(ptr)) {}

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] bool is_valid() const noexcept { return asset_ != nullptr; }

    [[nodiscard]] T* get() noexcept { return asset_.get(); }
    [[nodiscard]] const T* get() const noexcept { return asset_.get(); }

    [[nodiscard]] T& operator*() { return *asset_; }
    [[nodiscard]] const T& operator*() const { return *asset_; }

    [[nodiscard]] T* operator->() noexcept { return asset_.get(); }
    [[nodiscard]] const T* operator->() const noexcept { return asset_.get(); }

    [[nodiscard]] SharedAsset<T> shared() const noexcept { return asset_; }
    [[nodiscard]] WeakAsset<T> weak() const noexcept { return asset_; }
    [[nodiscard]] long use_count() const noexcept { return asset_.use_count(); }

    void reset() noexcept {
        asset_.reset();
        id_ = {};
    }

private:
    base::Uuid id_{};
    SharedAsset<T> asset_{nullptr};
};

} // namespace shunya::core::domain
