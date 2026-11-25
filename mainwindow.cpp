#include "mainwindow.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "mat4.h"
#include "obj_io.h"
#include "surface.h"
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

double bowlHeight(double x, double y) {
    return x * x + y * y;
}

double saddleHeight(double x, double y) {
    return x * x - y * y;
}

double rippleHeight(double x, double y) {
    const double r = x * x + y * y + 1.0;
    return 5.0 * (std::cos(r) / r + 0.1);
}

struct SurfacePreset {
    const char* label;
    geometry::HeightFunction height;
};

constexpr SurfacePreset kSurfacePresets[] = {
    {"Чаша: x² + y²", bowlHeight},
    {"Седло: x² − y²", saddleHeight},
    {"Волны из лекции", rippleHeight},
};
} // namespace

// Общее окно проекта. Для участника 3 здесь создаются поля оси/центра,
// точек A и B, угла, проекции и расстояния камеры; connect связывает их с методами ниже.
// Вычисления вынесены в Mat4/Polyhedron/Projection, окно только читает параметры.
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
    // Фигура вращения
    auto* revGroup = new QGroupBox("Фигура вращения", panel);
    auto* revLayout = new QVBoxLayout(revGroup);

    revLayout->addWidget(new QLabel("Образующая (x,y,z; ...):", revGroup));
    revolutionPoints_ = new QLineEdit(revGroup);
    revolutionPoints_->setObjectName("revolutionPoints");
    revolutionPoints_->setPlaceholderText("x,y,z; x,y,z; ...");
    revolutionPoints_->setText("1,0,0; 1,0,1; 0,0,2");
    revolutionPoints_->setToolTip("Точки через ';', координаты через ','.");
    revLayout->addWidget(revolutionPoints_);

    auto* revForm = new QFormLayout();
    revolutionAxis_ = new QComboBox(revGroup);
    revolutionAxis_->setObjectName("revolutionAxis");
    revolutionAxis_->addItems({"X", "Y", "Z"});
    revolutionAxis_->setCurrentIndex(2);
    revForm->addRow("Ось вращения:", revolutionAxis_);

    revolutionSegments_ = new QDoubleSpinBox(revGroup);
    revolutionSegments_->setObjectName("revolutionSegments");
    revolutionSegments_->setRange(3, 360);
    revolutionSegments_->setDecimals(0);
    revolutionSegments_->setValue(24);
    revForm->addRow("Разбиений:", revolutionSegments_);
    revLayout->addLayout(revForm);

    auto* revBtn = new QPushButton("Построить фигуру вращения", revGroup);
    revBtn->setObjectName("buildRevolutionButton");
    revLayout->addWidget(revBtn);

    revolutionStatus_ = new QLabel(revGroup);
    revolutionStatus_->setObjectName("revolutionStatus");
    revolutionStatus_->setWordWrap(true);
    revolutionStatus_->setStyleSheet("color: #b3261e;");
    revLayout->addWidget(revolutionStatus_);

    panelLayout->addWidget(revGroup);

    // Отсечение нелицевых граней — лабораторная №8
    auto* cullGroup = new QGroupBox("Отсечение нелицевых граней", panel);
    auto* cullLayout = new QVBoxLayout(cullGroup);

    cullCheckBox_ = new QCheckBox("Включить отсечение", cullGroup);
    cullCheckBox_->setObjectName("cullBackfaces");
    cullLayout->addWidget(cullCheckBox_);

    auto* viewForm = new QFormLayout();
    viewDirX_ = new QDoubleSpinBox(cullGroup);
    viewDirY_ = new QDoubleSpinBox(cullGroup);
    viewDirZ_ = new QDoubleSpinBox(cullGroup);
    for (auto* box : {viewDirX_, viewDirY_, viewDirZ_}) {
        box->setRange(-10.0, 10.0);
        box->setDecimals(2);
        box->setSingleStep(0.1);
    }
    viewDirX_->setValue(0.0);
    viewDirY_->setValue(0.0);
    viewDirZ_->setValue(1.0);
    viewDirX_->setObjectName("viewDirX");
    viewDirY_->setObjectName("viewDirY");
    viewDirZ_->setObjectName("viewDirZ");
    viewForm->addRow("Обзор X:", viewDirX_);
    viewForm->addRow("Обзор Y:", viewDirY_);
    viewForm->addRow("Обзор Z:", viewDirZ_);
    cullLayout->addLayout(viewForm);

    applyViewDirBtn_ = new QPushButton("Применить вектор обзора", cullGroup);
    applyViewDirBtn_->setObjectName("applyViewDirectionButton");
    cullGroup->setToolTip("В аксонометрии направление задают поля X, Y и Z. "
                          "В перспективе отсечение использует положение камеры.");
    cullLayout->addWidget(applyViewDirBtn_);

    panelLayout->addWidget(cullGroup);

    auto* objGroup = new QGroupBox("Егор — лабораторная №7: загрузка OBJ", panel);
    auto* objLayout = new QVBoxLayout(objGroup);
    auto* loadButton = new QPushButton("Загрузить OBJ…", objGroup);
    loadButton->setObjectName("loadObjButton");
    auto* saveButton = new QPushButton("Сохранить OBJ…", objGroup);
    saveButton->setObjectName("saveObjButton");
    objStatus_ = new QLabel("Сохранение включает текущие преобразования модели.", objGroup);
    objStatus_->setObjectName("objStatus");
    objStatus_->setWordWrap(true);
    objLayout->addWidget(loadButton);
    objLayout->addWidget(saveButton);
    objLayout->addWidget(objStatus_);
    panelLayout->addWidget(objGroup);

    // Михаил (FbGod): параметры графика функции двух переменных.
    auto* surfaceGroup = new QGroupBox("График поверхности", panel);
    auto* surfaceForm = new QFormLayout(surfaceGroup);

    auto* functionBox = new QComboBox(surfaceGroup);
    for (const auto& preset : kSurfacePresets) {
        functionBox->addItem(preset.label);
    }
    surfaceForm->addRow("Функция:", functionBox);

    const auto makeBoundBox = [surfaceGroup](double initialValue) {
        auto* box = new QDoubleSpinBox(surfaceGroup);
        box->setDecimals(2);
        box->setRange(-100.0, 100.0);
        box->setSingleStep(0.25);
        box->setValue(initialValue);
        return box;
    };

    auto* xMin = makeBoundBox(-2.0);
    auto* xMax = makeBoundBox(2.0);
    auto* yMin = makeBoundBox(-2.0);
    auto* yMax = makeBoundBox(2.0);

    surfaceForm->addRow("X от:", xMin);
    surfaceForm->addRow("X до:", xMax);
    surfaceForm->addRow("Y от:", yMin);
    surfaceForm->addRow("Y до:", yMax);

    const auto makeSegmentsBox = [surfaceGroup] {
        auto* box = new QSpinBox(surfaceGroup);
        box->setRange(1, 200);
        box->setValue(16);
        return box;
    };

    auto* xSegments = makeSegmentsBox();
    auto* ySegments = makeSegmentsBox();

    surfaceForm->addRow("Разбиений по X:", xSegments);
    surfaceForm->addRow("Разбиений по Y:", ySegments);

    auto* buildSurfaceBtn = new QPushButton("Построить поверхность", surfaceGroup);
    surfaceForm->addRow(buildSurfaceBtn);

    auto* surfaceError = new QLabel(surfaceGroup);
    surfaceError->setWordWrap(true);
    surfaceError->setStyleSheet("color: #b3261e;");
    surfaceError->hide();
    surfaceForm->addRow(surfaceError);

    panelLayout->addWidget(surfaceGroup);

    auto* zGroup = new QGroupBox("Егор — лабораторная №8: Z-буфер", panel);
    auto* zLayout = new QVBoxLayout(zGroup);
    renderModeBox_ = new QComboBox(zGroup);
    renderModeBox_->setObjectName("renderModeBox");
    renderModeBox_->addItems({"Каркас", "Закраска с Z-буфером"});
    overlapDemoBox_ = new QCheckBox("Показать 3 перекрывающихся объекта", zGroup);
    overlapDemoBox_->setObjectName("overlapDemoBox");
    zLayout->addWidget(renderModeBox_);
    zLayout->addWidget(overlapDemoBox_);
    panelLayout->addWidget(zGroup);

    // Лекция 9, слайды 39 и 54: выбор матрицы проекции.
    auto* projectionGroup = new QGroupBox("Проекция", panel);
    auto* projectionForm = new QFormLayout(projectionGroup);
    projectionBox_ = new QComboBox(projectionGroup);
    projectionBox_->setObjectName("projectionBox");
    projectionBox_->addItems({
    "Аксонометрическая",
    "Перспективная",
    "Перспективная — облёт"
    });
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
    auto* orbitAzimuth = new QDoubleSpinBox(projectionGroup);
    orbitAzimuth->setObjectName("orbitAzimuth");
    orbitAzimuth->setRange(0.0, 360.0);
    orbitAzimuth->setDecimals(0);
    orbitAzimuth->setSingleStep(5.0);
    orbitAzimuth->setValue(35.0);
    orbitAzimuth->setSuffix("°");
    projectionForm->addRow("Азимут камеры:", orbitAzimuth);

    auto* orbitElevation = new QDoubleSpinBox(projectionGroup);
    orbitElevation->setRange(-85.0, 85.0);
    orbitElevation->setDecimals(0);
    orbitElevation->setSingleStep(5.0);
    orbitElevation->setValue(25.0);
    orbitElevation->setSuffix("°");
    projectionForm->addRow("Возвышение камеры:", orbitElevation);
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
    connect(buildSurfaceBtn, &QPushButton::clicked, this,
            [this, functionBox, xMin, xMax, yMin, yMax,
             xSegments, ySegments, surfaceError] {
        const double left = xMin->value();
        const double right = xMax->value();
        const double bottom = yMin->value();
        const double top = yMax->value();

        if (left >= right || bottom >= top) {
            surfaceError->setText(
                "По каждой оси значение «от» должно быть меньше значения «до».");
            surfaceError->show();
            return;
        }

        const auto height = kSurfacePresets[functionBox->currentIndex()].height;

        try {
            model_ = geometry::makeFunctionSurface(
                height, left, right, bottom, top,
                xSegments->value(), ySegments->value());

            // У поверхности нет единой длины ребра.
            // Для настроек холста используем опорную величину 2, как в lab6.
            edgeLength_ = 2.0;
            importedModel_.reset();
            canvas_->setAutoFit(false);
            updateOverlapDemo();
            objStatus_->setText("Построен график поверхности.");
            surfaceError->hide();
            canvas_->update();
        } catch (const std::invalid_argument&) {
            surfaceError->setText("Не удалось построить поверхность с этими параметрами.");
            surfaceError->show();
        }
    });
    connect(loadButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Загрузить модель", {}, "Wavefront OBJ (*.obj)");
        if (!path.isEmpty()) loadObjFile(path);
    });
    connect(saveButton, &QPushButton::clicked, this, [this] {
        QString path = QFileDialog::getSaveFileName(this, "Сохранить модель", {}, "Wavefront OBJ (*.obj)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".obj", Qt::CaseInsensitive)) path += ".obj";
        saveObjFile(path);
    });
    connect(renderModeBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
        canvas_->setFilledMode(index == 1);
    });
    connect(overlapDemoBox_, &QCheckBox::toggled, this, [this] { updateOverlapDemo(); });
    connect(edgeBox_, &QDoubleSpinBox::valueChanged, this, [this] { rebuildModel(); });
    connect(transBtn,   &QPushButton::clicked, this, &MainWindow::applyTranslate);
    connect(rotBtn,     &QPushButton::clicked, this, &MainWindow::applyRotate);
    connect(lineBtn, &QPushButton::clicked, this, &MainWindow::applyRotateAroundLine);
    // Переключение проекции меняет только способ отображения, вершины не трогаем.
    const auto updateCameraControls = [this, orbitAzimuth, orbitElevation] {
        const int index = projectionBox_->currentIndex();
        const bool orbit = index == 2;
        const bool perspective = index == 1 || orbit;

        cameraDistance_->setEnabled(perspective);
        orbitAzimuth->setEnabled(orbit);
        orbitElevation->setEnabled(orbit);
        for (auto* box : {viewDirX_, viewDirY_, viewDirZ_}) {
            box->setEnabled(!perspective);
        }
        applyViewDirBtn_->setEnabled(!perspective);

        canvas_->setProjectionType(perspective
            ? geometry::ProjectionType::Perspective
            : geometry::ProjectionType::Axonometric);

        canvas_->setOrbitCamera(
            orbit,
            orbitAzimuth->value() * kPi / 180.0,
            orbitElevation->value() * kPi / 180.0);
    };

    connect(projectionBox_, &QComboBox::currentIndexChanged, this, updateCameraControls);
    connect(orbitAzimuth, &QDoubleSpinBox::valueChanged, this, updateCameraControls);
    connect(orbitElevation, &QDoubleSpinBox::valueChanged, this, updateCameraControls);

    updateCameraControls();
    // Значение камеры передаётся в единицах длины ребра; в холсте оно станет c.
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
    connect(revBtn, &QPushButton::clicked, this, &MainWindow::buildRevolution);
    connect(cullCheckBox_, &QCheckBox::toggled, this, [this](bool enabled) {
        canvas_->setCullBackfaces(enabled);
    });
    connect(applyViewDirBtn_, &QPushButton::clicked, this, &MainWindow::applyViewDirection);
    connect(resetBtn,   &QPushButton::clicked, this, &MainWindow::resetModel);

    updateRotationGuide();
    rebuildModel();
}

// Участник 3. Синхронизируем наглядную ось с параметрами вращения.
// Режимы: 0 — скрыта, 1 — выбранная X/Y/Z, 2 — прямая AB.
// preferredMode выбирает соответствующую ось при редактировании полей, но сохраняет
// явное «Скрыть ось». Это только подсказка на холсте, модель здесь не вращается.
// Центр передаётся как режим: холст вычислит актуальный C после смещения модели.
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
    importedModel_.reset();
    canvas_->setAutoFit(false);
    edgeLength_ = edgeBox_->value();
    model_ = geometry::makeRegularSolid(solidFromIndex(solidBox_->currentIndex()), edgeLength_);
    updateOverlapDemo();
    objStatus_->setText("Выбран правильный многогранник.");
    canvas_->update();
}

bool MainWindow::loadObjFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        objStatus_->setText("Ошибка загрузки: " + file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFile::NoError) {
        objStatus_->setText("Ошибка загрузки: " + file.errorString());
        return false;
    }
    try {
        std::istringstream input(std::string(bytes.constData(), bytes.size()));
        auto imported = geometry::readObj(input);
        const auto& vertices = imported.vertices();
        double minX = vertices.front().x, maxX = minX;
        double minY = vertices.front().y, maxY = minY;
        double minZ = vertices.front().z, maxZ = minZ;
        for (const auto& point : vertices) {
            minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
            minZ = std::min(minZ, point.z); maxZ = std::max(maxZ, point.z);
        }
        edgeLength_ = std::max({maxX - minX, maxY - minY, maxZ - minZ, 0.01});
        importedModel_ = imported;
        model_ = std::move(imported);
        canvas_->setAutoFit(true);
        updateOverlapDemo();
        objStatus_->setText(QString("Загружено: %1 (%2 вершин, %3 граней)")
                                .arg(QFileInfo(file).fileName())
                                .arg(model_.vertices().size()).arg(model_.faces().size()));
        canvas_->update();
        return true;
    } catch (const std::exception& error) {
        objStatus_->setText("Ошибка OBJ: " + QString::fromUtf8(error.what()));
        return false;
    }
}

bool MainWindow::saveObjFile(const QString& path) {
    try {
        std::ostringstream output;
        geometry::writeObj(output, model_);
        const std::string content = output.str();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(content.data(), content.size()) != content.size() ||
            !file.commit()) {
            objStatus_->setText("Ошибка сохранения: " + file.errorString());
            return false;
        }
        objStatus_->setText("Сохранено: " + QFileInfo(path).fileName());
        return true;
    } catch (const std::exception& error) {
        objStatus_->setText("Ошибка сохранения: " + QString::fromUtf8(error.what()));
        return false;
    }
}

void MainWindow::applyTranslate() {
    model_.translate(tx_->value(), ty_->value(), tz_->value());
    canvas_->update();
}

// Участник 3. Обработчик кнопки поворота вокруг выбранной координатной оси.
// Переводим градусы в радианы: rad=degrees*pi/180; выбираем X/Y/Z.
// Через O — обычная мировая ось; через C — параллельная ей прямая через центр модели.
// После изменения вершин update() запрашивает paintEvent, где применится проекция.
void MainWindow::applyRotate() {
    updateRotationGuide(1);
    const double rad = rotAngle_->value() * kPi / 180.0;
    const char axis = (rotAxis_->currentIndex() == 0) ? 'x'
                    : (rotAxis_->currentIndex() == 1) ? 'y' : 'z';
    if (rotMode_->currentIndex() == 1) model_.rotateAroundCenter(rad, axis);
    else model_.rotate(rad, axis);
    canvas_->update();
}

// Участник 3. Считываем A/B в мировых координатах и угол в градусах из интерфейса.
// Polyhedron строит матрицу AB и применяет её ко всем вершинам; окно матрицы не считает.
// Ошибка совпадающих точек показывается под кнопкой; модель при ней не изменяется.
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

void MainWindow::resetModel() {
    if (importedModel_) {
        model_ = *importedModel_;
        objStatus_->setText("Загруженная модель восстановлена.");
        canvas_->update();
    } else {
        rebuildModel();
    }
}

void MainWindow::updateOverlapDemo() {
    if (!overlapDemoBox_ || !overlapDemoBox_->isChecked()) {
        canvas_->setAdditionalModels({});
        return;
    }
    const auto center = model_.center();
    auto cube = geometry::makeRegularSolid(geometry::RegularSolid::Hexahedron, edgeLength_ * 0.85);
    cube.translate(center.x + edgeLength_ * 0.85,
                   center.y + edgeLength_ * 0.15,
                   center.z - edgeLength_ * 0.55);
    auto octahedron = geometry::makeRegularSolid(geometry::RegularSolid::Octahedron,
                                                  edgeLength_ * 0.95);
    octahedron.translate(center.x - edgeLength_ * 0.65,
                         center.y - edgeLength_ * 0.15,
                         center.z + edgeLength_ * 0.65);
    std::vector<geometry::Polyhedron> extras;
    extras.push_back(std::move(cube));
    extras.push_back(std::move(octahedron));
    canvas_->setAdditionalModels(std::move(extras));
}

void MainWindow::buildRevolution() {
    revolutionStatus_->clear();

    std::vector<geometry::Point> generatrix;
    const QStringList chunks = revolutionPoints_->text().split(';', Qt::SkipEmptyParts);
    for (const QString& chunk : chunks) {
        const QStringList parts = chunk.split(',', Qt::SkipEmptyParts);
        if (parts.size() != 3) {
            revolutionStatus_->setText("Точка должна содержать ровно 3 координаты.");
            return;
        }
        bool okX = false, okY = false, okZ = false;
        const double x = parts[0].trimmed().toDouble(&okX);
        const double y = parts[1].trimmed().toDouble(&okY);
        const double z = parts[2].trimmed().toDouble(&okZ);
        if (!okX || !okY || !okZ) {
            revolutionStatus_->setText("Не удалось разобрать координаты точки.");
            return;
        }
        generatrix.push_back({x, y, z});
    }
    if (generatrix.size() < 2) {
        revolutionStatus_->setText("Нужно минимум 2 точки образующей.");
        return;
    }

    const char axis = (revolutionAxis_->currentIndex() == 0) ? 'x'
                    : (revolutionAxis_->currentIndex() == 1) ? 'y' : 'z';
    const int segments = static_cast<int>(revolutionSegments_->value());

    try {
        importedModel_.reset();
        model_ = geometry::makeSolidOfRevolution(generatrix, axis, segments);
        canvas_->setAutoFit(true);
        updateOverlapDemo();
        objStatus_->setText(QString("Построена фигура вращения (%1 вершин, %2 граней)")
                                .arg(model_.vertices().size()).arg(model_.faces().size()));
        canvas_->update();
    } catch (const std::exception& error) {
        revolutionStatus_->setText(QString("Ошибка: %1").arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::applyViewDirection() {
    canvas_->setViewDirection({viewDirX_->value(), viewDirY_->value(), viewDirZ_->value()});
}
