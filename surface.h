//
// Created by Michael on 24.11.2026.
//

// Михаил
#ifndef COMPUTER_GRAPHICS6_SURFACE_H
#define COMPUTER_GRAPHICS6_SURFACE_H
#include "geometry.h"

namespace geometry {

    // Формула высоты принимает x и y и возвращает z.
    using HeightFunction = double (*)(double x, double y);

    Polyhedron makeFunctionSurface(HeightFunction height,
                                   double xMin, double xMax,
                                   double yMin, double yMax,
                                   int xSegments, int ySegments);

} // namespace geometry
#endif //COMPUTER_GRAPHICS6_SURFACE_H
