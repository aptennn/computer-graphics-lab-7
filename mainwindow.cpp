#include "mainwindow.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <stdexcept>

#include "mat4.h"
#include "wireframe_canvas.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

geometry::RegularSolid solidFromIndex(int index) {
    switch (index) {
    case 0: return geometry::RegularSolid::Tetrahedron;
    case 1: return geometry::RegularSolid::Hexahedron;
    case 2: return geometry::RegularSolid::Octahedron;
    case 3: return geometry::RegularSolid::Icosahedron;
    default: return geometry::RegularSolid::Dodecahedron;
    }
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      model_(geometry::makeRegularSolid(geometry::RegularSolid::Tetrahedron)) {
    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);

    auto* panel = new QWidget(central);
    auto* panelLayout = new QVBoxLayout(panel);

    // Выбор многогранника
    auto* solidGroup = new QGroupBox("Многогранник", panel);
    auto* solidForm = new QFormLayout(solidGroup);
    solidBox_ = new QComboBox(solidGroup);
    solidBox_->setObjectName("solidBox");
    solidBox_->addItems({"Тетраэдр", "Гексаэдр (куб)", "Октаэдр",
                         "Икосаэдр", "Додекаэдр"});
    solidForm->addRow("Тип:", solidBox_);
    edgeBox_ = new QDoubleSpinBox(solidGroup);
    edgeBox_->setObjectName("edgeLength");
    edgeBox_->setRange(0.1, 10.0);
    edgeBox_->setValue(edgeLength_);
    edgeBox_->setDecimals(2);
    solidForm->addRow("Длина ребра:", edgeBox_);
    auto* rebuildBtn = new QPushButton("Построить", solidGroup);
    solidForm->addRow(rebuildBtn);
    panelLayout->addWidget(solidGroup);

    // Лекция 9, слайды 39 и 54: выбор матрицы проекции.
    auto* projectionGroup = new QGroupBox("Проекция", panel);
    auto* projectionForm = new QFormLayout(projectionGroup);
    projectionBox_ = new QComboBox(projectionGroup);
    projectionBox_->setObjectName("projectionBox");
    projectionBox_->addItems({"Аксонометрическая", "Перспективная"});
    projectionForm->addRow("Тип:", projectionBox_);
    cameraDistance_ = new QDoubleSpinBox(projectionGroup);
    cameraDistance_->setObjectName("cameraDistance");
    cameraDistance_->setRange(0.5, 1000.0);
    cameraDistance_->setDecimals(2);
    cameraDistance_->setSingleStep(0.5);
    cameraDistance_->setValue(6.0);
    cameraDistance_->setEnabled(false);
    cameraDistance_->setToolTip("Расстояние от центра проекции до плоскости z=0 "
                                "в системе камеры, в единицах длины ребра. "
                                "Если модель за камерой, увеличьте расстояние.");
    projectionForm->addRow("Камера (в рёбрах):", cameraDistance_);
    rotationGuideBox_ = new QComboBox(projectionGroup);
    rotationGuideBox_->setObjectName("rotationGuideBox");
    rotationGuideBox_->addItems({"Скрыть ось", "Выбранная ось X/Y/Z", "Прямая AB"});
    rotationGuideBox_->setCurrentIndex(1);
    rotationGuideBox_->setToolTip("Синяя — выбранная координатная ось; оранжевая — прямая AB. "
                                 "C — центр модели, O — начало координат. "
                                 "Стрелка задаёт положительное направление оси.");
    projectionForm->addRow("Ось вращения:", rotationGuideBox_);
    panelLayout->addWidget(projectionGroup);

    // Смещение
    auto* transGroup = new QGroupBox("Смещение", panel);
    auto* transForm = new QFormLayout(transGroup);
    tx_ = new QDoubleSpinBox(transGroup); tx_->setRange(-100, 100); tx_->setDecimals(2);
    ty_ = new QDoubleSpinBox(transGroup); ty_->setRange(-100, 100); ty_->setDecimals(2);
    tz_ = new QDoubleSpinBox(transGroup); tz_->setRange(-100, 100); tz_->setDecimals(2);
    tx_->setObjectName("translateX");
    ty_->setObjectName("translateY");
    tz_->setObjectName("translateZ");
    transForm->addRow("dx:", tx_);
    transForm->addRow("dy:", ty_);
    transForm->addRow("dz:", tz_);
    auto* transBtn = new QPushButton("Применить", transGroup);
    transForm->addRow(transBtn);
    panelLayout->addWidget(transGroup);

    // Поворот
    auto* rotGroup = new QGroupBox("Поворот", panel);
    auto* rotForm = new QFormLayout(rotGroup);
    rotAxis_ = new QComboBox(rotGroup);
    rotAxis_->setObjectName("rotationAxis");
    rotAxis_->addItems({"X", "Y", "Z"});
    rotForm->addRow("Ось:", rotAxis_);
    rotMode_ = new QComboBox(rotGroup);
    rotMode_->setObjectName("rotationMode");
    rotMode_->addItems({"Через начало координат", "Через центр многогранника"});
    rotForm->addRow("Прямая:", rotMode_);
    rotAngle_ = new QDoubleSpinBox(rotGroup);
    rotAngle_->setObjectName("rotationAngle");
    rotAngle_->setRange(-360, 360);
    rotAngle_->setValue(45);
    rotAngle_->setSuffix("°");
    rotForm->addRow("Угол:", rotAngle_);
    auto* rotBtn = new QPushButton("Применить", rotGroup);
    rotForm->addRow(rotBtn);
    panelLayout->addWidget(rotGroup);

    // Прямая A+t(B-A); положительный угол — по правилу правой руки от A к B.
    auto* lineGroup = new QGroupBox("Поворот вокруг прямой AB", panel);
    auto* lineLayout = new QVBoxLayout(lineGroup);
    auto* coordinates = new QGridLayout();
    const std::array<QString, 3> names{"X", "Y", "Z"};
    coordinates->addWidget(new QLabel("Точка", lineGroup), 0, 0);
    coordinates->addWidget(new QLabel("A", lineGroup), 1, 0);
    coordinates->addWidget(new QLabel("B", lineGroup), 2, 0);
    for (int i = 0; i < 3; ++i) {
        coordinates->addWidget(new QLabel(names[i], lineGroup), 0, i + 1);
        for (int row = 0; row < 2; ++row) {
            auto* input = new QDoubleSpinBox(lineGroup);
            input->setRange(-1000, 1000);
            input->setDecimals(3);
            input->setSingleStep(0.1);
            input->setValue(row == 0 ? 0.0 : 1.0);
            input->setObjectName(QString("line%1%2").arg(row == 0 ? "A" : "B", names[i]));
            (row == 0 ? lineA_ : lineB_)[i] = input;
            coordinates->addWidget(input, row + 1, i + 1);
        }
    }
    lineLayout->addLayout(coordinates);
    auto* lineForm = new QFormLayout();
    lineAngle_ = new QDoubleSpinBox(lineGroup);
    lineAngle_->setObjectName("lineAngle");
    lineAngle_->setRange(-360, 360);
    lineAngle_->setValue(45);
    lineAngle_->setSuffix("°");
    lineAngle_->setToolTip("Положительный угол: правило правой руки, направление оси от A к B.");
    lineForm->addRow("Угол:", lineAngle_);
    lineLayout->addLayout(lineForm);
    auto* lineBtn = new QPushButton("Повернуть вокруг AB", lineGroup);
    lineBtn->setObjectName("rotateAroundLineButton");
    lineLayout->addWidget(lineBtn);
    lineError_ = new QLabel(lineGroup);
    lineError_->setObjectName("lineError");
    lineError_->setWordWrap(true);
    lineError_->setStyleSheet("color: #b3261e;");
    lineError_->hide();
    lineLayout->addWidget(lineError_);
    panelLayout->addWidget(lineGroup);

    // Масштаб
    auto* scaleGroup = new QGroupBox("Масштаб", panel);
    auto* scaleForm = new QFormLayout(scaleGroup);
    sx_ = new QDoubleSpinBox(scaleGroup); sx_->setRange(-10, 10); sx_->setValue(1.0); sx_->setDecimals(2);
    sy_ = new QDoubleSpinBox(scaleGroup); sy_->setRange(-10, 10); sy_->setValue(1.0); sy_->setDecimals(2);
    sz_ = new QDoubleSpinBox(scaleGroup); sz_->setRange(-10, 10); sz_->setValue(1.0); sz_->setDecimals(2);
    scaleForm->addRow("sx:", sx_);
    scaleForm->addRow("sy:", sy_);
    scaleForm->addRow("sz:", sz_);
    auto* scaleBtn = new QPushButton("Относительно начала координат", scaleGroup);
    auto* scaleCenterBtn = new QPushButton("Относительно центра", scaleGroup);
    scaleForm->addRow(scaleBtn);
    scaleForm->addRow(scaleCenterBtn);
    panelLayout->addWidget(scaleGroup);

    // Отражение
    auto* reflGroup = new QGroupBox("Отражение", panel);
    auto* reflForm = new QFormLayout(reflGroup);
    reflectPlane_ = new QComboBox(reflGroup);
    reflectPlane_->addItems({"Плоскость XY (z→-z)", "Плоскость XZ (y→-y)",
                             "Плоскость YZ (x→-x)"});
    reflForm->addRow("Плоскость:", reflectPlane_);
    auto* reflBtn = new QPushButton("Отразить", reflGroup);
    reflForm->addRow(reflBtn);
    panelLayout->addWidget(reflGroup);

    auto* resetBtn = new QPushButton("Сбросить модель", panel);
    panelLayout->addWidget(resetBtn);
    panelLayout->addStretch();

    // Область отрисовки
    canvas_ = new WireframeCanvas(model_, edgeLength_, central);

    auto* panelScroll = new QScrollArea(central);
    panelScroll->setWidgetResizable(true);
    panelScroll->setFrameShape(QFrame::NoFrame);
    panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    panelScroll->setWidget(panel);
    panelScroll->setFixedWidth(panel->minimumSizeHint().width()
                               + panelScroll->verticalScrollBar()->sizeHint().width() + 4);

    layout->addWidget(panelScroll);
    layout->addWidget(canvas_, 1);
    setCentralWidget(central);
    resize(1000, 640);

    // Подключения
    connect(rebuildBtn, &QPushButton::clicked, this, &MainWindow::rebuildModel);
    connect(edgeBox_, &QDoubleSpinBox::valueChanged, this, [this] { rebuildModel(); });
    connect(transBtn,   &QPushButton::clicked, this, &MainWindow::applyTranslate);
    connect(rotBtn,     &QPushButton::clicked, this, &MainWindow::applyRotate);
    connect(lineBtn, &QPushButton::clicked, this, &MainWindow::applyRotateAroundLine);
    connect(projectionBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool perspective = index == 1;
        cameraDistance_->setEnabled(perspective);
        canvas_->setProjectionType(perspective ? geometry::ProjectionType::Perspective
                                               : geometry::ProjectionType::Axonometric);
    });
    connect(cameraDistance_, &QDoubleSpinBox::valueChanged, this, [this](double distance) {
        canvas_->setCameraDistance(distance);
    });
    connect(rotationGuideBox_, &QComboBox::currentIndexChanged, this,
            [this] { updateRotationGuide(); });
    for (auto* box : {rotAxis_, rotMode_}) {
        connect(box, &QComboBox::currentIndexChanged, this, [this] { updateRotationGuide(1); });
    }
    connect(rotAngle_, &QDoubleSpinBox::valueChanged, this, [this] { updateRotationGuide(1); });
    for (int i = 0; i < 3; ++i) {
        for (auto* input : {lineA_[i], lineB_[i]}) {
            connect(input, &QDoubleSpinBox::valueChanged, this, [this] { updateRotationGuide(2); });
        }
    }
    connect(lineAngle_, &QDoubleSpinBox::valueChanged, this, [this] { updateRotationGuide(2); });
    connect(scaleBtn,   &QPushButton::clicked, this, &MainWindow::applyScale);
    connect(scaleCenterBtn, &QPushButton::clicked, this, &MainWindow::applyScaleAroundCenter);
    connect(reflBtn,    &QPushButton::clicked, this, &MainWindow::applyReflect);
    connect(resetBtn,   &QPushButton::clicked, this, &MainWindow::resetModel);

    updateRotationGuide();
    rebuildModel();
}

void MainWindow::updateRotationGuide(int preferredMode) {
    if (preferredMode > 0 && rotationGuideBox_->currentIndex() != 0 &&
        rotationGuideBox_->currentIndex() != preferredMode) {
        rotationGuideBox_->setCurrentIndex(preferredMode);
        return;
    }
    const geometry::Point a{lineA_[0]->value(), lineA_[1]->value(), lineA_[2]->value()};
    const geometry::Point b{lineB_[0]->value(), lineB_[1]->value(), lineB_[2]->value()};
    canvas_->setRotationGuide(static_cast<WireframeCanvas::RotationGuide>(rotationGuideBox_->currentIndex()),
                               rotAxis_->currentIndex(), rotMode_->currentIndex() == 1, a, b);
}

void MainWindow::rebuildModel() {
    edgeLength_ = edgeBox_->value();
    model_ = geometry::makeRegularSolid(solidFromIndex(solidBox_->currentIndex()), edgeLength_);
    canvas_->update();
}

void MainWindow::applyTranslate() {
    model_.translate(tx_->value(), ty_->value(), tz_->value());
    canvas_->update();
}

void MainWindow::applyRotate() {
    updateRotationGuide(1);
    const double rad = rotAngle_->value() * kPi / 180.0;
    const char axis = (rotAxis_->currentIndex() == 0) ? 'x'
                    : (rotAxis_->currentIndex() == 1) ? 'y' : 'z';
    if (rotMode_->currentIndex() == 1) model_.rotateAroundCenter(rad, axis);
    else model_.rotate(rad, axis);
    canvas_->update();
}

void MainWindow::applyRotateAroundLine() {
    updateRotationGuide(2);
    const geometry::Point a{lineA_[0]->value(), lineA_[1]->value(), lineA_[2]->value()};
    const geometry::Point b{lineB_[0]->value(), lineB_[1]->value(), lineB_[2]->value()};
    try {
        model_.rotateAroundLine(a, b, lineAngle_->value() * kPi / 180.0);
        lineError_->hide();
        canvas_->update();
    } catch (const std::invalid_argument&) {
        lineError_->setText("Точки A и B совпадают. Задайте две разные точки прямой.");
        lineError_->show();
    }
}

void MainWindow::applyScale() {
    model_.scale(sx_->value(), sy_->value(), sz_->value());
    canvas_->update();
}

void MainWindow::applyScaleAroundCenter() {
    model_.scaleAroundCenter(sx_->value(), sy_->value(), sz_->value());
    canvas_->update();
}

void MainWindow::applyReflect() {
    switch (reflectPlane_->currentIndex()) {
    case 0: model_.reflect(geometry::Mat4::reflectionXY()); break;
    case 1: model_.reflect(geometry::Mat4::reflectionXZ()); break;
    default: model_.reflect(geometry::Mat4::reflectionYZ()); break;
    }
    canvas_->update();
}

void MainWindow::resetModel() { rebuildModel(); }
