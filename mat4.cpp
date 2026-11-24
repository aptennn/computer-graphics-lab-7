#include "mat4.h"
#include "geometry.h"

#include <cmath>
#include <stdexcept>

namespace geometry {

Mat4 Mat4::identity() {
    Mat4 r;
    for (int i = 0; i < 4; ++i) r.m[i][i] = 1.0;
    return r;
}

Mat4 Mat4::translation(double dx, double dy, double dz) {
    Mat4 r = identity();
    r.m[0][3] = dx;
    r.m[1][3] = dy;
    r.m[2][3] = dz;
    return r;
}

Mat4 Mat4::scale(double sx, double sy, double sz) {
    Mat4 r = identity();
    r.m[0][0] = sx;
    r.m[1][1] = sy;
    r.m[2][2] = sz;
    return r;
}

Mat4 Mat4::rotationX(double a) {
    Mat4 r = identity();
    const double c = std::cos(a), s = std::sin(a);
    r.m[1][1] = c; r.m[1][2] = -s;
    r.m[2][1] = s; r.m[2][2] =  c;
    return r;
}

Mat4 Mat4::rotationY(double a) {
    Mat4 r = identity();
    const double c = std::cos(a), s = std::sin(a);
    r.m[0][0] =  c; r.m[0][2] = s;
    r.m[2][0] = -s; r.m[2][2] = c;
    return r;
}

Mat4 Mat4::rotationZ(double a) {
    Mat4 r = identity();
    const double c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c; r.m[0][1] = -s;
    r.m[1][0] = s; r.m[1][1] =  c;
    return r;
}

Mat4 Mat4::rotationAroundLine(const Point& a, const Point& b, double angleRad) {
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z) ||
        !std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z) ||
        !std::isfinite(angleRad)) {
        throw std::invalid_argument("Line coordinates and angle must be finite");
    }
    const Point direction{b.x - a.x, b.y - a.y, b.z - a.z};
    const double length = std::hypot(direction.x, direction.y, direction.z);
    if (!std::isfinite(length) || length <= 1e-12) {
        throw std::invalid_argument("Two distinct points are required for a rotation axis");
    }
    const double l = direction.x / length;
    const double m = direction.y / length;
    const double n = direction.z / length;
    const double d = std::hypot(m, n);

    // Rx(psi) переводит (l,m,n) в (l,0,d), Ry(theta) — в (0,0,1).
    // При оси, параллельной X, d=0: первый поворот не нужен.
    const double psi = d > 0.0 ? std::atan2(m, n) : 0.0;
    const double theta = std::atan2(-l, d);

    // В лекции векторы-строки, здесь векторы-столбцы: применяем справа налево.
    return translation(a.x, a.y, a.z)
         * rotationX(-psi) * rotationY(-theta) * rotationZ(angleRad)
         * rotationY(theta) * rotationX(psi)
         * translation(-a.x, -a.y, -a.z);
}

Mat4 Mat4::reflectionXY() { return scale(1.0, 1.0, -1.0); }
Mat4 Mat4::reflectionXZ() { return scale(1.0, -1.0, 1.0); }
Mat4 Mat4::reflectionYZ() { return scale(-1.0, 1.0, 1.0); }

Mat4 Mat4::operator*(const Mat4& other) const {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) sum += m[i][k] * other.m[k][j];
            r.m[i][j] = sum;
        }
    return r;
}

Point Mat4::transformPoint(const Point& p) const {
    const double x = m[0][0]*p.x + m[0][1]*p.y + m[0][2]*p.z + m[0][3];
    const double y = m[1][0]*p.x + m[1][1]*p.y + m[1][2]*p.z + m[1][3];
    const double z = m[2][0]*p.x + m[2][1]*p.y + m[2][2]*p.z + m[2][3];
    const double w = m[3][0]*p.x + m[3][1]*p.y + m[3][2]*p.z + m[3][3];
    if (std::abs(w) > 1e-12 && std::abs(w - 1.0) > 1e-12)
        return {x / w, y / w, z / w};
    return {x, y, z};
}

Point Mat4::transformVector(const Point& v) const {
    return {m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z};
}

double Mat4::determinant3x3() const {
    return m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1])
         - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0])
         + m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);
}

} // namespace geometry
