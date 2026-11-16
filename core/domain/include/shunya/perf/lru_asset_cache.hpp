#pragma once

#include <shunya/core/base/uuid.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace shunya::perf {

/**
 * @brief Default cost calculation functor that returns sizeof(Asset) or asset.byte_size().
 */
template <typename Asset>
struct DefaultAssetCost {
    size_t operator()(const Asset& asset) const noexcept {
        if constexpr (requires { asset.byte_size(); }) {
            return asset.byte_size();
        } else if constexpr (requires { asset->byte_size(); }) {
            return asset ? asset->byte_size() : 0;
        } else {
            return sizeof(Asset);
        }
    }
};

/**
 * @brief Thread-safe Least Recently Used (LRU) Asset Cache with strict memory budget clamping.
 * 
 * Supports O(1) amortized insertion, lookup, and LRU eviction.
 * When the cumulative memory footprint exceeds capacity_bytes, the oldest
 * unreferenced assets are systematically evicted.
 */
template <
    typename Key,
    typename Asset,
    typename CostFn = DefaultAssetCost<Asset>
>
class LruAssetCache {
public:
    using EvictionCallback = std::function<void(const Key&, const Asset&)>;

    explicit LruAssetCache(size_t max_capacity_bytes = 64 * 1024 * 1024,
                           EvictionCallback on_evict = nullptr)
        : max_capacity_bytes_(max_capacity_bytes),
          on_evict_(std::move(on_evict)) {}

    ~LruAssetCache() {
        clear();
    }

    LruAssetCache(const LruAssetCache&) = delete;
    LruAssetCache& operator=(const LruAssetCache&) = delete;

    /**
     * @brief Check whether a key is present in cache without updating LRU order.
     */
    [[nodiscard]] bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.find(key) != map_.end();
    }

    /**
     * @brief Retrieve an asset from cache, promoting it to Most Recently Used.
     */
    [[nodiscard]] std::optional<Asset> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            ++miss_count_;
            return std::nullopt;
        }

        // Promote to front of MRU list
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_iter);
        ++hit_count_;
        return it->second.asset;
    }

    /**
     * @brief Insert or replace an asset in the cache.
     * 
     * Automatically evicts least recently used items if inserting this asset
     * causes total memory footprint to exceed max_capacity_bytes_.
     */
    void insert(const Key& key, Asset asset, size_t explicit_bytes = 0) {
        size_t asset_bytes = explicit_bytes > 0 ? explicit_bytes : cost_fn_(asset);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end()) {
            // Updating existing entry
            current_bytes_ -= it->second.bytes;
            it->second.asset = std::move(asset);
            it->second.bytes = asset_bytes;
            current_bytes_ += asset_bytes;
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_iter);
        } else {
            // New entry
            lru_list_.push_front(key);
            CacheEntry entry{asset, asset_bytes, lru_list_.begin()};
            map_.emplace(key, std::move(entry));
            current_bytes_ += asset_bytes;
        }

        clamp_to_budget_locked();
    }

    /**
     * @brief Erase a specific item from the cache.
     */
    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }

        current_bytes_ -= it->second.bytes;
        if (on_evict_) {
            on_evict_(it->first, it->second.asset);
        }
        lru_list_.erase(it->second.list_iter);
        map_.erase(it);
        return true;
    }

    /**
     * @brief Clear all cached assets.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (on_evict_) {
            for (const auto& [k, entry] : map_) {
                on_evict_(k, entry.asset);
            }
        }
        map_.clear();
        lru_list_.clear();
        current_bytes_ = 0;
    }

    /**
     * @brief Dynamically set new memory budget and evict excess assets.
     */
    void set_capacity_bytes(size_t new_capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_capacity_bytes_ = new_capacity;
        clamp_to_budget_locked();
    }

    [[nodiscard]] size_t capacity_bytes() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return max_capacity_bytes_;
    }

    [[nodiscard]] size_t current_bytes() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return current_bytes_;
    }

    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.empty();
    }

    [[nodiscard]] size_t hit_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return hit_count_;
    }

    [[nodiscard]] size_t miss_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return miss_count_;
    }

    [[nodiscard]] size_t eviction_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return eviction_count_;
    }

    [[nodiscard]] double hit_rate() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = hit_count_ + miss_count_;
        if (total == 0) return 0.0;
        return static_cast<double>(hit_count_) / static_cast<double>(total);
    }

    void set_eviction_callback(EvictionCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_evict_ = std::move(cb);
    }

private:
    struct CacheEntry {
        Asset asset;
        size_t bytes{0};
        typename std::list<Key>::iterator list_iter;
    };

    void clamp_to_budget_locked() {
        while (current_bytes_ > max_capacity_bytes_ && !lru_list_.empty()) {
            const Key& lru_key = lru_list_.back();
            auto it = map_.find(lru_key);
            if (it != map_.end()) {
                current_bytes_ -= it->second.bytes;
                ++eviction_count_;
                if (on_evict_) {
                    on_evict_(it->first, it->second.asset);
                }
                map_.erase(it);
            }
            lru_list_.pop_back();
        }
    }

    size_t max_capacity_bytes_{64 * 1024 * 1024};
    size_t current_bytes_{0};
    size_t hit_count_{0};
    size_t miss_count_{0};
    size_t eviction_count_{0};

    CostFn cost_fn_{};
    EvictionCallback on_evict_{nullptr};

    std::list<Key> lru_list_;
    std::unordered_map<Key, CacheEntry> map_;
    mutable std::mutex mutex_;
};

} // namespace shunya::perf
