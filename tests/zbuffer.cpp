#include "zbuffer_renderer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

geometry::Polyhedron inCameraSpace(std::vector<geometry::Point> vertices,
                                   std::vector<geometry::Polygon> faces) {
    geometry::Polyhedron model(std::move(vertices), std::move(faces));
    constexpr double pi = 3.14159265358979323846;
    const auto inverse = geometry::Mat4::rotationY(pi / 4.0) *
        geometry::Mat4::rotationX(-std::atan(1.0 / std::sqrt(2.0)));
    model.applyMatrix(inverse);
    return model;
}

geometry::Polyhedron triangle(double z) {
    return inCameraSpace({{-1, -1, z}, {1, -1, z}, {0, 1, z}}, {{{0, 1, 2}}});
}

QImage render(const std::vector<geometry::RasterObject>& objects,
              geometry::ProjectionType type = geometry::ProjectionType::Axonometric) {
    return geometry::ZBufferRenderer::render(objects, type, 4.0, QSize(100, 100), QPointF(50, 50), 20);
}
}

int main() {
    try {
        const auto near = triangle(1);
        const auto far = triangle(-1);
        const QRgb blue = qRgb(40, 100, 220);
        const QRgb red = qRgb(220, 70, 50);
        for (auto type : {geometry::ProjectionType::Axonometric,
                          geometry::ProjectionType::Perspective}) {
            const auto forward = render({{&far, red}, {&near, blue}}, type);
            const auto reverse = render({{&near, blue}, {&far, red}}, type);
            require(forward == reverse, "Object order changed Z-buffer result");
            require(forward.pixel(50, 50) == blue, "Near triangle did not cover far triangle");
            require(forward.pixel(5, 5) == qRgb(255, 255, 255), "Background was overwritten");
        }

        const auto slanted = inCameraSpace(
            {{-1, -1, 1}, {1, -1, -1}, {0, 1, -1}}, {{{0, 1, 2}}});
        const auto middle = triangle(0);
        const auto mixed = render({{&slanted, blue}, {&middle, red}});
        require(mixed == render({{&middle, red}, {&slanted, blue}}),
                "Slanted triangles depend on drawing order");
        require(mixed.pixel(42, 58) == blue && mixed.pixel(58, 58) == red,
                "Depth was not interpolated separately for each pixel");

        const auto concave = inCameraSpace(
            {{-1, -1, 0}, {1, -1, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {-1, 1, 0}},
            {{{0, 1, 2, 3, 4, 5}}});
        const auto concaveFrame = render({{&concave, red}});
        require(concaveFrame.pixel(60, 60) == red, "Concave polygon interior was not filled");
        require(concaveFrame.pixel(60, 40) == qRgb(255, 255, 255),
                "Concave polygon notch was filled");

        const auto behind = triangle(5);
        const auto behindFrame = render({{&behind, red}}, geometry::ProjectionType::Perspective);
        require(behindFrame.pixel(50, 50) == qRgb(255, 255, 255),
                "Triangle behind perspective camera was drawn");
        const auto clipped = inCameraSpace(
            {{-1, -1, 0}, {1, -1, 0}, {0, 1, 5}}, {{{0, 1, 2}}});
        const auto clippedFrame = render({{&clipped, red}}, geometry::ProjectionType::Perspective);
        int visible = 0;
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                if (clippedFrame.pixel(x, y) == red) ++visible;
        require(visible > 0, "Near-plane clipping removed the visible triangle portion");
        std::cout << "Z-buffer order, overlap, concave filling and near clipping passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
