#pragma once

#include "geometry.h"
#include <optional>

namespace geometry {

enum class ProjectionType { Axonometric, Perspective };

struct ProjectedEdge {
    Point first;
    Point second;
};

// Лекция 9: мировые координаты -> вид -> проекция -> деление на w.
class Projection {
public:
    // Повороты вокруг Y и X для прямоугольной изометрии (слайд 39).
    static Mat4 viewMatrix();
    static Mat4 axonometricMatrix();
    // Центр проекции (0,0,c), плоскость z=0; w=1-z/c (слайд 54).
    static Mat4 perspectiveMatrix(double cameraDistance);
    // Отрезок и вспомогательная ось проходят те же преобразования, что и модель.
    static std::optional<ProjectedEdge> projectSegment(const Point& a, const Point& b,
                                                       ProjectionType type, double cameraDistance);

    // Перед делением на w отсекаем рёбра у камеры, сохраняя видимую часть.
    static std::vector<ProjectedEdge> projectEdges(const Polyhedron& model,
                                                  ProjectionType type,
                                                  double cameraDistance);
};

} // namespace geometry
