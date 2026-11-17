#pragma once

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

struct ClipboardItem {
    std::shared_ptr<Curve2D> curve;
    std::string original_layer{"0"};
    bool is_construction{false};
};

/**
 * @brief High-precision 2D CAD clipboard manager supporting Copy, Cut, Paste,
 * Paste-At, and Duplicate-With-Offset operations with deep object cloning.
 */
class ClipboardManager2D {
public:
    ClipboardManager2D() = default;

    /**
     * @brief Deep clones selected document entities into the clipboard buffer.
     * @return Number of entities successfully copied.
     */
    size_t copy(const SketchDocument2D& doc, const std::vector<std::string>& entity_ids);

    /**
     * @brief Copies selected entities into clipboard buffer and removes them from doc.
     * @return Number of entities cut.
     */
    size_t cut(SketchDocument2D& doc, const std::vector<std::string>& entity_ids);

    /**
     * @brief Pastes clipboard items into doc, shifted by cumulative or explicit offset.
     * @return IDs of newly created entities in doc.
     */
    std::vector<std::string> paste(SketchDocument2D& doc, Point2D base_offset = {10.0, 10.0});

    /**
     * @brief Pastes clipboard items such that their bounding box center is aligned at target_center.
     * @return IDs of newly created entities in doc.
     */
    std::vector<std::string> paste_at(SketchDocument2D& doc, Point2D target_center);

    /**
     * @brief Duplicates selected entities directly inside doc with given offset without modifying clipboard.
     * @return IDs of duplicated entities in doc.
     */
    std::vector<std::string> duplicate(
        SketchDocument2D& doc,
        const std::vector<std::string>& entity_ids,
        Point2D offset = {10.0, 10.0}
    );

    [[nodiscard]] bool has_data() const noexcept { return !items_.empty(); }
    [[nodiscard]] size_t item_count() const noexcept { return items_.size(); }
    [[nodiscard]] const std::vector<ClipboardItem>& items() const noexcept { return items_; }
    [[nodiscard]] BBox2D clipboard_bbox() const noexcept { return buffer_bbox_; }

    void clear() noexcept;
    void reset_paste_streak() noexcept { paste_streak_ = 0; }
    [[nodiscard]] size_t paste_streak() const noexcept { return paste_streak_; }

private:
    std::vector<ClipboardItem> items_;
    BBox2D buffer_bbox_{};
    size_t paste_streak_{0};
};

} // namespace shunya::cad2d
