#pragma once

#include "geometry.h"
#include "projection.h"

#include <QImage>
#include <QPointF>

#include <vector>

namespace geometry {

// Егор, лабораторная №8: объект сцены для общей растеризации и Z-буфера.
struct RasterObject {
    const Polyhedron* model;
    QRgb color;
};

class ZBufferRenderer {
public:
    // Все объекты рисуются в одном буфере глубины; порядок объектов не важен.
    static QImage render(const std::vector<RasterObject>& objects, ProjectionType type,
                         double cameraDistance, QSize size, QPointF origin, double scale);

    static QImage render(const std::vector<RasterObject>& objects,
                         const ProjectionView& frame,
                         QSize size, QPointF origin, double scale, bool cullBackfaces = false);
};

} // namespace geometry
