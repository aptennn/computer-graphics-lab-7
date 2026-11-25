#include "wireframe_canvas.h"
#include "camera.h"
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "wireframe_renderer.h"
#include "zbuffer_renderer.h"

// Холст — отдельный QWidget, поэтому каркас не попадает на панель управления.
// Ссылки позволяют видеть актуальные вершины после вращения без копирования модели.
WireframeCanvas::WireframeCanvas(const geometry::Polyhedron& model,
                               const double& edgeLength, QWidget* parent)
    : QWidget(parent), model_(model), edgeLength_(edgeLength) {
    setObjectName("wireframeCanvas");
    setMinimumSize(240, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// Участник 3. Меняем выбранную проекцию и запрашиваем перерисовку.
// Координаты модели сохраняются: ракурс и проекция действуют только на изображение.
void WireframeCanvas::setProjectionType(geometry::ProjectionType type) {
    projectionType_ = type;
    update();
}

// Участник 3. Храним расстояние камеры в единицах исходной длины ребра.
// Например, ребро=2 и distanceInEdges=6 дают c=12 при проецировании.
// c влияет на перспективное деление w=1-z/c; в аксонометрии этот параметр не нужен.
void WireframeCanvas::setCameraDistance(double distanceInEdges) {
    if (!std::isfinite(distanceInEdges) || distanceInEdges <= 0.0) {
        throw std::invalid_argument("Camera distance must be finite and positive");
    }
    cameraDistanceInEdges_ = distanceInEdges;
    update();
}

// Участник 3. Сохраняем параметры вспомогательной прямой для демонстрации вращения.
// coordinateAxis: 0=X, 1=Y, 2=Z; throughCenter выбирает C вместо O.
// Для произвольной прямой используем переданные A/B. Это параметры подсказки,
// они не добавляют вершин к многограннику и не меняют его геометрию.
void WireframeCanvas::setRotationGuide(RotationGuide guide, int coordinateAxis, bool throughCenter,
                                       const geometry::Point& a, const geometry::Point& b) {
    rotationGuide_ = guide;
    coordinateAxis_ = coordinateAxis;
    throughCenter_ = throughCenter;
    lineA_ = a;
    lineB_ = b;
    update();
}

void WireframeCanvas::setOrbitCamera(bool enabled, double azimuthRad,
                                     double elevationRad) {
    orbitEnabled_ = enabled;
    orbitAzimuthRad_ = azimuthRad;
    orbitElevationRad_ = elevationRad;
    update();
}

// Лилия: меняем направление параллельного обзора для всей сцены.
void WireframeCanvas::setViewDirection(const geometry::Point& direction) {
    const double length = std::hypot(direction.x, direction.y, direction.z);
    if (!std::isfinite(length) || length <= 1e-12) return;
    axonometricView_ = geometry::Projection::viewFromDirection(direction);
    update();
}

geometry::ProjectionView WireframeCanvas::projectionView() const {
    const double distance = cameraDistanceInEdges_ * edgeLength_;

    if (!orbitEnabled_) {
        auto frame = geometry::Projection::fixedView(projectionType_, distance);
        if (projectionType_ == geometry::ProjectionType::Axonometric && axonometricView_) {
            frame.view = *axonometricView_;
        }
        return frame;
    }

    const geometry::OrbitCamera camera(
        model_.center(), distance, orbitAzimuthRad_, orbitElevationRad_);

    return {
        geometry::ProjectionType::Perspective,
        distance,
        camera.viewMatrix(),
        camera.projectionMatrix()
    };
}

// Участник 3, связь с отрисовкой: вызывается Qt после update() или изменения окна.
// Порядок: спроецировать рёбра и ось -> подобрать масштаб вида -> нарисовать.
// Масштаб вида лишь размещает изображение в окне; это не Mat4::scale модели.
// В режиме каркаса QPainter рисует уже вычисленные отрезки, а в режиме заливки
// выводит изображение, подготовленное Z-буфером. 3D-математика выполняется нашим кодом.
void WireframeCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    painter.setRenderHint(QPainter::Antialiasing);

    // Отступы оставляют место для пера и подписи даже после изменения окна.
    const QRectF viewport = QRectF(rect()).adjusted(24, 24, -24, -48);
    if (viewport.width() <= 0 || viewport.height() <= 0) {
        return;
    }

    const auto view = projectionView();
    const auto edges = geometry::Projection::projectEdges(model_, view, cullBackfaces_);

    std::vector<std::vector<geometry::ProjectedEdge>> additionalEdges;
    additionalEdges.reserve(additionalModels_.size());
    for (const auto& model : additionalModels_) {
        additionalEdges.push_back(geometry::Projection::projectEdges(model, view, cullBackfaces_));
    }

    double extentX = 0.0;
    double extentY = 0.0;
    for (const auto& edge : edges) {
        for (const auto& p : {edge.first, edge.second}) {
            extentX = std::max(extentX, std::abs(p.x));
            extentY = std::max(extentY, std::abs(p.y));
        }
    }
    for (const auto& objectEdges : additionalEdges)
        for (const auto& edge : objectEdges)
            for (const auto& p : {edge.first, edge.second}) {
                extentX = std::max(extentX, std::abs(p.x));
                extentY = std::max(extentY, std::abs(p.y));
            }

    // Для оси через центр: A=C, B=C+направление X/Y/Z. Центр берём заново,
    // поэтому подсказка следует за моделью после смещения. AB остаётся мировой прямой.
    geometry::Point a = lineA_, b = lineB_;
    const bool coordinateGuide = rotationGuide_ == RotationGuide::CoordinateAxis;
    if (coordinateGuide) {
        a = throughCenter_ ? model_.center() : geometry::Point{};
        b = a;
        if (coordinateAxis_ == 0) b.x += edgeLength_;
        else if (coordinateAxis_ == 1) b.y += edgeLength_;
        else b.z += edgeLength_;
    }
    const bool validGuide = rotationGuide_ != RotationGuide::Hidden &&
        std::hypot(b.x - a.x, b.y - a.y, b.z - a.z) > 1e-12;
    // Ось проходит те же матрицы вида/проекции и отсечение, что и рёбра модели.
    const auto guide = validGuide
        ? geometry::Projection::projectSegment(a, b, view) : std::nullopt;
    const auto pointA = validGuide
        ? geometry::Projection::projectSegment(a, a, view) : std::nullopt;
    const auto pointB = validGuide && !coordinateGuide
        ? geometry::Projection::projectSegment(b, b, view) : std::nullopt;
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
    double scale = autoFit_ ? std::numeric_limits<double>::infinity()
                            : 0.25 * std::min(viewport.width(), viewport.height()) / 2.0;
    if (extentX > 0.0) scale = std::min(scale, (viewport.width() - 20) * 0.5 / extentX);
    if (extentY > 0.0) scale = std::min(scale, (viewport.height() - 20) * 0.5 / extentY);
    if (!std::isfinite(scale)) scale = 1.0;

    painter.save();
    painter.setClipRect(viewport);
    if (filledMode_) {
        std::vector<geometry::RasterObject> objects{{&model_, qRgb(66, 133, 194)}};
        constexpr QRgb colors[] = {qRgb(220, 112, 70), qRgb(104, 165, 91), qRgb(157, 110, 178)};
        for (std::size_t i = 0; i < additionalModels_.size(); ++i)
            objects.push_back({&additionalModels_[i], colors[i % 3]});
        const QImage frame = geometry::ZBufferRenderer::render(
            objects, view, size(), viewport.center(), scale, cullBackfaces_);
        painter.drawImage(0, 0, frame);
    } else {
        painter.setPen(QPen(Qt::black, 1.5));
        geometry::WireframeRenderer::draw(painter, edges, viewport.center(), scale);
        const QColor colors[] = {QColor(220, 112, 70), QColor(104, 165, 91), QColor(157, 110, 178)};
        for (std::size_t i = 0; i < additionalEdges.size(); ++i) {
            painter.setPen(QPen(colors[i % 3], 1.5));
            geometry::WireframeRenderer::draw(painter, additionalEdges[i], viewport.center(), scale);
        }
    }
    if (guide) {
        const QColor color = coordinateGuide ? QColor("#1565c0") : QColor("#c65d00");
        // Из проекционных единиц в пиксели: u=cx+s*x, v=cy-s*y.
        // Минус у Y нужен потому, что экранная Y растёт вниз.
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
            // Берём ближайшую к центру холста точку прямой и продолжаем её в обе стороны.
            // clipRect ограничит рисунок холстом: AB показывается как прямая, не только отрезок.
            const QPointF anchor = first + direction * (offset.x() * direction.x() + offset.y() * direction.y());
            painter.drawLine(anchor - direction * reach, anchor + direction * reach);
            // Стрелка указывает положительное направление оси (A -> B).
            const QPointF tip = coordinateGuide ? first + direction * 55 : second;
            const QPointF normal(-direction.y(), direction.x());
            painter.setPen(QPen(color, 2));
            painter.drawLine(tip, tip - direction * 10 + normal * 5);
            painter.drawLine(tip, tip - direction * 10 - normal * 5);
        }
        // Метки A/B/C/O: кружок, подпись и ограничение подписи границами холста.
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
        // Если ось направлена вдоль взгляда, её точки проецируются в одно место.
        // Вместо неразличимой прямой показываем кольцо; это не ошибка вращения.
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
    if (!cullBackfaces_ && edges.empty() && additionalModels_.empty()) {
        painter.drawText(viewport, Qt::AlignCenter,
                         "Модель за камерой. Увеличьте расстояние камеры.");
    }
    painter.drawText(QRectF(12, height() - 32, width() - 24, 24),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("%1 · %2").arg(filledMode_ ? "Z-буфер" : "Каркасный режим")
                         .arg(projectionType_ == geometry::ProjectionType::Perspective
                                  ? "Перспективная" : "Аксонометрическая"));
}
