#include "obj_io.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void rejects(const std::string& data) {
    std::istringstream input(data);
    try { (void)geometry::readObj(input); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid OBJ was accepted");
}
}

int main() {
    try {
        std::istringstream input(
            "# square with metadata and all common face forms\n"
            "v 0 0 0\n v 2 0 0\n v 2 2 0\n v 0 2 0\n"
            "vt 0 0\n vn 0 0 1\n o Square\n"
            "f 1/1/1 2/1/1 3/1/1 4/1/1\n"
            "f -4//1 -3//1 -2//1\n");
        auto model = geometry::readObj(input);
        require(model.vertices().size() == 4 && model.faces().size() == 2,
                "OBJ geometry count is wrong");
        require(model.faces()[0].vertexIndices == std::vector<std::size_t>({0, 1, 2, 3}),
                "Positive face indices are wrong");
        require(model.faces()[1].vertexIndices == std::vector<std::size_t>({0, 1, 2}),
                "Negative face indices are wrong");
        model.translate(1, -2, 3);
        std::ostringstream output;
        geometry::writeObj(output, model);
        std::istringstream saved(output.str());
        const auto restored = geometry::readObj(saved);
        require(restored.faces()[0].vertexIndices == model.faces()[0].vertexIndices,
                "Saved polygon was changed");
        require(restored.vertices()[0].x == 1 && restored.vertices()[0].y == -2 &&
                restored.vertices()[0].z == 3, "Saved transformation was lost");
        std::istringstream homogeneous("v 2 4 6 2\nv 0 0 0\nv 1 0 0\nf 1 2 3\n");
        require(geometry::readObj(homogeneous).vertices()[0].x == 1,
                "Homogeneous coordinate was ignored");
        rejects("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n");
        rejects("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 1 3\n");
        rejects("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -4 -2 -1\n");
        rejects("v nan 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        for (auto solid : {geometry::RegularSolid::Tetrahedron, geometry::RegularSolid::Hexahedron,
                           geometry::RegularSolid::Octahedron, geometry::RegularSolid::Icosahedron,
                           geometry::RegularSolid::Dodecahedron}) {
            const auto original = geometry::makeRegularSolid(solid);
            std::ostringstream text;
            geometry::writeObj(text, original);
            std::istringstream copy(text.str());
            const auto result = geometry::readObj(copy);
            require(result.vertices().size() == original.vertices().size() &&
                    result.faces().size() == original.faces().size(),
                    "Regular solid OBJ round trip changed topology");
        }
        std::cout << "OBJ import, export, transformation and invalid input passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
