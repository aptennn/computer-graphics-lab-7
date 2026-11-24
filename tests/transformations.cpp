#include "geometry.h"
#include "projection.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double tolerance = 1e-9;
using geometry::Point;
using geometry::Mat4;
using geometry::Projection;
using geometry::ProjectionType;
using geometry::RegularSolid;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double distance(const Point& a, const Point& b) {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}

void equal(const Point& actual, const Point& expected, const std::string& message) {
    require(distance(actual, expected) < tolerance, message);
}

template<class Function>
void invalid(Function function, const std::string& message) {
    bool threw = false;
    try { function(); } catch (const std::invalid_argument&) { threw = true; }
    require(threw, message);
}

void testCenterRotation() {
    for (auto solid : {RegularSolid::Tetrahedron, RegularSolid::Hexahedron,
                       RegularSolid::Octahedron, RegularSolid::Icosahedron,
                       RegularSolid::Dodecahedron}) {
        for (char axis : {'x', 'y', 'z'}) {
            auto model = geometry::makeRegularSolid(solid);
            model.translate(3, -2, 5);
            const auto before = model.vertices();
            const Point center = model.center();
            model.rotateAroundCenter(pi / 2, axis);
            equal(model.center(), center, "Center rotation moved the center");
            for (std::size_t i = 0; i < before.size(); ++i) {
                const Point p{before[i].x - center.x, before[i].y - center.y, before[i].z - center.z};
                Point expected;
                if (axis == 'x') expected = {p.x, -p.z, p.y};
                if (axis == 'y') expected = {p.z, p.y, -p.x};
                if (axis == 'z') expected = {-p.y, p.x, p.z};
                expected = {expected.x + center.x, expected.y + center.y, expected.z + center.z};
                equal(model.vertices()[i], expected, "Incorrect center rotation coordinates");
            }
        }
    }
}

void testLineRotation() {
    const Point a{2, -3, 4};
    const Point b{3, -2, 5};
    const Mat4 matrix = Mat4::rotationAroundLine(a, b, 2 * pi / 3);
    equal(matrix.transformPoint(a), a, "Axis point A moved");
    equal(matrix.transformPoint(b), b, "Axis point B moved");
    equal(matrix.transformPoint({0, -5, 2}), {0, -5, 2}, "Another point on the line moved");
    // Around (1,1,1), +120 degrees sends X to Y and Y to Z.
    equal(matrix.transformPoint({3, -3, 4}), {2, -2, 4}, "Diagonal-axis X -> Y failed");
    equal(matrix.transformPoint({2, -2, 4}), {2, -3, 5}, "Diagonal-axis Y -> Z failed");

    const Point sample{2, -4, 7};
    for (Point axis : {Point{1, 0, 0}, Point{-1, 0, 0}, Point{0, 1, 0},
                        Point{0, -1, 0}, Point{0, 0, 1}, Point{0, 0, -1}}) {
        const double sign = axis.x + axis.y + axis.z;
        const Mat4 expected = axis.x != 0 ? Mat4::rotationX(sign * 0.73)
                            : axis.y != 0 ? Mat4::rotationY(sign * 0.73)
                                          : Mat4::rotationZ(sign * 0.73);
        equal(Mat4::rotationAroundLine({0, 0, 0}, axis, 0.73).transformPoint(sample),
              expected.transformPoint(sample), "Coordinate-parallel axis or sign failed");
    }

    auto model = geometry::makeRegularSolid(RegularSolid::Dodecahedron);
    model.translate(3, 4, -2);
    const auto before = model.vertices();
    const Point end{7, -1, 9};
    model.rotateAroundLine(a, end, 0.87);
    for (std::size_t i = 0; i < before.size(); ++i) {
        for (std::size_t j = 0; j < before.size(); ++j) {
            require(std::abs(distance(before[i], before[j]) -
                             distance(model.vertices()[i], model.vertices()[j])) < tolerance,
                    "Line rotation changed pairwise distances");
        }
    }
    model.rotateAroundLine(a, end, -0.87);
    for (std::size_t i = 0; i < before.size(); ++i)
        equal(model.vertices()[i], before[i], "Inverse rotation failed");
    equal(Mat4::rotationAroundLine(a, end, 0).transformPoint(sample), sample, "Zero angle failed");
    equal(Mat4::rotationAroundLine(a, end, 2 * pi).transformPoint(sample), sample, "Full turn failed");
    equal(Mat4::rotationAroundLine(a, end, 0.87).transformPoint(sample),
          Mat4::rotationAroundLine(end, a, -0.87).transformPoint(sample), "Reversing axis failed");
    invalid([&] { model.rotateAroundLine(a, a, 1); }, "Coincident points accepted");
    for (std::size_t i = 0; i < before.size(); ++i)
        equal(model.vertices()[i], before[i], "Invalid axis modified the model");
    invalid([&] { Mat4::rotationAroundLine(a, end, std::numeric_limits<double>::infinity()); },
            "Infinite angle accepted");
    invalid([&] { Mat4::rotationAroundLine(a, {0, 0, std::numeric_limits<double>::quiet_NaN()}, 1); },
            "NaN coordinate accepted");
}

void testProjections() {
    const Mat4 perspective = Projection::perspectiveMatrix(10);
    equal(perspective.transformPoint({2, 4, 5}), {4, 8, 0}, "Perspective division failed");
    equal(perspective.transformPoint({2, 4, -10}), {1, 2, 0}, "Distant point did not shrink");
    const Mat4 ortho = Projection::axonometricMatrix();
    equal(ortho.transformPoint({2, 4, -100}), {2, 4, 0}, "Orthographic depth dependence");
    equal(ortho.transformPoint({2, 4, 100}), {2, 4, 0}, "Orthographic depth dependence");
    const Mat4 view = Projection::viewMatrix();
    const Mat4 axonometric = ortho * view;
    const double expectedLength = std::sqrt(2.0 / 3.0);
    for (const Point axis : {Point{1, 0, 0}, Point{0, 1, 0}, Point{0, 0, 1}})
        require(std::abs(distance(axonometric.transformPoint(axis), {0, 0, 0}) - expectedLength)
                    < tolerance, "Isometric axis scales differ");

    const auto cube = geometry::makeRegularSolid(RegularSolid::Hexahedron);
    const auto before = cube.vertices();
    for (auto type : {ProjectionType::Axonometric, ProjectionType::Perspective}) {
        const auto edges = Projection::projectEdges(cube, type, 12);
        require(edges.size() == 12, "Cube edges lost or duplicated");
        for (std::size_t i = 0; i < before.size(); ++i)
            equal(cube.vertices()[i], before[i], "Projection changed world coordinates");
    }
    invalid([] { Projection::perspectiveMatrix(0); }, "Zero camera distance accepted");
    invalid([] { Projection::perspectiveMatrix(-1); }, "Negative camera distance accepted");
    invalid([] { Projection::perspectiveMatrix(std::numeric_limits<double>::quiet_NaN()); },
            "NaN camera distance accepted");

    // Construct a triangle in camera coordinates, then move it to world coordinates.
    const Mat4 inverseView = Mat4::rotationY(pi / 4)
                          * Mat4::rotationX(-std::atan(1.0 / std::sqrt(2.0)));
    const geometry::Polyhedron crossing({inverseView.transformPoint({1, 0, 11}),
                                         inverseView.transformPoint({0, 1, 0}),
                                         inverseView.transformPoint({0, -1, 0})}, {{{0, 1, 2}}});
    const auto clipped = Projection::projectEdges(crossing, ProjectionType::Perspective, 10);
    require(clipped.size() == 3, "Near-plane clipping discarded a visible edge");
    for (const auto& edge : clipped) {
        require(std::isfinite(edge.first.x) && std::isfinite(edge.first.y) &&
                std::isfinite(edge.second.x) && std::isfinite(edge.second.y), "Non-finite projection");
        require(std::abs(edge.first.x) < 101 && std::abs(edge.second.x) < 101,
                "Near-plane intersection was not clipped before division");
    }
    const geometry::Polyhedron behind({inverseView.transformPoint({1, 0, 11}),
                                       inverseView.transformPoint({0, 1, 12}),
                                       inverseView.transformPoint({0, -1, 13})}, {{{0, 1, 2}}});
    require(Projection::projectEdges(behind, ProjectionType::Perspective, 10).empty(),
            "Edges behind camera were drawn");
}
} // namespace

int main() {
    try {
        testCenterRotation();
        testLineRotation();
        testProjections();
        std::cout << "Center rotation, arbitrary line, invariants and projections passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
