#include "wireframe_renderer.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace geometry {
    // Егор: обход граней, удаление повторяющихся ребер и вывод линий на экран.

    void WireframeRenderer::draw(QPainter& painter, const std::vector<ProjectedEdge>& edges,
                                 const QPointF& origin, double scale) {
        for (const auto& edge : edges) {
            const QPointF first = origin + QPointF(edge.first.x, -edge.first.y) * scale;
            const QPointF second = origin + QPointF(edge.second.x, -edge.second.y) * scale;
            if (std::isfinite(first.x()) && std::isfinite(first.y()) &&
                std::isfinite(second.x()) && std::isfinite(second.y())) {
                painter.drawLine(first, second);
                }
        }
    }
}
