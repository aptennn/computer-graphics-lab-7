#include "mainwindow.h"
#include "wireframe_canvas.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Widget>
Widget* child(QWidget& parent, const char* name) {
    auto* widget = parent.findChild<Widget*>(name);
    require(widget != nullptr, name);
    return widget;
}

QImage render(QWidget& widget) {
    QApplication::processEvents();
    QImage image(widget.size(), QImage::Format_RGB32);
    image.fill(Qt::magenta);
    widget.render(&image);
    return image;
}

QImage modelImage(QWidget& canvas) {
    return render(canvas).copy(0, 0, canvas.width(), canvas.height() - 40);
}

QImage guideMask(QWidget& canvas, bool orange) {
    auto image = modelImage(canvas);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            const bool match = orange ? color.red() > 120 && color.green() > 40 &&
                color.green() < 160 && color.blue() < 80 : color.blue() > 120 &&
                color.red() < 100 && color.green() < 160;
            image.setPixelColor(x, y, match ? Qt::black : Qt::white);
        }
    }
    return image;
}

int guidePixels(const QImage& mask) {
    int count = 0;
    for (int y = 0; y < mask.height(); ++y)
        for (int x = 0; x < mask.width(); ++x)
            if (mask.pixelColor(x, y) == Qt::black) ++count;
    return count;
}

int maskDifference(const QImage& first, const QImage& second) {
    int count = 0;
    for (int y = 0; y < first.height(); ++y)
        for (int x = 0; x < first.width(); ++x)
            if (first.pixel(x, y) != second.pixel(x, y)) ++count;
    return count;
}

void checkBounds(QWidget& canvas) {
    const auto image = modelImage(canvas);
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).lightness() < 180) {
                require(x >= 24 && x < canvas.width() - 24 &&
                        y >= 24 && y < canvas.height() - 48, "Wireframe crossed canvas margins");
                ++count;
            }
        }
    }
    require(count > 10, "Wireframe is not visible");
}

void invoke(MainWindow& window, const char* method) {
    require(QMetaObject::invokeMethod(&window, method, Qt::DirectConnection), method);
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        MainWindow window;
        window.show();
        auto* canvas = child<QWidget>(window, "wireframeCanvas");
        auto* solid = child<QComboBox>(window, "solidBox");
        auto* projection = child<QComboBox>(window, "projectionBox");
        auto* camera = child<QDoubleSpinBox>(window, "cameraDistance");
        auto* guide = child<QComboBox>(window, "rotationGuideBox");
        guide->setCurrentIndex(0);
        auto* scroll = window.findChild<QScrollArea*>();
        require(scroll != nullptr, "Missing scroll panel");
        for (QSize size : {QSize(700, 420), QSize(1000, 640), QSize(1400, 900)}) {
            window.resize(size);
            QApplication::processEvents();
            require(!canvas->geometry().intersects(scroll->geometry()), "Canvas overlaps controls");
            for (int i = 0; i < 5; ++i) {
                solid->setCurrentIndex(i);
                invoke(window, "rebuildModel");
                for (int p = 0; p < 2; ++p) {
                    projection->setCurrentIndex(p);
                    checkBounds(*canvas);
                }
            }
        }
        window.resize(1000, 640);
        solid->setCurrentIndex(1);
        invoke(window, "rebuildModel");
        projection->setCurrentIndex(0);
        const auto axonometric = modelImage(*canvas);
        require(!camera->isEnabled(), "Camera controls enabled in axonometry");
        projection->setCurrentIndex(1);
        require(camera->isEnabled(), "Camera controls disabled in perspective");
        const auto perspective = modelImage(*canvas);
        require(perspective != axonometric, "Projection switch did not change the model image");
        camera->setValue(3);
        require(modelImage(*canvas) != perspective, "Camera distance has no effect");
        projection->setCurrentIndex(0);
        require(modelImage(*canvas) == axonometric, "Projection switch changed model coordinates");
        auto* edgeLength = child<QDoubleSpinBox>(window, "edgeLength");
        edgeLength->setValue(3);
        require(modelImage(*canvas) != axonometric, "Edge length has no visible effect");
        edgeLength->setValue(2);
        require(modelImage(*canvas) == axonometric, "Restoring edge length did not restore the model");

        auto* mode = child<QComboBox>(window, "rotationMode");
        auto* axis = child<QComboBox>(window, "rotationAxis");
        child<QDoubleSpinBox>(window, "translateX")->setValue(2);
        for (int i = 0; i < 3; ++i) {
            invoke(window, "rebuildModel");
            invoke(window, "applyTranslate");
            const auto before = modelImage(*canvas);
            mode->setCurrentIndex(1);
            axis->setCurrentIndex(i);
            invoke(window, "applyRotate");
            require(modelImage(*canvas) != before, "Center rotation UI did not rotate");
            checkBounds(*canvas);
        }

        invoke(window, "rebuildModel");
        auto* error = child<QLabel>(window, "lineError");
        auto* button = child<QPushButton>(window, "rotateAroundLineButton");
        for (const char* name : {"lineBX", "lineBY", "lineBZ"})
            child<QDoubleSpinBox>(window, name)->setValue(0);
        const auto beforeInvalid = modelImage(*canvas);
        button->click();
        require(!error->isHidden(), "Coincident axis points did not show an error");
        require(modelImage(*canvas) == beforeInvalid, "Invalid line changed the model");
        child<QDoubleSpinBox>(window, "lineBZ")->setValue(1);
        button->click();
        require(error->isHidden(), "Valid axis did not clear the error");
        require(modelImage(*canvas) != beforeInvalid, "Arbitrary-axis UI did not rotate");
        checkBounds(*canvas);

        // The center guide follows the translated model, but stays fixed during center rotation.
        projection->setCurrentIndex(0);
        invoke(window, "rebuildModel");
        invoke(window, "applyTranslate");
        mode->setCurrentIndex(1);
        axis->setCurrentIndex(1);
        guide->setCurrentIndex(1);
        const auto centerGuide = guideMask(*canvas, false);
        require(guidePixels(centerGuide) > 50, "Coordinate guide is missing");
        invoke(window, "applyRotate");
        // Antialiased intersections with moving black edges can change a few blue pixels.
        require(maskDifference(guideMask(*canvas, false), centerGuide) < guidePixels(centerGuide) / 20 + 5,
                "Center guide moved during center rotation");
        invoke(window, "applyTranslate");
        require(guideMask(*canvas, false) != centerGuide, "Center guide did not follow model translation");
        const auto translatedGuide = guideMask(*canvas, false);
        axis->setCurrentIndex(2);
        require(guideMask(*canvas, false) != translatedGuide, "Axis selection did not update guide");

        invoke(window, "rebuildModel");
        child<QDoubleSpinBox>(window, "lineBX")->setValue(2);
        child<QDoubleSpinBox>(window, "lineBY")->setValue(1);
        child<QDoubleSpinBox>(window, "lineBZ")->setValue(0);
        require(guide->currentIndex() == 2, "Editing AB did not select its guide");
        const auto lineGuide = guideMask(*canvas, true);
        require(guidePixels(lineGuide) > 50, "Arbitrary line guide is missing");
        child<QDoubleSpinBox>(window, "lineBZ")->setValue(1);
        require(guideMask(*canvas, true) != lineGuide, "AB guide did not update with coordinates");
        const auto axonometricGuide = guideMask(*canvas, true);
        projection->setCurrentIndex(1);
        require(guideMask(*canvas, true) != axonometricGuide, "Guide did not switch projection");
        checkBounds(*canvas);
        guide->setCurrentIndex(0);
        child<QDoubleSpinBox>(window, "lineBX")->setValue(1);
        require(guide->currentIndex() == 0, "Editing re-enabled hidden guide");
        require(guidePixels(guideMask(*canvas, true)) == 0, "Hidden AB guide was drawn");
        guide->setCurrentIndex(2);
        for (const char* name : {"lineBX", "lineBY", "lineBZ"})
            child<QDoubleSpinBox>(window, name)->setValue(0);
        require(guidePixels(guideMask(*canvas, true)) == 0, "Coincident points displayed a false line");
        for (const char* name : {"lineBX", "lineBY", "lineBZ"})
            child<QDoubleSpinBox>(window, name)->setValue(1);
        require(guidePixels(guideMask(*canvas, true)) > 10, "View-aligned axis marker is missing");

        // Optional screenshots for visual inspection; normal CTest runs do not write them.
        const QString screenshots = qEnvironmentVariable("LAB6_SCREENSHOT_DIR");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            invoke(window, "rebuildModel");
            projection->setCurrentIndex(0);
            mode->setCurrentIndex(1);
            axis->setCurrentIndex(2);
            guide->setCurrentIndex(1);
            scroll->verticalScrollBar()->setValue(0);
            render(window).save(screenshots + "/axonometric.png");
            invoke(window, "applyTranslate");
            scroll->ensureWidgetVisible(mode);
            render(window).save(screenshots + "/center-axis.png");
            projection->setCurrentIndex(1);
            render(window).save(screenshots + "/perspective.png");
            invoke(window, "rebuildModel");
            child<QDoubleSpinBox>(window, "lineBX")->setValue(2);
            child<QDoubleSpinBox>(window, "lineBY")->setValue(1);
            child<QDoubleSpinBox>(window, "lineBZ")->setValue(0);
            scroll->ensureWidgetVisible(button);
            render(window).save(screenshots + "/line-controls.png");
        }
        std::cout << "30 model/projection/size combinations, projection switching, camera, "
                     "three center axes, arbitrary-axis UI and rotation guides passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
