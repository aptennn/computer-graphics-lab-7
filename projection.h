#pragma once

#include "geometry.h"
#include <optional>

namespace geometry {

enum class ProjectionType { Axonometric, Perspective };

// Общие параметры одного кадра для каркаса, осей и Z-буфера.
struct ProjectionView {
    ProjectionType type;
    double cameraDistance;
    Mat4 view;
    Mat4 projection;
};

// Результат проецирования одного ребра: используются x/y, а z равна 0.
// Это ещё не пиксели: масштаб и положение на холсте задаются при отрисовке.
struct ProjectedEdge {
    Point first;
    Point second;
};

// Лекция 9: мировые координаты -> вид -> проекция -> деление на w.
class Projection {
public:
    // Повороты вокруг Y и X для прямоугольной изометрии (слайд 39).
    static Mat4 viewMatrix();
    // Ось +Z вида направлена от сцены к наблюдателю.
    static Mat4 viewFromDirection(const Point& direction);
    static Mat4 axonometricMatrix();
    // Центр проекции (0,0,c), плоскость z=0; w=1-z/c (слайд 54).
    static Mat4 perspectiveMatrix(double cameraDistance);

    static ProjectionView fixedView(ProjectionType type, double cameraDistance);

    static std::optional<ProjectedEdge> projectSegment(
        const Point& a, const Point& b, const ProjectionView& frame);

    static std::vector<ProjectedEdge> projectEdges(
        const Polyhedron& model, const ProjectionView& frame,
        bool cullBackfaces = false);

    // Лилия: одна проверка лицевой стороны для каркаса и заливки.
    static bool isFrontFacing(const Point& normalInView, const Point& pointInView,
                              const ProjectionView& frame);

    // Отрезок и вспомогательная ось проходят те же преобразования, что и модель.
    static std::optional<ProjectedEdge> projectSegment(const Point& a, const Point& b,
                                                       ProjectionType type, double cameraDistance);

    // Перед делением на w отсекаем рёбра у камеры, сохраняя видимую часть.
    static std::vector<ProjectedEdge> projectEdges(const Polyhedron& model,
                                                  ProjectionType type,
                                                  double cameraDistance);
};

} // namespace geometry
