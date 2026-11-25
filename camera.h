//
// Created by Michael on 25.11.2026.
//

#ifndef COMPUTER_GRAPHICS6_CAMERA_H
#define COMPUTER_GRAPHICS6_CAMERA_H
#include "geometry.h"

namespace geometry {

    // Михаил (FbGod), lab8: перспективная камера с облётом точки target.
    class OrbitCamera {
    public:
        OrbitCamera(Point target, double radius,
                    double azimuthRad, double elevationRad);

        Point position() const;
        Point direction() const;
        Mat4 viewMatrix() const;
        Mat4 projectionMatrix() const;

    private:
        Point backwardAxis() const;

        Point target_;
        double radius_;
        double azimuthRad_;
        double elevationRad_;
    };

} // namespace geometry
#endif //COMPUTER_GRAPHICS6_CAMERA_H
