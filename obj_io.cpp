#include "obj_io.h"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace geometry {
namespace {

[[noreturn]] void invalid(std::size_t line, const char* message) {
    throw std::runtime_error("OBJ line " + std::to_string(line) + ": " + message);
}

long long parseIndex(std::string_view token, std::size_t line) {
    long long value = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (token.empty() || error != std::errc{} || end != token.data() + token.size() || value == 0)
        invalid(line, "invalid face index");
    return value;
}

std::size_t resolveIndex(std::string_view token, std::size_t count, std::size_t line) {
    const long long index = parseIndex(token, line);
    if (index > 0) {
        if (static_cast<unsigned long long>(index) > count)
            invalid(line, "face references a missing vertex");
        return static_cast<std::size_t>(index - 1);
    }
    // Negative OBJ indices refer to vertices already seen at this face.
    if (static_cast<unsigned long long>(-(index + 1)) >= count)
        invalid(line, "face references a missing vertex");
    return count - static_cast<std::size_t>(-(index + 1)) - 1;
}

double parseNumber(const std::string& token, std::size_t line) {
    std::size_t used = 0;
    double value = 0;
    try {
        value = std::stod(token, &used);
    } catch (const std::exception&) {
        invalid(line, "invalid vertex coordinate");
    }
    if (used != token.size() || !std::isfinite(value)) invalid(line, "invalid vertex coordinate");
    return value;
}

} // namespace

Polyhedron readObj(std::istream& input) {
    std::vector<Point> vertices;
    std::vector<Polygon> faces;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.resize(comment);
        std::istringstream fields(line);
        std::string tag;
        if (!(fields >> tag)) continue;
        if (tag == "v") {
            std::vector<double> coordinates;
            std::string token;
            while (fields >> token) coordinates.push_back(parseNumber(token, lineNumber));
            if (coordinates.size() < 3 || coordinates.size() > 4)
                invalid(lineNumber, "vertex needs x y z and optional w");
            const double w = coordinates.size() == 4 ? coordinates[3] : 1.0;
            if (w == 0.0) invalid(lineNumber, "vertex w must be nonzero");
            const Point point{coordinates[0] / w, coordinates[1] / w, coordinates[2] / w};
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                invalid(lineNumber, "vertex coordinate is out of range");
            vertices.push_back(point);
        } else if (tag == "f") {
            Polygon face;
            std::string token;
            while (fields >> token) {
                const auto slash = token.find('/');
                const std::string_view vertexIndex = std::string_view(token).substr(0, slash);
                face.vertexIndices.push_back(resolveIndex(vertexIndex, vertices.size(), lineNumber));
            }
            if (face.vertexIndices.size() < 3) invalid(lineNumber, "face needs at least three vertices");
            faces.push_back(std::move(face));
        }
        // Other OBJ records describe data not represented by Polyhedron.
    }
    if (input.bad()) throw std::runtime_error("Could not read OBJ data");
    if (vertices.empty() || faces.empty())
        throw std::runtime_error("OBJ needs vertices and faces");
    try {
        return {std::move(vertices), std::move(faces)};
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error("Invalid OBJ geometry: " + std::string(error.what()));
    }
}

void writeObj(std::ostream& output, const Polyhedron& model) {
    output << "# Wavefront OBJ exported by computer_graphics7\n" << std::setprecision(17);
    for (const Point& vertex : model.vertices())
        output << "v " << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
    for (const Polygon& face : model.faces()) {
        output << 'f';
        for (std::size_t index : face.vertexIndices) output << ' ' << index + 1;
        output << '\n';
    }
    if (!output) throw std::runtime_error("Could not write OBJ data");
}

} // namespace geometry
