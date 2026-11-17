#include "shunya/cad2d/drawing_tools.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <limits>
#include <algorithm>

namespace shunya::cad2d {

namespace {

using shunya::core::base::math::PI;

[[nodiscard]] double point_distance(const Point2D& a, const Point2D& b) noexcept {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] double perpendicular_distance(const Point2D& pt, const Point2D& line_start, const Point2D& line_end) noexcept {
    double dx = line_end.x - line_start.x;
    double dy = line_end.y - line_start.y;
    double len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-14) {
        return point_distance(pt, line_start);
    }
    double num = std::abs((line_end.y - line_start.y) * pt.x - (line_end.x - line_start.x) * pt.y + line_end.x * line_start.y - line_end.y * line_start.x);
    return num / std::sqrt(len_sq);
}

void rdp_recurse(const std::vector<Point2D>& pts, size_t start_idx, size_t end_idx, double epsilon, std::vector<bool>& keep) {
    if (end_idx <= start_idx + 1) return;

    double max_dist = 0.0;
    size_t split_idx = start_idx;

    for (size_t i = start_idx + 1; i < end_idx; ++i) {
        double dist = perpendicular_distance(pts[i], pts[start_idx], pts[end_idx]);
        if (dist > max_dist) {
            max_dist = dist;
            split_idx = i;
        }
    }

    if (max_dist > epsilon) {
        keep[split_idx] = true;
        rdp_recurse(pts, start_idx, split_idx, epsilon, keep);
        rdp_recurse(pts, split_idx, end_idx, epsilon, keep);
    }
}

std::vector<Point2D> ramer_douglas_peucker(const std::vector<Point2D>& pts, double epsilon) {
    if (pts.size() <= 2) return pts;
    std::vector<bool> keep(pts.size(), false);
    keep.front() = true;
    keep.back() = true;

    rdp_recurse(pts, 0, pts.size() - 1, epsilon, keep);

    std::vector<Point2D> result;
    result.reserve(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        if (keep[i]) {
            result.push_back(pts[i]);
        }
    }
    return result;
}

} // namespace

// =============================================================================
// LineTool
// =============================================================================

void LineTool::reset() {
    state_ = ToolState::Idle;
    p0_ = {};
    p1_ = {};
}

void LineTool::cancel() {
    reset();
}

bool LineTool::on_pointer_down(const Point2D& pt) {
    if (state_ == ToolState::Idle || state_ == ToolState::Finished) {
        p0_ = pt;
        p1_ = pt;
        state_ = ToolState::Active;
        return false;
    }
    if (state_ == ToolState::Active) {
        p1_ = pt;
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void LineTool::on_pointer_move(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        p1_ = pt;
    }
}

bool LineTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> LineTool::preview_curves() const {
    if (state_ == ToolState::Active && point_distance(p0_, p1_) > 1e-9) {
        return {std::make_shared<LineSegment2D>(p0_, p1_)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> LineTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    if (point_distance(p0_, p1_) > 1e-9) {
        res.push_back(std::make_shared<LineSegment2D>(p0_, p1_));
    }
    reset();
    return res;
}

// =============================================================================
// RectangleTool
// =============================================================================

void RectangleTool::reset() {
    state_ = ToolState::Idle;
    corner0_ = {};
    corner1_ = {};
}

void RectangleTool::cancel() {
    reset();
}

bool RectangleTool::on_pointer_down(const Point2D& pt) {
    if (state_ == ToolState::Idle || state_ == ToolState::Finished) {
        corner0_ = pt;
        corner1_ = pt;
        state_ = ToolState::Active;
        return false;
    }
    if (state_ == ToolState::Active) {
        corner1_ = pt;
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void RectangleTool::on_pointer_move(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        corner1_ = pt;
    }
}

bool RectangleTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> RectangleTool::preview_curves() const {
    if (state_ == ToolState::Active) {
        std::vector<Point2D> pts = {
            corner0_,
            {corner1_.x, corner0_.y},
            corner1_,
            {corner0_.x, corner1_.y}
        };
        return {std::make_shared<Polyline2D>(std::move(pts), true)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> RectangleTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    double w = std::abs(corner1_.x - corner0_.x);
    double h = std::abs(corner1_.y - corner0_.y);
    if (w > 1e-9 && h > 1e-9) {
        std::vector<Point2D> pts = {
            corner0_,
            {corner1_.x, corner0_.y},
            corner1_,
            {corner0_.x, corner1_.y}
        };
        res.push_back(std::make_shared<Polyline2D>(std::move(pts), true));
    }
    reset();
    return res;
}

// =============================================================================
// CircleTool
// =============================================================================

void CircleTool::reset() {
    state_ = ToolState::Idle;
    center_ = {};
    radius_ = 0.0;
}

void CircleTool::cancel() {
    reset();
}

bool CircleTool::on_pointer_down(const Point2D& pt) {
    if (state_ == ToolState::Idle || state_ == ToolState::Finished) {
        center_ = pt;
        radius_ = 0.0;
        state_ = ToolState::Active;
        return false;
    }
    if (state_ == ToolState::Active) {
        radius_ = point_distance(center_, pt);
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void CircleTool::on_pointer_move(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        radius_ = point_distance(center_, pt);
    }
}

bool CircleTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> CircleTool::preview_curves() const {
    if (state_ == ToolState::Active && radius_ > 1e-9) {
        return {std::make_shared<Circle2D>(center_, radius_)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> CircleTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    if (radius_ > 1e-9) {
        res.push_back(std::make_shared<Circle2D>(center_, radius_));
    }
    reset();
    return res;
}

// =============================================================================
// EllipseTool
// =============================================================================

void EllipseTool::reset() {
    state_ = ToolState::Idle;
    step_ = 0;
    center_ = {};
    rx_ = 1.0;
    ry_ = 0.5;
    rotation_ = 0.0;
}

void EllipseTool::cancel() {
    reset();
}

bool EllipseTool::on_pointer_down(const Point2D& pt) {
    if (step_ == 0) {
        center_ = pt;
        step_ = 1;
        state_ = ToolState::Active;
        return false;
    }
    if (step_ == 1) {
        double dist = point_distance(center_, pt);
        if (dist < 1e-9) dist = 1e-6;
        rx_ = dist;
        rotation_ = std::atan2(pt.y - center_.y, pt.x - center_.x);
        ry_ = rx_ * 0.5;
        step_ = 2;
        return false;
    }
    if (step_ == 2) {
        double dx = pt.x - center_.x;
        double dy = pt.y - center_.y;
        double cos_r = std::cos(-rotation_);
        double sin_r = std::sin(-rotation_);
        double local_y = std::abs(-dx * sin_r + dy * cos_r);
        ry_ = (local_y > 1e-9) ? local_y : (rx_ * 0.5);
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void EllipseTool::on_pointer_move(const Point2D& pt) {
    if (step_ == 1) {
        rx_ = std::max(point_distance(center_, pt), 1e-6);
        rotation_ = std::atan2(pt.y - center_.y, pt.x - center_.x);
        ry_ = rx_ * 0.5;
    } else if (step_ == 2) {
        double dx = pt.x - center_.x;
        double dy = pt.y - center_.y;
        double cos_r = std::cos(-rotation_);
        double sin_r = std::sin(-rotation_);
        double local_y = std::abs(-dx * sin_r + dy * cos_r);
        ry_ = std::max(local_y, 1e-6);
    }
}

bool EllipseTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> EllipseTool::preview_curves() const {
    if (state_ == ToolState::Active && rx_ > 1e-9 && ry_ > 1e-9) {
        return {std::make_shared<Ellipse2D>(center_, rx_, ry_, rotation_)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> EllipseTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    if (rx_ > 1e-9 && ry_ > 1e-9) {
        res.push_back(std::make_shared<Ellipse2D>(center_, rx_, ry_, rotation_));
    }
    reset();
    return res;
}

// =============================================================================
// RegularPolygonTool
// =============================================================================

void RegularPolygonTool::reset() {
    state_ = ToolState::Idle;
    center_ = {};
    outer_pt_ = {};
}

void RegularPolygonTool::cancel() {
    reset();
}

bool RegularPolygonTool::on_pointer_down(const Point2D& pt) {
    if (state_ == ToolState::Idle || state_ == ToolState::Finished) {
        center_ = pt;
        outer_pt_ = pt;
        state_ = ToolState::Active;
        return false;
    }
    if (state_ == ToolState::Active) {
        outer_pt_ = pt;
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void RegularPolygonTool::on_pointer_move(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        outer_pt_ = pt;
    }
}

bool RegularPolygonTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> RegularPolygonTool::preview_curves() const {
    if (state_ == ToolState::Active) {
        double r = point_distance(center_, outer_pt_);
        if (r > 1e-9) {
            double base_angle = std::atan2(outer_pt_.y - center_.y, outer_pt_.x - center_.x);
            std::vector<Point2D> pts;
            pts.reserve(sides_);
            for (size_t i = 0; i < sides_; ++i) {
                double a = base_angle + 2.0 * PI * static_cast<double>(i) / static_cast<double>(sides_);
                pts.push_back({center_.x + r * std::cos(a), center_.y + r * std::sin(a)});
            }
            return {std::make_shared<Polyline2D>(std::move(pts), true)};
        }
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> RegularPolygonTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    double r = point_distance(center_, outer_pt_);
    if (r > 1e-9) {
        double base_angle = std::atan2(outer_pt_.y - center_.y, outer_pt_.x - center_.x);
        std::vector<Point2D> pts;
        pts.reserve(sides_);
        for (size_t i = 0; i < sides_; ++i) {
            double a = base_angle + 2.0 * PI * static_cast<double>(i) / static_cast<double>(sides_);
            pts.push_back({center_.x + r * std::cos(a), center_.y + r * std::sin(a)});
        }
        res.push_back(std::make_shared<Polyline2D>(std::move(pts), true));
    }
    reset();
    return res;
}

// =============================================================================
// ArbitraryPolygonTool
// =============================================================================

void ArbitraryPolygonTool::reset() {
    state_ = ToolState::Idle;
    vertices_.clear();
    current_cursor_ = {};
}

void ArbitraryPolygonTool::cancel() {
    reset();
}

bool ArbitraryPolygonTool::on_pointer_down(const Point2D& pt) {
    if (state_ == ToolState::Idle || state_ == ToolState::Finished) {
        vertices_.clear();
        vertices_.push_back(pt);
        current_cursor_ = pt;
        state_ = ToolState::Active;
        return false;
    }
    if (state_ == ToolState::Active) {
        // If clicking near first vertex and we have at least 3 vertices, finish
        if (vertices_.size() >= 3 && point_distance(pt, vertices_.front()) < 1e-2) {
            finish();
            return true;
        }
        vertices_.push_back(pt);
        current_cursor_ = pt;
        return false;
    }
    return false;
}

void ArbitraryPolygonTool::on_pointer_move(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        current_cursor_ = pt;
    }
}

bool ArbitraryPolygonTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

void ArbitraryPolygonTool::finish() {
    if (vertices_.size() >= 3) {
        state_ = ToolState::Finished;
    } else {
        reset();
    }
}

std::vector<std::shared_ptr<Curve2D>> ArbitraryPolygonTool::preview_curves() const {
    if (state_ == ToolState::Active && !vertices_.empty()) {
        std::vector<Point2D> pts = vertices_;
        pts.push_back(current_cursor_);
        return {std::make_shared<Polyline2D>(std::move(pts), false)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> ArbitraryPolygonTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    if (vertices_.size() >= 3) {
        res.push_back(std::make_shared<Polyline2D>(vertices_, true));
    }
    reset();
    return res;
}

// =============================================================================
// Arc3PointTool
// =============================================================================

void Arc3PointTool::reset() {
    state_ = ToolState::Idle;
    step_ = 0;
    p1_ = {};
    p2_ = {};
    p3_ = {};
}

void Arc3PointTool::cancel() {
    reset();
}

bool Arc3PointTool::on_pointer_down(const Point2D& pt) {
    if (step_ == 0) {
        p1_ = pt;
        p2_ = pt;
        p3_ = pt;
        step_ = 1;
        state_ = ToolState::Active;
        return false;
    }
    if (step_ == 1) {
        p2_ = pt;
        p3_ = pt;
        step_ = 2;
        return false;
    }
    if (step_ == 2) {
        p3_ = pt;
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void Arc3PointTool::on_pointer_move(const Point2D& pt) {
    if (step_ == 1) {
        p2_ = pt;
    } else if (step_ == 2) {
        p3_ = pt;
    }
}

bool Arc3PointTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

namespace {

std::shared_ptr<Arc2D> compute_arc_from_3points(const Point2D& p1, const Point2D& p2, const Point2D& p3) {
    double d = 2.0 * (p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y));
    if (std::abs(d) < 1e-12) return nullptr;

    double p1_sq = p1.x * p1.x + p1.y * p1.y;
    double p2_sq = p2.x * p2.x + p2.y * p2.y;
    double p3_sq = p3.x * p3.x + p3.y * p3.y;

    double ux = (p1_sq * (p2.y - p3.y) + p2_sq * (p3.y - p1.y) + p3_sq * (p1.y - p2.y)) / d;
    double uy = (p1_sq * (p3.x - p2.x) + p2_sq * (p1.x - p3.x) + p3_sq * (p2.x - p1.x)) / d;
    Point2D center{ux, uy};

    double r = point_distance(center, p1);
    double a1 = std::atan2(p1.y - uy, p1.x - ux);
    double a2 = std::atan2(p2.y - uy, p2.x - ux);
    double a3 = std::atan2(p3.y - uy, p3.x - ux);

    auto norm_ang = [](double a) {
        while (a < 0.0) a += 2.0 * PI;
        while (a >= 2.0 * PI) a -= 2.0 * PI;
        return a;
    };

    double sweep12 = norm_ang(a2 - a1);
    double sweep13 = norm_ang(a3 - a1);

    double sweep = 0.0;
    if (sweep12 <= sweep13) {
        sweep = sweep13; // CCW
    } else {
        sweep = sweep13 - 2.0 * PI; // CW
    }

    return std::make_shared<Arc2D>(center, r, a1, sweep);
}

} // namespace

std::vector<std::shared_ptr<Curve2D>> Arc3PointTool::preview_curves() const {
    if (state_ == ToolState::Active && step_ == 2) {
        auto arc = compute_arc_from_3points(p1_, p2_, p3_);
        if (arc) return {arc};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> Arc3PointTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    auto arc = compute_arc_from_3points(p1_, p2_, p3_);
    if (arc) {
        res.push_back(arc);
    }
    reset();
    return res;
}

// =============================================================================
// ArcCenterRadiusTool
// =============================================================================

void ArcCenterRadiusTool::reset() {
    state_ = ToolState::Idle;
    step_ = 0;
    center_ = {};
    start_pt_ = {};
    sweep_pt_ = {};
}

void ArcCenterRadiusTool::cancel() {
    reset();
}

bool ArcCenterRadiusTool::on_pointer_down(const Point2D& pt) {
    if (step_ == 0) {
        center_ = pt;
        start_pt_ = pt;
        sweep_pt_ = pt;
        step_ = 1;
        state_ = ToolState::Active;
        return false;
    }
    if (step_ == 1) {
        start_pt_ = pt;
        sweep_pt_ = pt;
        step_ = 2;
        return false;
    }
    if (step_ == 2) {
        sweep_pt_ = pt;
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void ArcCenterRadiusTool::on_pointer_move(const Point2D& pt) {
    if (step_ == 1) {
        start_pt_ = pt;
    } else if (step_ == 2) {
        sweep_pt_ = pt;
    }
}

bool ArcCenterRadiusTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> ArcCenterRadiusTool::preview_curves() const {
    if (state_ == ToolState::Active && step_ == 2) {
        double r = point_distance(center_, start_pt_);
        if (r > 1e-9) {
            double a0 = std::atan2(start_pt_.y - center_.y, start_pt_.x - center_.x);
            double a1 = std::atan2(sweep_pt_.y - center_.y, sweep_pt_.x - center_.x);
            double sweep = a1 - a0;
            while (sweep < -PI) sweep += 2.0 * PI;
            while (sweep > PI) sweep -= 2.0 * PI;
            return {std::make_shared<Arc2D>(center_, r, a0, sweep)};
        }
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> ArcCenterRadiusTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    double r = point_distance(center_, start_pt_);
    if (r > 1e-9) {
        double a0 = std::atan2(start_pt_.y - center_.y, start_pt_.x - center_.x);
        double a1 = std::atan2(sweep_pt_.y - center_.y, sweep_pt_.x - center_.x);
        double sweep = a1 - a0;
        while (sweep < -PI) sweep += 2.0 * PI;
        while (sweep > PI) sweep -= 2.0 * PI;
        res.push_back(std::make_shared<Arc2D>(center_, r, a0, sweep));
    }
    reset();
    return res;
}

// =============================================================================
// CubicBezierTool
// =============================================================================

void CubicBezierTool::reset() {
    state_ = ToolState::Idle;
    step_ = 0;
    p0_ = {};
    p1_ = {};
    p2_ = {};
    p3_ = {};
}

void CubicBezierTool::cancel() {
    reset();
}

bool CubicBezierTool::on_pointer_down(const Point2D& pt) {
    if (step_ == 0) {
        p0_ = pt;
        p1_ = pt;
        p2_ = pt;
        p3_ = pt;
        step_ = 1;
        state_ = ToolState::Active;
        return false;
    }
    if (step_ == 1) {
        p1_ = pt;
        p2_ = pt;
        p3_ = pt;
        step_ = 2;
        return false;
    }
    if (step_ == 2) {
        p2_ = pt;
        p3_ = pt;
        step_ = 3;
        return false;
    }
    if (step_ == 3) {
        p3_ = pt;
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

void CubicBezierTool::on_pointer_move(const Point2D& pt) {
    if (step_ == 1) {
        p1_ = pt;
        p2_ = pt;
        p3_ = pt;
    } else if (step_ == 2) {
        p2_ = pt;
        p3_ = pt;
    } else if (step_ == 3) {
        p3_ = pt;
    }
}

bool CubicBezierTool::on_pointer_up(const Point2D& pt) {
    (void)pt;
    return state_ == ToolState::Finished;
}

std::vector<std::shared_ptr<Curve2D>> CubicBezierTool::preview_curves() const {
    if (state_ == ToolState::Active && step_ >= 1) {
        return {std::make_shared<BezierCubic2D>(p0_, p1_, p2_, p3_)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> CubicBezierTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    res.push_back(std::make_shared<BezierCubic2D>(p0_, p1_, p2_, p3_));
    reset();
    return res;
}

// =============================================================================
// FreehandPolylineTool
// =============================================================================

void FreehandPolylineTool::reset() {
    state_ = ToolState::Idle;
    raw_points_.clear();
}

void FreehandPolylineTool::cancel() {
    reset();
}

bool FreehandPolylineTool::on_pointer_down(const Point2D& pt) {
    raw_points_.clear();
    raw_points_.push_back(pt);
    state_ = ToolState::Active;
    return false;
}

void FreehandPolylineTool::on_pointer_move(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        if (raw_points_.empty() || point_distance(raw_points_.back(), pt) > 1e-4) {
            raw_points_.push_back(pt);
        }
    }
}

bool FreehandPolylineTool::on_pointer_up(const Point2D& pt) {
    if (state_ == ToolState::Active) {
        on_pointer_move(pt);
        state_ = ToolState::Finished;
        return true;
    }
    return false;
}

std::vector<std::shared_ptr<Curve2D>> FreehandPolylineTool::preview_curves() const {
    if (state_ == ToolState::Active && raw_points_.size() >= 2) {
        return {std::make_shared<Polyline2D>(raw_points_, false)};
    }
    return {};
}

std::vector<std::shared_ptr<Curve2D>> FreehandPolylineTool::commit_curves() {
    std::vector<std::shared_ptr<Curve2D>> res;
    if (raw_points_.size() >= 2) {
        auto simplified = ramer_douglas_peucker(raw_points_, simplify_tol_);
        if (simplified.size() >= 2) {
            res.push_back(std::make_shared<Polyline2D>(std::move(simplified), false));
        }
    }
    reset();
    return res;
}

// =============================================================================
// DrawingToolController
// =============================================================================

DrawingToolController::DrawingToolController() {
    select_tool(DrawingToolType::Line);
}

void DrawingToolController::select_tool(DrawingToolType type) {
    switch (type) {
        case DrawingToolType::Line:
            current_tool_ = std::make_unique<LineTool>();
            break;
        case DrawingToolType::Rectangle:
            current_tool_ = std::make_unique<RectangleTool>();
            break;
        case DrawingToolType::Circle:
            current_tool_ = std::make_unique<CircleTool>();
            break;
        case DrawingToolType::Ellipse:
            current_tool_ = std::make_unique<EllipseTool>();
            break;
        case DrawingToolType::RegularPolygon:
            current_tool_ = std::make_unique<RegularPolygonTool>(6);
            break;
        case DrawingToolType::ArbitraryPolygon:
            current_tool_ = std::make_unique<ArbitraryPolygonTool>();
            break;
        case DrawingToolType::Arc3Point:
            current_tool_ = std::make_unique<Arc3PointTool>();
            break;
        case DrawingToolType::ArcCenterRadius:
            current_tool_ = std::make_unique<ArcCenterRadiusTool>();
            break;
        case DrawingToolType::CubicBezier:
            current_tool_ = std::make_unique<CubicBezierTool>();
            break;
        case DrawingToolType::FreehandPolyline:
            current_tool_ = std::make_unique<FreehandPolylineTool>();
            break;
    }
}

DrawingToolType DrawingToolController::active_tool_type() const noexcept {
    return current_tool_ ? current_tool_->tool_type() : DrawingToolType::Line;
}

bool DrawingToolController::on_pointer_down(const Point2D& pt) {
    return current_tool_ ? current_tool_->on_pointer_down(pt) : false;
}

void DrawingToolController::on_pointer_move(const Point2D& pt) {
    if (current_tool_) {
        current_tool_->on_pointer_move(pt);
    }
}

bool DrawingToolController::on_pointer_up(const Point2D& pt) {
    return current_tool_ ? current_tool_->on_pointer_up(pt) : false;
}

void DrawingToolController::cancel() {
    if (current_tool_) {
        current_tool_->cancel();
    }
}

std::vector<std::shared_ptr<Curve2D>> DrawingToolController::preview_curves() const {
    return current_tool_ ? current_tool_->preview_curves() : std::vector<std::shared_ptr<Curve2D>>{};
}

size_t DrawingToolController::commit_to_document(SketchDocument2D& doc, bool is_construction, const std::string& layer) {
    if (!current_tool_) return 0;
    auto curves = current_tool_->commit_curves();
    for (auto& c : curves) {
        doc.add_entity(std::move(c), is_construction, layer);
    }
    return curves.size();
}

} // namespace shunya::cad2d
