#include "projection.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace geometry {
namespace {
std::optional<ProjectedEdge> projectCameraSegment(Point a, Point b, ProjectionType type,
                                                  double cameraDistance, const Mat4& projection) {
    if (type == ProjectionType::Perspective) {
        const double nearZ = cameraDistance * 0.99;
        if (a.z > nearZ && b.z > nearZ) return std::nullopt;
        if ((a.z > nearZ) != (b.z > nearZ)) {
            const double t = (nearZ - a.z) / (b.z - a.z);
            const Point intersection{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), nearZ};
            if (a.z > nearZ) a = intersection;
            else b = intersection;
        }
    }
    return ProjectedEdge{projection.transformPoint(a), projection.transformPoint(b)};
}
} // namespace

Mat4 Projection::viewMatrix() {
    constexpr double pi = 3.14159265358979323846;
    // Равное сокращение по всем трём осям: psi=-45°, phi=35.264...°.
    return Mat4::rotationX(std::atan(1.0 / std::sqrt(2.0)))
         * Mat4::rotationY(-pi / 4.0);
}

Mat4 Projection::axonometricMatrix() {
    Mat4 result = Mat4::identity();
    result.m[2][2] = 0.0;
    return result;
}

Mat4 Projection::perspectiveMatrix(double cameraDistance) {
    if (!std::isfinite(cameraDistance) || cameraDistance <= 0.0) {
        throw std::invalid_argument("Camera distance must be finite and positive");
    }
    Mat4 result = axonometricMatrix();
    result.m[3][2] = -1.0 / cameraDistance;
    return result;
}

std::optional<ProjectedEdge> Projection::projectSegment(const Point& a, const Point& b,
                                                        ProjectionType type, double cameraDistance) {
    const Mat4 view = viewMatrix();
    const Mat4 projection = type == ProjectionType::Perspective
        ? perspectiveMatrix(cameraDistance) : axonometricMatrix();
    return projectCameraSegment(view.transformPoint(a), view.transformPoint(b),
                                type, cameraDistance, projection);
}

std::vector<ProjectedEdge> Projection::projectEdges(const Polyhedron& model,
                                                   ProjectionType type,
                                                   double cameraDistance) {
    const Mat4 view = viewMatrix();
    const bool perspective = type == ProjectionType::Perspective;
    const Mat4 projection = perspective ? perspectiveMatrix(cameraDistance) : axonometricMatrix();
    std::vector<Point> cameraVertices;
    cameraVertices.reserve(model.vertices().size());
    for (const Point& p : model.vertices()) cameraVertices.push_back(view.transformPoint(p));

    std::set<std::pair<std::size_t, std::size_t>> visited;
    std::vector<ProjectedEdge> result;
    for (const Polygon& face : model.faces()) {
        const auto& indices = face.vertexIndices;
        for (std::size_t i = 0; i < indices.size(); ++i) {
            const auto edge = std::minmax(indices[i], indices[(i + 1) % indices.size()]);
            if (!visited.insert(edge).second) continue;
            if (const auto projected = projectCameraSegment(cameraVertices[edge.first],
                    cameraVertices[edge.second], type, cameraDistance, projection)) {
                result.push_back(*projected);
            }
        }
    }
    return result;
}

} // namespace geometry
