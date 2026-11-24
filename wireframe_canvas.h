#pragma once

#include <QWidget>

#include "geometry.h"
#include "projection.h"
#include <vector>

// Холст рисует модель только в своей области, отдельно от панели управления.
class WireframeCanvas : public QWidget {
public:
    enum class RotationGuide { Hidden, CoordinateAxis, ArbitraryLine };
    explicit WireframeCanvas(const geometry::Polyhedron& model,
                             const double& edgeLength, QWidget* parent = nullptr);
    void setProjectionType(geometry::ProjectionType type);
    void setCameraDistance(double distanceInEdges);
    void setAutoFit(bool enabled) { autoFit_ = enabled; update(); }
    // Егор, лабораторная №8: общий буфер глубины для основной и дополнительных моделей.
    void setFilledMode(bool enabled) { filledMode_ = enabled; update(); }
    void setAdditionalModels(std::vector<geometry::Polyhedron> models) {
        additionalModels_ = std::move(models); update();
    }
    void setRotationGuide(RotationGuide guide, int coordinateAxis, bool throughCenter,
                          const geometry::Point& a, const geometry::Point& b);
    void setCullBackfaces(bool enabled) { cullBackfaces_ = enabled; update(); }
    // Лилия
    void setViewDirection(const geometry::Point& direction) {
        const double len = std::hypot(direction.x, direction.y, direction.z);
        if (len > 1e-12) viewDirection_ = {direction.x / len, direction.y / len, direction.z / len};
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const geometry::Polyhedron& model_;
    const double& edgeLength_;
    geometry::ProjectionType projectionType_ = geometry::ProjectionType::Axonometric;
    double cameraDistanceInEdges_ = 6.0;
    bool autoFit_ = false;
    bool filledMode_ = false;
    std::vector<geometry::Polyhedron> additionalModels_;
    RotationGuide rotationGuide_ = RotationGuide::Hidden;
    int coordinateAxis_ = 0;
    bool throughCenter_ = false;
    geometry::Point lineA_;
    geometry::Point lineB_{1, 1, 1};
    bool cullBackfaces_ = false;
    geometry::Point viewDirection_{0, 0, 1};   // направление обзора (мировое)
};
