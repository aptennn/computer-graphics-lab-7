#include "wireframe_renderer.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace geometry {
    // Егор: обход граней, удаление повторяющихся ребер и вывод линий на экран.

    // Участник 3. Перегрузка для рёбер, уже подготовленных Projection::projectEdges().
    // Видовая матрица, отсечение и деление на w выполнены раньше; здесь только экран:
    // u=origin.x+scale*x, v=origin.y-scale*y, затем QPainter::drawLine().
    // Проверка конечности не допускает передачу NaN/бесконечностей в рисование.
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
