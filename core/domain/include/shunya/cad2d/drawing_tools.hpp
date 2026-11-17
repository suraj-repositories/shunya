#pragma once

#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/bezier_spline.hpp"
#include "shunya/cad2d/construction_layer.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace shunya::cad2d {

enum class DrawingToolType {
    Line,
    Rectangle,
    Circle,
    Ellipse,
    RegularPolygon,
    ArbitraryPolygon,
    Arc3Point,
    ArcCenterRadius,
    CubicBezier,
    FreehandPolyline
};

enum class ToolState {
    Idle,
    Active,
    Finished
};

/**
 * @brief Base class for interactive 2D drafting creation tools.
 */
class InteractiveDrawingTool {
public:
    virtual ~InteractiveDrawingTool() = default;

    [[nodiscard]] virtual DrawingToolType tool_type() const noexcept = 0;
    [[nodiscard]] virtual ToolState state() const noexcept = 0;
    virtual void reset() = 0;
    virtual void cancel() = 0;

    // Pointer events in world coordinates
    virtual bool on_pointer_down(const Point2D& pt) = 0;
    virtual void on_pointer_move(const Point2D& pt) = 0;
    virtual bool on_pointer_up(const Point2D& pt) = 0;

    // Visual feedback curves during interaction
    [[nodiscard]] virtual std::vector<std::shared_ptr<Curve2D>> preview_curves() const = 0;

    // Commit created curves (transitions state to Idle)
    virtual std::vector<std::shared_ptr<Curve2D>> commit_curves() = 0;
};

// =============================================================================
// Concrete Drawing Tools
// =============================================================================

class LineTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::Line; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    Point2D p0_{};
    Point2D p1_{};
};

class RectangleTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::Rectangle; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    Point2D corner0_{};
    Point2D corner1_{};
};

class CircleTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::Circle; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    Point2D center_{};
    double radius_{0.0};
};

class EllipseTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::Ellipse; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    int step_{0}; // 0: wait center, 1: wait major axis pt, 2: wait minor axis pt
    Point2D center_{};
    double rx_{1.0};
    double ry_{0.5};
    double rotation_{0.0};
};

class RegularPolygonTool final : public InteractiveDrawingTool {
public:
    explicit RegularPolygonTool(size_t sides = 6) : sides_(sides >= 3 ? sides : 3) {}

    DrawingToolType tool_type() const noexcept override { return DrawingToolType::RegularPolygon; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    void set_sides(size_t sides) noexcept { sides_ = sides >= 3 ? sides : 3; }
    [[nodiscard]] size_t sides() const noexcept { return sides_; }

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    size_t sides_{6};
    Point2D center_{};
    Point2D outer_pt_{};
};

class ArbitraryPolygonTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::ArbitraryPolygon; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;
    void finish(); // Closes polygon

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    std::vector<Point2D> vertices_;
    Point2D current_cursor_{};
};

class Arc3PointTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::Arc3Point; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    int step_{0}; // 0: wait p1, 1: wait p2, 2: wait p3
    Point2D p1_{};
    Point2D p2_{};
    Point2D p3_{};
};

class ArcCenterRadiusTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::ArcCenterRadius; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    int step_{0}; // 0: wait center, 1: wait start pt, 2: wait sweep pt
    Point2D center_{};
    Point2D start_pt_{};
    Point2D sweep_pt_{};
};

class CubicBezierTool final : public InteractiveDrawingTool {
public:
    DrawingToolType tool_type() const noexcept override { return DrawingToolType::CubicBezier; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    int step_{0}; // 0: p0, 1: p1, 2: p2, 3: p3
    Point2D p0_{};
    Point2D p1_{};
    Point2D p2_{};
    Point2D p3_{};
};

class FreehandPolylineTool final : public InteractiveDrawingTool {
public:
    explicit FreehandPolylineTool(double simplify_tol = 0.05) : simplify_tol_(simplify_tol) {}

    DrawingToolType tool_type() const noexcept override { return DrawingToolType::FreehandPolyline; }
    ToolState state() const noexcept override { return state_; }
    void reset() override;
    void cancel() override;

    void set_simplify_tolerance(double tol) noexcept { simplify_tol_ = tol > 0.0 ? tol : 0.01; }
    [[nodiscard]] double simplify_tolerance() const noexcept { return simplify_tol_; }

    bool on_pointer_down(const Point2D& pt) override;
    void on_pointer_move(const Point2D& pt) override;
    bool on_pointer_up(const Point2D& pt) override;

    std::vector<std::shared_ptr<Curve2D>> preview_curves() const override;
    std::vector<std::shared_ptr<Curve2D>> commit_curves() override;

private:
    ToolState state_{ToolState::Idle};
    double simplify_tol_{0.05};
    std::vector<Point2D> raw_points_;
};

/**
 * @brief Controller coordinating active interactive drawing tools and committing finished geometry.
 */
class DrawingToolController {
public:
    DrawingToolController();

    void select_tool(DrawingToolType type);
    [[nodiscard]] DrawingToolType active_tool_type() const noexcept;
    [[nodiscard]] InteractiveDrawingTool* active_tool() noexcept { return current_tool_.get(); }
    [[nodiscard]] const InteractiveDrawingTool* active_tool() const noexcept { return current_tool_.get(); }

    bool on_pointer_down(const Point2D& pt);
    void on_pointer_move(const Point2D& pt);
    bool on_pointer_up(const Point2D& pt);
    void cancel();

    [[nodiscard]] std::vector<std::shared_ptr<Curve2D>> preview_curves() const;

    /**
     * @brief Commits finished geometry from active tool directly to a SketchDocument2D.
     * @return Number of entities added to document.
     */
    size_t commit_to_document(SketchDocument2D& doc, bool is_construction = false, const std::string& layer = "0");

private:
    std::unique_ptr<InteractiveDrawingTool> current_tool_;
};

} // namespace shunya::cad2d
