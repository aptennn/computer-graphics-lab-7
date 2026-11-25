#include "zbuffer_renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace geometry {
namespace {

struct Vertex {
    double x, y;       // screen coordinates
    double cameraZ;    // camera space z, larger is nearer
    double invW;       // reciprocal homogeneous w for perspective correction
};

double cross(const Vertex& a, const Vertex& b, const Vertex& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

std::vector<Point> clipNearPlane(const std::vector<Point>& polygon, double nearZ) {
    std::vector<Point> result;
    if (polygon.empty()) return result;
    Point previous = polygon.back();
    bool previousInside = previous.z <= nearZ;
    for (const Point& current : polygon) {
        const bool inside = current.z <= nearZ;
        if (inside != previousInside) {
            const double t = (nearZ - previous.z) / (current.z - previous.z);
            result.push_back({previous.x + t * (current.x - previous.x),
                              previous.y + t * (current.y - previous.y), nearZ});
        }
        if (inside) result.push_back(current);
        previous = current;
        previousInside = inside;
    }
    return result;
}

void drawTriangle(const Vertex& a, const Vertex& b, const Vertex& c, QRgb color,
                  QImage& image, std::vector<double>& zbuffer, bool perspective) {
    const double area = cross(a, b, c);
    if (!std::isfinite(area) || std::abs(area) < 1e-12) return;
    const double left = std::min({a.x, b.x, c.x}) - 0.5;
    const double right = std::max({a.x, b.x, c.x}) - 0.5;
    const double top = std::min({a.y, b.y, c.y}) - 0.5;
    const double bottom = std::max({a.y, b.y, c.y}) - 0.5;
    if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(top) ||
        !std::isfinite(bottom) || right < 0 || bottom < 0 ||
        left >= image.width() || top >= image.height()) return;
    const int minX = static_cast<int>(std::ceil(std::max(0.0, left)));
    const int maxX = static_cast<int>(std::floor(std::min(double(image.width() - 1), right)));
    const int minY = static_cast<int>(std::ceil(std::max(0.0, top)));
    const int maxY = static_cast<int>(std::floor(std::min(double(image.height() - 1), bottom)));
    if (minX > maxX || minY > maxY) return;

    for (int y = minY; y <= maxY; ++y) {
        auto* pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = minX; x <= maxX; ++x) {
            const Vertex pixel{double(x) + 0.5, double(y) + 0.5, 0, 0};
            const double u = cross(b, c, pixel) / area;
            const double v = cross(c, a, pixel) / area;
            const double w = 1.0 - u - v;
            if (u < -1e-10 || v < -1e-10 || w < -1e-10) continue;
            const double reciprocalW = u * a.invW + v * b.invW + w * c.invW;
            if (reciprocalW <= 0.0) continue;
            // z/w and 1/w are linear in screen space; their ratio restores camera z.
            const double z = perspective
                ? (u * a.cameraZ * a.invW + v * b.cameraZ * b.invW +
                   w * c.cameraZ * c.invW) / reciprocalW
                : u * a.cameraZ + v * b.cameraZ + w * c.cameraZ;
            const double depth = -z; // near camera means larger z and smaller stored depth
            const std::size_t index = static_cast<std::size_t>(y) * image.width() + x;
            if (depth < zbuffer[index]) {
                zbuffer[index] = depth;
                pixels[x] = color;
            }
        }
    }
}

// Ear clipping keeps concave OBJ polygons correct after projection.
void drawPolygon(const std::vector<Vertex>& vertices, QRgb color, QImage& image,
                 std::vector<double>& zbuffer, bool perspective) {
    if (vertices.size() < 3) return;
    double signedArea = 0;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto& a = vertices[i];
        const auto& b = vertices[(i + 1) % vertices.size()];
        signedArea += a.x * b.y - b.x * a.y;
    }
    if (std::abs(signedArea) < 1e-12) return;
    const double direction = signedArea > 0 ? 1.0 : -1.0;
    std::vector<std::size_t> remaining(vertices.size());
    for (std::size_t i = 0; i < remaining.size(); ++i) remaining[i] = i;
    while (remaining.size() > 3) {
        bool found = false;
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            const std::size_t p = remaining[(i + remaining.size() - 1) % remaining.size()];
            const std::size_t q = remaining[i];
            const std::size_t r = remaining[(i + 1) % remaining.size()];
            if (direction * cross(vertices[p], vertices[q], vertices[r]) <= 1e-12) continue;
            bool containsVertex = false;
            for (std::size_t candidate : remaining) {
                if (candidate == p || candidate == q || candidate == r) continue;
                const auto& point = vertices[candidate];
                if (direction * cross(vertices[p], vertices[q], point) >= -1e-12 &&
                    direction * cross(vertices[q], vertices[r], point) >= -1e-12 &&
                    direction * cross(vertices[r], vertices[p], point) >= -1e-12) {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex) continue;
            drawTriangle(vertices[p], vertices[q], vertices[r], color, image, zbuffer, perspective);
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            found = true;
            break;
        }
        if (!found) return; // non-simple or edge-on polygon
    }
    drawTriangle(vertices[remaining[0]], vertices[remaining[1]], vertices[remaining[2]],
                 color, image, zbuffer, perspective);
}

} // namespace

QImage ZBufferRenderer::render(const std::vector<RasterObject>& objects,
                                   ProjectionType type, double cameraDistance,
                                   QSize size, QPointF origin, double scale) {
    return render(objects, Projection::fixedView(type, cameraDistance),
                  size, origin, scale);
}

QImage ZBufferRenderer::render(const std::vector<RasterObject>& objects,
                                   const ProjectionView& frame,
                                   QSize size, QPointF origin, double scale, bool cullBackfaces) {
    if (size.width() <= 0 || size.height() <= 0) return {};

    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);
    std::vector<double> zbuffer(
        static_cast<std::size_t>(size.width()) * size.height(),
        std::numeric_limits<double>::infinity());

    const Mat4& view = frame.view;
    const Mat4& projection = frame.projection;
    const bool perspective = frame.type == ProjectionType::Perspective;
    const double cameraDistance = frame.cameraDistance;
    for (const RasterObject& object : objects) {
        if (!object.model) continue;
        std::vector<Point> cameraVertices;
        cameraVertices.reserve(object.model->vertices().size());
        for (const Point& vertex : object.model->vertices())
            cameraVertices.push_back(view.transformPoint(vertex));
        for (const Polygon& face : object.model->faces()) {
            if (cullBackfaces && !Projection::isFrontFacing(
                    view.transformVector(faceNormal(*object.model, face)),
                    cameraVertices[face.vertexIndices.front()], frame)) {
                continue;
            }
            std::vector<Point> polygon;
            polygon.reserve(face.vertexIndices.size());
            for (std::size_t index : face.vertexIndices) polygon.push_back(cameraVertices[index]);
            if (perspective) polygon = clipNearPlane(polygon, 0.99 * cameraDistance);
            if (polygon.size() < 3) continue;
            std::vector<Vertex> projected;
            projected.reserve(polygon.size());
            for (const Point& point : polygon) {
                const Point screen = projection.transformPoint(point);
                projected.push_back({origin.x() + screen.x * scale,
                                     origin.y() - screen.y * scale,
                                     point.z, perspective ? 1.0 / (1.0 - point.z / cameraDistance) : 1.0});
            }
            if (std::any_of(projected.begin(), projected.end(), [](const Vertex& vertex) {
                    return !std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
                           !std::isfinite(vertex.invW);
                })) continue;
            drawPolygon(projected, object.color, image, zbuffer, perspective);
        }
    }
    return image;
}

} // namespace geometry
