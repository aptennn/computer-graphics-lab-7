#pragma once

#include <QMainWindow>
#include <array>

#include "geometry.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class WireframeCanvas;

// главное окно — управление моделью и её аффинными преобразованиями.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void rebuildModel();     
    void applyTranslate();
    void applyRotate();
    void applyRotateAroundLine();
    void applyScale();
    void applyScaleAroundCenter();
    void applyReflect();
    void resetModel();

private:
    void updateRotationGuide(int preferredMode = -1);
    // Модель
    geometry::Polyhedron model_;
    double edgeLength_ = 2.0;

    // Виджеты
    WireframeCanvas* canvas_ = nullptr;
    QComboBox* solidBox_ = nullptr;
    QDoubleSpinBox* edgeBox_ = nullptr;

    QDoubleSpinBox* tx_ = nullptr; QDoubleSpinBox* ty_ = nullptr; QDoubleSpinBox* tz_ = nullptr;

    QDoubleSpinBox* rotAngle_ = nullptr;
    QComboBox* rotAxis_ = nullptr;
    QComboBox* rotMode_ = nullptr;

    std::array<QDoubleSpinBox*, 3> lineA_{};
    std::array<QDoubleSpinBox*, 3> lineB_{};
    QDoubleSpinBox* lineAngle_ = nullptr;
    QLabel* lineError_ = nullptr;

    QComboBox* projectionBox_ = nullptr;
    QDoubleSpinBox* cameraDistance_ = nullptr;
    QComboBox* rotationGuideBox_ = nullptr;

    QDoubleSpinBox* sx_ = nullptr; QDoubleSpinBox* sy_ = nullptr; QDoubleSpinBox* sz_ = nullptr;

    QComboBox* reflectPlane_ = nullptr;
};
