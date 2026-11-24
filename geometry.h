#pragma once

#include "mat4.h"
#include <cstddef>
#include <vector>

namespace geometry {

// Егор: точка в трехмерном пространстве — основа модели многогранника.
struct Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Егор: грань хранит индексы общих вершин, а не их копии.
// прочит час
struct Polygon {
    std::vector<std::size_t> vertexIndices;
};

// Егор: многогранник объединяет вершины и грани и предоставляет его центр.
class Polyhedron {
public:
    Polyhedron(std::vector<Point> vertices, std::vector<Polygon> faces);

    const std::vector<Point>& vertices() const noexcept { return vertices_; }
    std::vector<Point>& vertices() noexcept { return vertices_; }
    const std::vector<Polygon>& faces() const noexcept { return faces_; }

    // Центр вычисляется как среднее координат вершин; правильные тела ниже
    // изначально расположены вокруг точки (0, 0, 0).
    Point center() const noexcept;

    // Лилия: аффинные преобразования через матрицы Mat4.
    void applyMatrix(const Mat4& matrix);
    void translate(double dx, double dy, double dz);
    void rotate(double angleRad, char axis);            // 'x' | 'y' | 'z'
    void rotateAroundCenter(double angleRad, char axis);
    void rotateAroundLine(const Point& a, const Point& b, double angleRad);
    void scale(double sx, double sy, double sz);
    void scaleAroundCenter(double sx, double sy, double sz);
    void reflect(const Mat4& reflectionMatrix);

private:
    std::vector<Point> vertices_;
    std::vector<Polygon> faces_;
};

// Лилия: Внешняя нормаль грани: единичный вектор, направленный наружу от центра.
Point faceNormal(const Polyhedron& polyhedron, const Polygon& face);

// Егор: обязательные и бонусные правильные многогранники из лабораторной №6.
enum class RegularSolid {
    Tetrahedron,
    Hexahedron,
    Octahedron,
    Icosahedron,
    Dodecahedron
};

// Егор: создание выбранного многогранника с заданной длиной ребра.
// Длина ребра должна быть конечной и положительной.
Polyhedron makeRegularSolid(RegularSolid solid, double edgeLength = 2.0);

// Лилия: Фигура вращения: образующая (набор точек), ось 'x'|'y'|'z', число разбиений.
Polyhedron makeSolidOfRevolution(const std::vector<Point>& generatrix,
                                  char axis,
                                  int segments);

} // namespace geometry
