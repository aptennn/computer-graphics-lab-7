#pragma once

#include <QMainWindow>
#include <array>
#include <optional>

#include "geometry.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class WireframeCanvas;

// главное окно — управление моделью и её аффинными преобразованиями.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool loadObjFile(const QString& path);
    bool saveObjFile(const QString& path);

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
    void updateOverlapDemo();
    // Модель
    geometry::Polyhedron model_;
    std::optional<geometry::Polyhedron> importedModel_;
    double edgeLength_ = 2.0;

    // Виджеты
    WireframeCanvas* canvas_ = nullptr;
    QComboBox* solidBox_ = nullptr;
    QDoubleSpinBox* edgeBox_ = nullptr;
    QLabel* objStatus_ = nullptr;
    QComboBox* renderModeBox_ = nullptr;
    QCheckBox* overlapDemoBox_ = nullptr;

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
