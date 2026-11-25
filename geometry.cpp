#include "geometry.h"
#include "mat4.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace geometry {

namespace {
//вычисляет вектор от b к a.
Point subtract(Point a, Point b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

    // векторное произведение
Point cross(Point a, Point b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
// — скалярное произведени
double dot(Point a, Point b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Point scaled(Point p, double factor) {
    return {p.x * factor, p.y * factor, p.z * factor};
}

// Егор: построение икосаэдра по схеме из лекции: два пятиугольника
// на окружностях цилиндра, повернутые друг относительно друга на 36 градусов,
// и по одной вершине над и под ними. Цилиндр — только вспомогательная схема.
Polyhedron makeIcosahedron(double edgeLength) {
    constexpr double pi = 3.14159265358979323846;
    // Сначала строим фигуру с ребром 1, затем умножаем координаты на edgeLength.
    const double radius = 1.0 / (2.0 * std::sin(pi / 5.0));
    const double horizontalGap = 2.0 * radius * std::sin(pi / 10.0);
    const double halfRingGap = std::sqrt(1.0 - horizontalGap * horizontalGap) / 2.0;
    const double poleZ = halfRingGap + std::sqrt(1.0 - radius * radius);

    std::vector<Point> vertices;
    vertices.reserve(12);
    vertices.push_back({0, 0, poleZ});  // 0: верхняя вершина на оси
    for (int i = 0; i < 5; ++i) {
        const double angle = 2.0 * pi * i / 5.0;
        vertices.push_back({radius * std::cos(angle), radius * std::sin(angle), halfRingGap});
    }
    for (int i = 0; i < 5; ++i) {
        const double angle = 2.0 * pi * i / 5.0 + pi / 5.0;
        vertices.push_back({radius * std::cos(angle), radius * std::sin(angle), -halfRingGap});
    }
    vertices.push_back({0, 0, -poleZ}); // 11: нижняя вершина на оси

    // Пять треугольников сверху, десять между кольцами и пять снизу.
    std::vector<Polygon> faces;
    faces.reserve(20);
    const auto addFace = [&](std::size_t a, std::size_t b, std::size_t c) {
        const Point normal = cross(subtract(vertices[b], vertices[a]),
                                   subtract(vertices[c], vertices[a]));
        if (dot(normal, vertices[a]) < 0.0) {
            std::swap(b, c);
        }
        faces.push_back({{a, b, c}});
    };
    for (std::size_t i = 0; i < 5; ++i) {
        const std::size_t upper = 1 + i;
        const std::size_t nextUpper = 1 + (i + 1) % 5;
        const std::size_t lower = 6 + i;
        const std::size_t nextLower = 6 + (i + 1) % 5;
        addFace(0, upper, nextUpper);
        addFace(upper, lower, nextUpper);
        addFace(lower, nextLower, nextUpper);
        addFace(11, nextLower, lower);
    }

    for (Point& vertex : vertices) {
        vertex = scaled(vertex, edgeLength);
    }
    return {std::move(vertices), std::move(faces)};
}

// Егор: додекаэдр строится как двойственный икосаэдру: центры его
// треугольных граней становятся вершинами пятиугольников.
Polyhedron makeDodecahedron(double edgeLength) {
    const Polyhedron icosahedron = makeIcosahedron(2.0);
    std::vector<Point> vertices;
    vertices.reserve(icosahedron.faces().size());
    for (const Polygon& face : icosahedron.faces()) {
        Point center;
        for (std::size_t index : face.vertexIndices) {
            const Point& p = icosahedron.vertices()[index];
            center.x += p.x;
            center.y += p.y;
            center.z += p.z;
        }
        vertices.push_back(scaled(center, 1.0 / 3.0));
    }

    std::vector<Polygon> faces;
    faces.reserve(icosahedron.vertices().size());
    for (std::size_t i = 0; i < icosahedron.vertices().size(); ++i) {
        std::vector<std::size_t> adjacent;
        for (std::size_t j = 0; j < icosahedron.faces().size(); ++j) {
            const auto& indices = icosahedron.faces()[j].vertexIndices;
            if (std::find(indices.begin(), indices.end(), i) != indices.end()) {
                adjacent.push_back(j);
            }
        }

        const Point axis = icosahedron.vertices()[i];
        const Point reference = vertices[adjacent.front()];
        const Point tangent = subtract(reference, scaled(axis, dot(reference, axis) / dot(axis, axis)));
        const Point bitangent = cross(axis, tangent);
        std::sort(adjacent.begin(), adjacent.end(), [&](std::size_t a, std::size_t b) {
            return std::atan2(dot(vertices[a], bitangent), dot(vertices[a], tangent)) <
                   std::atan2(dot(vertices[b], bitangent), dot(vertices[b], tangent));
        });
        faces.push_back({std::move(adjacent)});
    }

    const auto& firstFace = faces.front().vertexIndices;
    const Point edge = subtract(vertices[firstFace[0]], vertices[firstFace[1]]);
    const double scale = edgeLength / std::sqrt(dot(edge, edge));
    for (Point& vertex : vertices) {
        vertex = scaled(vertex, scale);
    }
    return {std::move(vertices), std::move(faces)};
}

} // namespace

// Лилия: Фигура вращения: кольца из образующей, повёрнутые вокруг оси.
Polyhedron makeSolidOfRevolution(const std::vector<Point>& generatrix,
                                 char axis, int segments) {
    if (generatrix.size() < 2)
        throw std::invalid_argument("Generatrix needs at least two points");
    if (segments < 3)
        throw std::invalid_argument("Segments must be at least 3");
    if (axis != 'x' && axis != 'y' && axis != 'z')
        throw std::invalid_argument("Axis must be x, y or z");

    constexpr double pi = 3.14159265358979323846;
    const int rings = segments;
    const std::size_t n = generatrix.size();

    // Возвращает точку, повёрнутую на angle вокруг выбранной оси.
    auto rotatePoint = [&](const Point& p, double angle) -> Point {
        double u, v, w;
        if (axis == 'z')      { u = p.x; v = p.y; w = p.z; }
        else if (axis == 'x') { u = p.y; v = p.z; w = p.x; }
        else                  { u = p.z; v = p.x; w = p.y; }
        const double c = std::cos(angle), s = std::sin(angle);
        const double ru = u * c - v * s;
        const double rv = u * s + v * c;
        if (axis == 'z')      return Point{ru, rv, w};
        else if (axis == 'x') return Point{w, ru, rv};
        else                  return Point{rv, w, ru};
    };

    std::vector<Point> vertices;
    vertices.reserve(n * static_cast<std::size_t>(rings));
    for (int k = 0; k < rings; ++k) {
        const double angle = 2.0 * pi * k / rings;
        for (const Point& p : generatrix)
            vertices.push_back(rotatePoint(p, angle));
    }

    const auto axial = [axis](const Point& p) {
        return axis == 'x' ? p.x : axis == 'y' ? p.y : p.z;
    };
    const auto radial = [axis](const Point& p) {
        if (axis == 'x') return std::hypot(p.y, p.z);
        if (axis == 'y') return std::hypot(p.z, p.x);
        return std::hypot(p.x, p.y);
    };
    // Знак вклада полос в объём задаёт общий обход даже для обратной образующей.
    double volumeSign = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
        const Point& a = generatrix[i - 1];
        const Point& b = generatrix[i];
        const double ra = radial(a), rb = radial(b);
        volumeSign += (axial(b) - axial(a)) * (ra * ra + ra * rb + rb * rb);
    }
    const bool reverseWinding = volumeSign > 0.0;

    // На оси одна из двух половин полосы может схлопнуться в линию.
    std::vector<Polygon> faces;
    faces.reserve(static_cast<std::size_t>(rings) * (n - 1) * 2);
    const auto addTriangle = [&](std::size_t a, std::size_t b, std::size_t c) {
        const auto ab = subtract(vertices[b], vertices[a]);
        const auto ac = subtract(vertices[c], vertices[a]);
        const Point normal = cross(ab, ac);
        const double edgeProduct = std::hypot(ab.x, ab.y, ab.z) *
                                   std::hypot(ac.x, ac.y, ac.z);
        constexpr double tolerance = 8.0 * std::numeric_limits<double>::epsilon();
        if (std::hypot(normal.x, normal.y, normal.z) <= tolerance * edgeProduct) return;
        if (reverseWinding) std::swap(b, c);
        faces.push_back({{a, b, c}});
    };
    for (int k = 0; k < rings; ++k) {
        const int next = (k + 1) % rings;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const std::size_t a = static_cast<std::size_t>(k) * n + i;
            const std::size_t b = static_cast<std::size_t>(k) * n + (i + 1);
            const std::size_t c = static_cast<std::size_t>(next) * n + (i + 1);
            const std::size_t d = static_cast<std::size_t>(next) * n + i;
            addTriangle(a, b, c);
            addTriangle(a, c, d);
        }
    }
    if (faces.empty()) throw std::invalid_argument("Generatrix produces no surface");
    return {std::move(vertices), std::move(faces)};
}

// Лилия: отсечение граней
Point faceNormal(const Polyhedron& polyhedron, const Polygon& face) {
    const auto& indices = face.vertexIndices;
    const auto& verts = polyhedron.vertices();
    if (indices.size() < 3) return {};

    // Сумма векторных произведений сохраняет обход грани, даже если первые
    // три вершины коллинеарны. Для открытой поверхности ориентацию задаёт обход.
    const Point origin = verts[indices.front()];
    Point n;
    for (std::size_t i = 1; i + 1 < indices.size(); ++i) {
        const Point contribution = cross(subtract(verts[indices[i]], origin),
                                         subtract(verts[indices[i + 1]], origin));
        n.x += contribution.x;
        n.y += contribution.y;
        n.z += contribution.z;
    }
    const double len = std::hypot(n.x, n.y, n.z);
    if (!std::isfinite(len) || len == 0.0) return {};
    return {n.x / len, n.y / len, n.z / len};
}

// Егор: проверка данных модели при создании многогранника.
Polyhedron::Polyhedron(std::vector<Point> vertices, std::vector<Polygon> faces)
    : vertices_(std::move(vertices)), faces_(std::move(faces)) {
    if (vertices_.empty()) {
        throw std::invalid_argument("A polyhedron needs vertices");
    }
    for (const Point& vertex : vertices_) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
            throw std::invalid_argument("Vertex coordinates must be finite");
        }
    }
    for (const Polygon& face : faces_) {
        if (face.vertexIndices.size() < 3) {
            throw std::invalid_argument("A face needs at least three vertices");
        }
        std::unordered_set<std::size_t> uniqueIndices;
        for (std::size_t index : face.vertexIndices) {
            if (index >= vertices_.size() || !uniqueIndices.insert(index).second) {
                throw std::invalid_argument("A face has an invalid or repeated vertex index");
            }
        }
    }
}

// Егор: вычисление центра по координатам всех вершин.
// Опора вращения участника 3: берём среднее ТЕКУЩИХ координат, поэтому центр
// учитывает предыдущие смещения. Для наших правильных тел это геометрический центр.
Point Polyhedron::center() const noexcept {
    Point result;
    for (const Point& vertex : vertices_) {
        result.x += vertex.x;
        result.y += vertex.y;
        result.z += vertex.z;
    }
    const double count = static_cast<double>(vertices_.size());
    result.x /= count;
    result.y /= count;
    result.z /= count;
    return result;
}

// Егор: координаты и грани трех обязательных и двух бонусных тел.
Polyhedron makeRegularSolid(RegularSolid solid, double edgeLength) {
    if (!std::isfinite(edgeLength) || edgeLength <= 0.0) {
        throw std::invalid_argument("Edge length must be finite and positive");
    }

    switch (solid) {
    case RegularSolid::Tetrahedron: {
        const double a = edgeLength / (2.0 * std::sqrt(2.0));
        return {
            {{ a,  a,  a}, {-a, -a,  a}, {-a,  a, -a}, { a, -a, -a}},
            {{{0, 2, 1}}, {{0, 1, 3}}, {{0, 3, 2}}, {{1, 2, 3}}}
        };
    }
    case RegularSolid::Hexahedron: {
        const double a = edgeLength / 2.0;
        return {
            {{-a, -a, -a}, { a, -a, -a}, { a,  a, -a}, {-a,  a, -a},
             {-a, -a,  a}, { a, -a,  a}, { a,  a,  a}, {-a,  a,  a}},
            {{{0, 3, 2, 1}}, {{4, 5, 6, 7}}, {{0, 1, 5, 4}},
             {{3, 7, 6, 2}}, {{0, 4, 7, 3}}, {{1, 2, 6, 5}}}
        };
    }
    case RegularSolid::Octahedron: {
        const double a = edgeLength / std::sqrt(2.0);
        return {
            {{ a, 0, 0}, {-a, 0, 0}, {0,  a, 0}, {0, -a, 0},
             {0, 0,  a}, {0, 0, -a}},
            {{{4, 0, 2}}, {{4, 2, 1}}, {{4, 1, 3}}, {{4, 3, 0}},
             {{5, 2, 0}}, {{5, 1, 2}}, {{5, 3, 1}}, {{5, 0, 3}}}
        };
    }
    case RegularSolid::Icosahedron:
        return makeIcosahedron(edgeLength);
    case RegularSolid::Dodecahedron:
        return makeDodecahedron(edgeLength);
    }
    throw std::invalid_argument("Unknown regular solid");
}

// Лилия: применение произвольной аффинной матрицы ко всем вершинам
// Общая точка подключения: одна матрица участника 3 применяется ко всем вершинам;
// Связи между вершинами сохраняются. При инверсии ориентации меняем обход граней.
void Polyhedron::applyMatrix(const Mat4& matrix) {
    for (Point& v : vertices_) {
        v = matrix.transformPoint(v);
    }
    if (matrix.determinant3x3() < 0.0) {
        for (Polygon& face : faces_) {
            std::reverse(face.vertexIndices.begin(), face.vertexIndices.end());
        }
    }
}

void Polyhedron::translate(double dx, double dy, double dz) {
    applyMatrix(Mat4::translation(dx, dy, dz));
}

void Polyhedron::rotate(double angleRad, char axis) {
    switch (axis) {
    case 'x': applyMatrix(Mat4::rotationX(angleRad)); break;
    case 'y': applyMatrix(Mat4::rotationY(angleRad)); break;
    case 'z': applyMatrix(Mat4::rotationZ(angleRad)); break;
    default: throw std::invalid_argument("Axis must be x, y or z");
    }
}

// Лилия: вращение вокруг оси, проходящей через центр
// Участник 3 подключает этот метод к режиму «Через центр многогранника».
// Ось параллельна мировой X/Y/Z и проходит через C; угол задан в радианах.
// M=T(C)*R*T(-C): сначала перенести C в O=(0,0,0), повернуть, вернуть обратно.
// Пример: C=(3,2,0), P=(4,2,0), Z:+90° => P'=(3,3,0), а C не изменится.
void Polyhedron::rotateAroundCenter(double angleRad, char axis) {
    const Point c = center();
    Mat4 R = Mat4::identity();
    switch (axis) {
    case 'x': R = Mat4::rotationX(angleRad); break;
    case 'y': R = Mat4::rotationY(angleRad); break;
    case 'z': R = Mat4::rotationZ(angleRad); break;
    default: throw std::invalid_argument("Axis must be x, y or z");
    }
    const Mat4 M = Mat4::translation(c.x, c.y, c.z) * R
                 * Mat4::translation(-c.x, -c.y, -c.z);
    applyMatrix(M);
}

void Polyhedron::scale(double sx, double sy, double sz) {
    applyMatrix(Mat4::scale(sx, sy, sz));
}

// Участник 3. Связываем математику Mat4 с моделью: строим матрицу оси AB
// и применяем её ко всем вершинам. При ошибке построения applyMatrix не вызывается,
// поэтому совпадение A и B не изменяет модель. Сами A и B заданы в мировых координатах.
void Polyhedron::rotateAroundLine(const Point& a, const Point& b, double angleRad) {
    applyMatrix(Mat4::rotationAroundLine(a, b, angleRad));
}

// Лилия: масштаб относительно центра многогранника
void Polyhedron::scaleAroundCenter(double sx, double sy, double sz) {
    const Point c = center();
    const Mat4 M = Mat4::translation(c.x, c.y, c.z)
                 * Mat4::scale(sx, sy, sz)
                 * Mat4::translation(-c.x, -c.y, -c.z);
    applyMatrix(M);
}

// Лилия: отражение
void Polyhedron::reflect(const Mat4& reflectionMatrix) {
    applyMatrix(reflectionMatrix);
}
} // namespace geometry
