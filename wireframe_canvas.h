#pragma once

#include <QWidget>

#include "geometry.h"
#include "projection.h"

// Холст рисует модель только в своей области, отдельно от панели управления.
class WireframeCanvas : public QWidget {
public:
    enum class RotationGuide { Hidden, CoordinateAxis, ArbitraryLine };
    explicit WireframeCanvas(const geometry::Polyhedron& model,
                             const double& edgeLength, QWidget* parent = nullptr);
    void setProjectionType(geometry::ProjectionType type);
    void setCameraDistance(double distanceInEdges);
    void setRotationGuide(RotationGuide guide, int coordinateAxis, bool throughCenter,
                          const geometry::Point& a, const geometry::Point& b);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const geometry::Polyhedron& model_;
    const double& edgeLength_;
    geometry::ProjectionType projectionType_ = geometry::ProjectionType::Axonometric;
    double cameraDistanceInEdges_ = 6.0;
    RotationGuide rotationGuide_ = RotationGuide::Hidden;
    int coordinateAxis_ = 0;
    bool throughCenter_ = false;
    geometry::Point lineA_;
    geometry::Point lineB_{1, 1, 1};
};
