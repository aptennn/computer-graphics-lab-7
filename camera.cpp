//
// Created by Michael on 25.11.2026.
//
#include "camera.h"

#include "projection.h"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace geometry {
namespace {

Point crossProduct(const Point& a, const Point& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

} // namespace

OrbitCamera::OrbitCamera(Point target, double radius,
                         double azimuthRad, double elevationRad)
    : target_(target),
      radius_(radius),
      azimuthRad_(azimuthRad),
      elevationRad_(elevationRad) {
    if (!std::isfinite(target.x) || !std::isfinite(target.y) ||
        !std::isfinite(target.z) ||
        !std::isfinite(radius) || radius <= 0.0 ||
        !std::isfinite(azimuthRad) || !std::isfinite(elevationRad) ||
        std::abs(elevationRad) >= std::numbers::pi / 2.0) {
        throw std::invalid_argument("Invalid orbit camera parameters");
    }
}

// Единичный вектор от цели к камере.
Point OrbitCamera::backwardAxis() const {
    const double horizontal = std::cos(elevationRad_);
    return {
        horizontal * std::cos(azimuthRad_),
        horizontal * std::sin(azimuthRad_),
        std::sin(elevationRad_)
    };
}

Point OrbitCamera::position() const {
    const Point back = backwardAxis();
    return {
        target_.x + radius_ * back.x,
        target_.y + radius_ * back.y,
        target_.z + radius_ * back.z
    };
}

Point OrbitCamera::direction() const {
    const Point back = backwardAxis();
    return {-back.x, -back.y, -back.z};
}

Mat4 OrbitCamera::viewMatrix() const {
    const Point back = backwardAxis();
    const Point right{-std::sin(azimuthRad_), std::cos(azimuthRad_), 0.0};
    const Point up = crossProduct(back, right);

    Mat4 basis = Mat4::identity();
    const Point axes[] = {right, up, back};
    for (int row = 0; row < 3; ++row) {
        basis.m[row][0] = axes[row].x;
        basis.m[row][1] = axes[row].y;
        basis.m[row][2] = axes[row].z;
    }

    // Сначала переносим цель в начало, затем измеряем координаты вдоль осей камеры.
    return basis * Mat4::translation(-target_.x, -target_.y, -target_.z);
}

Mat4 OrbitCamera::projectionMatrix() const {
    return Projection::perspectiveMatrix(radius_);
}

} // namespace geometry
