#pragma once

namespace geometry {

struct Point;

// аффинные преобразования в однородных координатах
struct Mat4 {
    double m[4][4] = {};

    static Mat4 identity();
    static Mat4 translation(double dx, double dy, double dz);
    static Mat4 scale(double sx, double sy, double sz);
    static Mat4 rotationX(double angleRad);
    static Mat4 rotationY(double angleRad);
    static Mat4 rotationZ(double angleRad);
    // Лекция 9, слайды 20–28: перенос, совмещение с Z, поворот и возврат.
    static Mat4 rotationAroundLine(const Point& a, const Point& b, double angleRad);

    // Отражение относительно координатной плоскости:
    // XY -> инверсия Z; XZ -> инверсия Y; YZ -> инверсия X
    static Mat4 reflectionXY();
    static Mat4 reflectionXZ();
    static Mat4 reflectionYZ();

    Mat4 operator*(const Mat4& other) const;
    Point transformPoint(const Point& p) const;   
    Point transformVector(const Point& v) const;  

    double determinant3x3() const; 
};

} // namespace geometry
