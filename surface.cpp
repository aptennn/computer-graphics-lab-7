#include "surface.h"

#include <cmath>
#include <stdexcept>
#include <utility>

// Михаил
namespace geometry {

Polyhedron makeFunctionSurface(HeightFunction height,
                               double xMin, double xMax,
                               double yMin, double yMax,
                               int xSegments, int ySegments) {
    constexpr int maxSegments = 200;

    if (height == nullptr) {
        throw std::invalid_argument("Choose a height function");
    }

    if (!std::isfinite(xMin) || !std::isfinite(xMax) ||
        !std::isfinite(yMin) || !std::isfinite(yMax) ||
        xMin >= xMax || yMin >= yMax) {
        throw std::invalid_argument("Each range needs finite, increasing bounds");
    }

    if (xSegments < 1 || xSegments > maxSegments ||
        ySegments < 1 || ySegments > maxSegments) {
        throw std::invalid_argument("Use 1 to 200 segments per axis");
    }

    const std::size_t columns = static_cast<std::size_t>(xSegments) + 1;
    const std::size_t rows = static_cast<std::size_t>(ySegments) + 1;

    std::vector<Point> vertices;
    std::vector<Polygon> faces;
    vertices.reserve(rows * columns);
    faces.reserve(2 * (rows - 1) * (columns - 1));

    // Сначала вычисляем вершины по рядам.
    for (std::size_t row = 0; row < rows; ++row) {
        const double yFraction = static_cast<double>(row) / ySegments;
        const double y = std::lerp(yMin, yMax, yFraction);

        for (std::size_t column = 0; column < columns; ++column) {
            const double xFraction = static_cast<double>(column) / xSegments;
            const double x = std::lerp(xMin, xMax, xFraction);
            const double z = height(x, y);

            if (!std::isfinite(z)) {
                throw std::invalid_argument("The function is undefined at a grid point");
            }

            vertices.push_back({x, y, z});
        }
    }

    // Каждая клетка даёт два треугольника.
    for (std::size_t row = 0; row + 1 < rows; ++row) {
        for (std::size_t column = 0; column + 1 < columns; ++column) {
            const std::size_t lowerLeft = row * columns + column;
            const std::size_t lowerRight = lowerLeft + 1;
            const std::size_t upperLeft = lowerLeft + columns;
            const std::size_t upperRight = upperLeft + 1;

            faces.push_back(Polygon{{lowerLeft, lowerRight, upperRight}});
            faces.push_back(Polygon{{lowerLeft, upperRight, upperLeft}});
        }
    }

    return Polyhedron(std::move(vertices), std::move(faces));
}

} // namespace geometry