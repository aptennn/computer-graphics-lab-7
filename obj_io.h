#pragma once

#include "geometry.h"

#include <iosfwd>

namespace geometry {

// Geometry subset of Wavefront OBJ: vertices and polygonal faces.
// Texture coordinates, normals, groups and materials may be present on import;
// the current Polyhedron stores only positions and vertex indices.
Polyhedron readObj(std::istream& input);
void writeObj(std::ostream& output, const Polyhedron& model);

} // namespace geometry
