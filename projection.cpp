#include "projection.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace geometry {
namespace {
// Участник 3. Внутренняя функция: концы отрезка УЖЕ в системе камеры.
// Сначала отсекаем недопустимую часть в 3D, потом применяем проекцию с делением на w.
// optional содержит видимый отрезок или nullopt, если выводить нечего.
// A и B переданы по значению: отсечение меняет только копии, исходная модель сохранится.
std::optional<ProjectedEdge> projectCameraSegment(Point a, Point b, ProjectionType type,
                                                  double cameraDistance, const Mat4& projection) {
    if (type == ProjectionType::Perspective) {
        // Камера в (0,0,c). При z=c получаем w=1-z/c=0 — делить нельзя.
        // Оставляем z<=0.99*c, то есть w>=0.01. Число 0.99 — практический отступ.
        const double nearZ = cameraDistance * 0.99;
        if (a.z > nearZ && b.z > nearZ) return std::nullopt;
        // Разные логические значения означают, что только один конец за границей.
        if ((a.z > nearZ) != (b.z > nearZ)) {
            // P(t)=A+t(B-A). Из z(t)=nearZ находим t и заменяем скрытый конец
            // точкой пересечения, сохраняя видимую часть пересекающего ребра.
            const double t = (nearZ - a.z) / (b.z - a.z);
            const Point intersection{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), nearZ};
            if (a.z > nearZ) a = intersection;
            else b = intersection;
        }
    }
    // transformPoint выполняет перспективное деление; при аксонометрии w остаётся 1.
    return ProjectedEdge{projection.transformPoint(a), projection.transformPoint(b)};
}
} // namespace

// Участник 3. Матрица вида задаёт ракурс: переводит мировые точки в систему камеры.
// Лекция 9, слайды 6–7 и 39. Сначала Ry(-45°), затем Rx(35.264...°).
// Вместе с ортографической матрицей получаем изометрию: равное сокращение осей.
// Модель не меняется: матрица применяется к временным копиям вершин при рисовании.
Mat4 Projection::viewMatrix() {
    constexpr double pi = 3.14159265358979323846;
    // Равное сокращение по всем трём осям: psi=-45°, phi=35.264...°.
    return Mat4::rotationX(std::atan(1.0 / std::sqrt(2.0)))
         * Mat4::rotationY(-pi / 4.0);
}

// Участник 3. Ортографическая часть аксонометрии (лекция 9, слайд 39).
// После выбора ракурса проецируем на z=0: (x,y,z,1) -> (x,y,0,1).
// Глубина не меняет размер, параллельность рёбер сохраняется.
// Наклонное объёмное изображение даёт композиция этой матрицы с viewMatrix().
Mat4 Projection::axonometricMatrix() {
    Mat4 result = Mat4::identity();
    result.m[2][2] = 0.0;
    return result;
}

// Участник 3. Центральная проекция из лекции 9, слайд 54.
// Центр проекции (0,0,c), плоскость изображения z=0, всё в системе камеры.
// Матрица переводит (x,y,z,1) в (x,y,0,1-z/c); transformPoint затем делит на w.
// Получаем x'=x/(1-z/c), y'=y/(1-z/c): удалённые от камеры детали меньше.
// Пример: c=10, x=2; при z=5 получаем x'=4, при z=-10 получаем x'=1.
Mat4 Projection::perspectiveMatrix(double cameraDistance) {
    if (!std::isfinite(cameraDistance) || cameraDistance <= 0.0) {
        throw std::invalid_argument("Camera distance must be finite and positive");
    }
    // Индексы с нуля: [3][2] — четвёртая строка, третий столбец, создающий w.
    Mat4 result = axonometricMatrix();
    result.m[3][2] = -1.0 / cameraDistance;
    return result;
}
ProjectionView Projection::fixedView(ProjectionType type, double cameraDistance) {
    const Mat4 projection = type == ProjectionType::Perspective
        ? perspectiveMatrix(cameraDistance)
        : axonometricMatrix();
    return {type, cameraDistance, viewMatrix(), projection};
}

// Участник 3. Полный путь одного отрезка: мировые точки -> вид -> отсечение -> проекция.
// Используется для оси вращения и меток A/B/C/O: ракурс совпадает с ракурсом модели.
// Проекция точки тоже возможна: передаём одинаковые концы A=A.
std::optional<ProjectedEdge> Projection::projectSegment(
        const Point& a, const Point& b, ProjectionType type, double cameraDistance) {
    return projectSegment(a, b, fixedView(type, cameraDistance));
}

std::optional<ProjectedEdge> Projection::projectSegment(
        const Point& a, const Point& b, const ProjectionView& frame) {
    return projectCameraSegment(
        frame.view.transformPoint(a), frame.view.transformPoint(b),
        frame.type, frame.cameraDistance, frame.projection);
}

// Участник 3. Подготовка каркаса целиком; координаты исходной модели не меняются.
// Лекция 9, слайды 57–58: мировые координаты -> камера -> проекция -> экран.
// Здесь возвращаем проекционные координаты; переход к пикселям выполняется позже.
std::vector<ProjectedEdge> Projection::projectEdges(
        const Polyhedron& model, ProjectionType type, double cameraDistance) {
    return projectEdges(model, fixedView(type, cameraDistance));
}

std::vector<ProjectedEdge> Projection::projectEdges(
        const Polyhedron& model, const ProjectionView& frame) {
    const Mat4& view = frame.view;
    const Mat4& projection = frame.projection;
    const ProjectionType type = frame.type;
    const double cameraDistance = frame.cameraDistance;
    // Переводим каждую общую вершину в систему камеры один раз для всех её рёбер.
    // reserve выделяет память; сами элементы добавляются через push_back.
    std::vector<Point> cameraVertices;
    cameraVertices.reserve(model.vertices().size());
    for (const Point& p : model.vertices()) cameraVertices.push_back(view.transformPoint(p));

    // Соседние грани делят общее ребро: множество исключает повторную отрисовку.
    std::set<std::pair<std::size_t, std::size_t>> visited;
    std::vector<ProjectedEdge> result;
    for (const Polygon& face : model.faces()) {
        const auto& indices = face.vertexIndices;
        for (std::size_t i = 0; i < indices.size(); ++i) {
            // Остаток % замыкает последнюю вершину на первую.
            // minmax приводит (i,j) и (j,i) к одной паре: например, (2,5).
            const auto edge = std::minmax(indices[i], indices[(i + 1) % indices.size()]);
            // insert(...).second == false означает, что ребро уже встречалось.
            if (!visited.insert(edge).second) continue;
            if (const auto projected = projectCameraSegment(cameraVertices[edge.first],
                    cameraVertices[edge.second], type, cameraDistance, projection)) {
                // optional непустой: * извлекает видимый спроецированный отрезок.
                result.push_back(*projected);
            }
        }
    }
    return result;
}

} // namespace geometry
