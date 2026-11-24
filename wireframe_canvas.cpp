#include "wireframe_canvas.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "wireframe_renderer.h"

WireframeCanvas::WireframeCanvas(const geometry::Polyhedron& model,
                               const double& edgeLength, QWidget* parent)
    : QWidget(parent), model_(model), edgeLength_(edgeLength) {
    setObjectName("wireframeCanvas");
    setMinimumSize(240, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WireframeCanvas::setProjectionType(geometry::ProjectionType type) {
    projectionType_ = type;
    update();
}

void WireframeCanvas::setCameraDistance(double distanceInEdges) {
    if (!std::isfinite(distanceInEdges) || distanceInEdges <= 0.0) {
        throw std::invalid_argument("Camera distance must be finite and positive");
    }
    cameraDistanceInEdges_ = distanceInEdges;
    update();
}

void WireframeCanvas::setRotationGuide(RotationGuide guide, int coordinateAxis, bool throughCenter,
                                       const geometry::Point& a, const geometry::Point& b) {
    rotationGuide_ = guide;
    coordinateAxis_ = coordinateAxis;
    throughCenter_ = throughCenter;
    lineA_ = a;
    lineB_ = b;
    update();
}

void WireframeCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    painter.setRenderHint(QPainter::Antialiasing);

    // Отступы оставляют место для пера и подписи даже после изменения окна.
    const QRectF viewport = QRectF(rect()).adjusted(24, 24, -24, -48);
    if (viewport.width() <= 0 || viewport.height() <= 0) {
        return;
    }

    const auto edges = geometry::Projection::projectEdges(
        model_, projectionType_, cameraDistanceInEdges_ * edgeLength_);
    double extentX = 0.0;
    double extentY = 0.0;
    for (const auto& edge : edges) {
        for (const auto& p : {edge.first, edge.second}) {
            extentX = std::max(extentX, std::abs(p.x));
            extentY = std::max(extentY, std::abs(p.y));
        }
    }

    geometry::Point a = lineA_, b = lineB_;
    const bool coordinateGuide = rotationGuide_ == RotationGuide::CoordinateAxis;
    if (coordinateGuide) {
        a = throughCenter_ ? model_.center() : geometry::Point{};
        b = a;
        if (coordinateAxis_ == 0) b.x += edgeLength_;
        else if (coordinateAxis_ == 1) b.y += edgeLength_;
        else b.z += edgeLength_;
    }
    const double cameraDistance = cameraDistanceInEdges_ * edgeLength_;
    const bool validGuide = rotationGuide_ != RotationGuide::Hidden &&
        std::hypot(b.x - a.x, b.y - a.y, b.z - a.z) > 1e-12;
    const auto guide = validGuide ? geometry::Projection::projectSegment(a, b, projectionType_, cameraDistance)
                                 : std::nullopt;
    const auto pointA = validGuide ? geometry::Projection::projectSegment(a, a, projectionType_, cameraDistance)
                                  : std::nullopt;
    const auto pointB = validGuide && !coordinateGuide
        ? geometry::Projection::projectSegment(b, b, projectionType_, cameraDistance) : std::nullopt;
    // Точки AB тоже должны помещаться на холсте; длина пунктирной прямой масштаб не меняет.
    for (const auto& point : {pointA, pointB}) {
        if (point) {
            extentX = std::max(extentX, std::abs(point->first.x));
            extentY = std::max(extentY, std::abs(point->first.y));
        }
    }

    // Начало координат остаётся в центре, чтобы смещение было видно.
    // При выходе за границы уменьшаем масштаб вида, не меняя саму модель.
    // Фиксированный базовый масштаб показывает изменение длины ребра.
    // При необходимости дополнительно уменьшаем вид, чтобы модель поместилась.
    double scale = 0.25 * std::min(viewport.width(), viewport.height()) / 2.0;
    if (extentX > 0.0) scale = std::min(scale, (viewport.width() - 20) * 0.5 / extentX);
    if (extentY > 0.0) scale = std::min(scale, (viewport.height() - 20) * 0.5 / extentY);

    painter.save();
    painter.setClipRect(viewport);
    painter.setPen(QPen(Qt::black, 1.5));
    geometry::WireframeRenderer::draw(painter, edges, viewport.center(), scale);
    if (guide) {
        const QColor color = coordinateGuide ? QColor("#1565c0") : QColor("#c65d00");
        const auto toScreen = [&](const geometry::Point& p) {
            return viewport.center() + QPointF(p.x, -p.y) * scale;
        };
        const QPointF first = toScreen(guide->first), second = toScreen(guide->second);
        const QPointF delta = second - first;
        const double length = std::hypot(delta.x(), delta.y());
        painter.setPen(QPen(color, 2, Qt::DashLine));
        if (length > 1e-6) {
            const QPointF direction = delta / length;
            const double reach = 2 * std::hypot(viewport.width(), viewport.height());
            const QPointF offset = viewport.center() - first;
            const QPointF anchor = first + direction * (offset.x() * direction.x() + offset.y() * direction.y());
            painter.drawLine(anchor - direction * reach, anchor + direction * reach);
            // Стрелка указывает положительное направление оси (A -> B).
            const QPointF tip = coordinateGuide ? first + direction * 55 : second;
            const QPointF normal(-direction.y(), direction.x());
            painter.setPen(QPen(color, 2));
            painter.drawLine(tip, tip - direction * 10 + normal * 5);
            painter.drawLine(tip, tip - direction * 10 - normal * 5);
        }
        const auto marker = [&](const auto& projected, const QString& text) {
            if (!projected) return;
            const QPointF position = toScreen(projected->first);
            painter.setPen(QPen(color, 2));
            painter.setBrush(Qt::white);
            painter.drawEllipse(position, 4, 4);
            const QString labelText = painter.fontMetrics().elidedText(
                text, Qt::ElideRight, std::max(1, static_cast<int>(viewport.width() - 10)));
            QRectF label = painter.boundingRect(QRectF(), Qt::AlignLeft, labelText).adjusted(-3, -2, 3, 2);
            label.moveTopLeft(position + QPointF(9, -label.height() - 4));
            label.moveLeft(std::clamp(label.left(), viewport.left() + 2,
                                     std::max(viewport.left() + 2, viewport.right() - label.width() - 2)));
            label.moveTop(std::clamp(label.top(), viewport.top() + 2,
                                    std::max(viewport.top() + 2, viewport.bottom() - label.height() - 2)));
            painter.fillRect(label, QColor(255, 255, 255, 225));
            painter.drawText(label, Qt::AlignCenter, labelText);
        };
        const QString axisName = QString("%1 · %2").arg(throughCenter_ ? "C" : "O")
                                                        .arg(QChar('X' + coordinateAxis_));
        if (length <= 1e-6) {
            painter.setPen(QPen(color, 2));
            painter.drawEllipse(first, 8, 8);
            marker(pointA, (coordinateGuide ? axisName : "AB") + " · вдоль взгляда");
        } else {
            marker(pointA, coordinateGuide ? axisName : "A");
            marker(pointB, "B");
        }
    }
    painter.restore();

    painter.setPen(Qt::darkGray);
    if (edges.empty()) {
        painter.drawText(viewport, Qt::AlignCenter,
                         "Модель за камерой. Увеличьте расстояние камеры.");
    }
    painter.drawText(QRectF(12, height() - 32, width() - 24, 24),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     projectionType_ == geometry::ProjectionType::Perspective
                         ? "Каркасный режим · Перспективная"
                         : "Каркасный режим · Аксонометрическая");
}
