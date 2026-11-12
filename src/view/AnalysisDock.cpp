#include "AnalysisDock.h"
#include "../viewmodel/ViewerViewModel.h"

#include <DockManager.h>
#include <DockWidget.h>

#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace sview {

namespace {
constexpr double kExposureScale = 100.0;   // 1 EV = 100 slider ticks
constexpr double kGammaScale = 100.0;      // 1.0 Gamma = 100 slider ticks
constexpr int kMaxWipeTicks = 1000;        // 0.0 to 1.0 wipe pos = 0 to 1000 ticks
}  // namespace

AnalysisDock::AnalysisDock(ViewerViewModel* vm, QWidget* parent)
    : QWidget(parent)
    , m_vm(vm)
{
    setupUi();
    setupConnections();
    refreshFromViewModel();
}

ads::CDockWidget* AnalysisDock::createDockWidget(ads::CDockManager* dockManager,
                                                 ViewerViewModel* vm,
                                                 QWidget* parent)
{
    auto* dockContent = new AnalysisDock(vm, parent);
    auto* dockWidget = new ads::CDockWidget(dockManager, tr("Image Controls"));
    dockWidget->setObjectName(QStringLiteral("AnalysisDockWidget"));
    dockWidget->setWidget(dockContent);
    dockWidget->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);
    return dockWidget;
}

void AnalysisDock::setupUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* scrollContent = new QWidget(scrollArea);
    auto* rootLayout = new QVBoxLayout(scrollContent);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);
    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);

    // =========================================================================
    // 1. Color & Tone (Exposure & Gamma)
    // =========================================================================
    auto* toneGroup = new QGroupBox(tr("Color & Tone"), this);
    auto* toneLayout = new QGridLayout(toneGroup);
    toneLayout->setContentsMargins(8, 12, 8, 8);
    toneLayout->setSpacing(6);

    // Exposure Controls (-10.0 to +10.0 EV, default 0.0, step 0.1)
    auto* expLabel = new QLabel(tr("Exposure:"), toneGroup);
    m_exposureSlider = new QSlider(Qt::Horizontal, toneGroup);
    m_exposureSlider->setRange(-1000, 1000); // -10.00 to +10.00 EV
    m_exposureSlider->setSingleStep(10);     // 0.10 EV
    m_exposureSlider->setPageStep(100);      // 1.00 EV
    m_exposureSlider->setValue(0);

    m_exposureSpin = new QDoubleSpinBox(toneGroup);
    m_exposureSpin->setRange(-10.0, 10.0);
    m_exposureSpin->setSingleStep(0.1);
    m_exposureSpin->setDecimals(2);
    m_exposureSpin->setValue(0.0);
    m_exposureSpin->setSuffix(QStringLiteral(" EV"));
    m_exposureSpin->setFixedWidth(82);

    m_exposureResetBtn = new QPushButton(tr("Reset"), toneGroup);
    m_exposureResetBtn->setToolTip(tr("Reset exposure to 0.0 EV"));
    m_exposureResetBtn->setFixedWidth(50);

    toneLayout->addWidget(expLabel, 0, 0);
    toneLayout->addWidget(m_exposureSlider, 0, 1);
    toneLayout->addWidget(m_exposureSpin, 0, 2);
    toneLayout->addWidget(m_exposureResetBtn, 0, 3);

    // Gamma Controls (0.2 to 5.0, default 1.0, step 0.05)
    auto* gammaLabel = new QLabel(tr("Gamma:"), toneGroup);
    m_gammaSlider = new QSlider(Qt::Horizontal, toneGroup);
    m_gammaSlider->setRange(20, 500);        // 0.20 to 5.00
    m_gammaSlider->setSingleStep(5);         // 0.05
    m_gammaSlider->setPageStep(25);          // 0.25
    m_gammaSlider->setValue(100);            // 1.00

    m_gammaSpin = new QDoubleSpinBox(toneGroup);
    m_gammaSpin->setRange(0.2, 5.0);
    m_gammaSpin->setSingleStep(0.05);
    m_gammaSpin->setDecimals(2);
    m_gammaSpin->setValue(1.0);
    m_gammaSpin->setFixedWidth(82);

    m_gammaResetBtn = new QPushButton(tr("Reset"), toneGroup);
    m_gammaResetBtn->setToolTip(tr("Reset display gamma to 1.0"));
    m_gammaResetBtn->setFixedWidth(50);

    toneLayout->addWidget(gammaLabel, 1, 0);
    toneLayout->addWidget(m_gammaSlider, 1, 1);
    toneLayout->addWidget(m_gammaSpin, 1, 2);
    toneLayout->addWidget(m_gammaResetBtn, 1, 3);

    // Color Space (OCIO)
    auto* ocioLabel = new QLabel(tr("Color Space:"), toneGroup);
    m_ocioCombo = new QComboBox(toneGroup);
    m_ocioCombo->setObjectName(QStringLiteral("analysisOcioCombo"));
    m_ocioCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toneLayout->addWidget(ocioLabel, 2, 0);
    toneLayout->addWidget(m_ocioCombo, 2, 1, 1, 3);

    rootLayout->addWidget(toneGroup);

    // =========================================================================
    // 2. Channels & AOVs
    // =========================================================================
    auto* channelGroup = new QGroupBox(tr("Channels & AOVs"), this);
    auto* channelLayout = new QHBoxLayout(channelGroup);
    channelLayout->setContentsMargins(8, 12, 8, 8);
    channelLayout->setSpacing(8);

    auto* aovLabel = new QLabel(tr("Layer / AOV:"), channelGroup);
    m_aovCombo = new QComboBox(channelGroup);
    m_aovCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    channelLayout->addWidget(aovLabel);
    channelLayout->addWidget(m_aovCombo);

    rootLayout->addWidget(channelGroup);

    // =========================================================================
    // 3. A/B Wipe Comparison Tool
    // =========================================================================
    auto* wipeGroup = new QGroupBox(tr("A/B Wipe Comparison"), this);
    auto* wipeLayout = new QVBoxLayout(wipeGroup);
    wipeLayout->setContentsMargins(8, 12, 8, 8);
    wipeLayout->setSpacing(8);

    auto* wipeRow1 = new QHBoxLayout();
    m_wipeToggleBtn = new QPushButton(tr("Enable A/B Wipe"), wipeGroup);
    m_wipeToggleBtn->setCheckable(true);
    m_wipeToggleBtn->setChecked(false);

    m_wipeModeCombo = new QComboBox(wipeGroup);
    m_wipeModeCombo->addItem(tr("Vertical Split"), 0);
    m_wipeModeCombo->addItem(tr("Horizontal Split"), 1);
    m_wipeModeCombo->addItem(tr("Onion Skin (Blend)"), 2);

    wipeRow1->addWidget(m_wipeToggleBtn);
    wipeRow1->addWidget(m_wipeModeCombo);
    wipeLayout->addLayout(wipeRow1);

    auto* wipeRow2 = new QHBoxLayout();
    m_wipePosLabel = new QLabel(tr("Position:"), wipeGroup);
    m_wipePosSlider = new QSlider(Qt::Horizontal, wipeGroup);
    m_wipePosSlider->setRange(0, kMaxWipeTicks);
    m_wipePosSlider->setValue(500); // 50% split

    wipeRow2->addWidget(m_wipePosLabel);
    wipeRow2->addWidget(m_wipePosSlider);
    wipeLayout->addLayout(wipeRow2);

    auto* wipeRow3 = new QHBoxLayout();
    m_loadBBtn = new QPushButton(tr("Load Image B..."), wipeGroup);
    m_imageBLabel = new QLabel(tr("Image B: (None)"), wipeGroup);
    m_imageBLabel->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));

    wipeRow3->addWidget(m_loadBBtn);
    wipeRow3->addWidget(m_imageBLabel, 1);
    wipeLayout->addLayout(wipeRow3);

    rootLayout->addWidget(wipeGroup);

    // =========================================================================
    // 4. Viewport Background & Alpha
    // =========================================================================
    auto* bgGroup = new QGroupBox(tr("Viewport Background"), this);
    auto* bgLayout = new QHBoxLayout(bgGroup);
    bgLayout->setContentsMargins(8, 12, 8, 8);

    m_checkerboardCheck = new QCheckBox(tr("Alpha Checkerboard"), bgGroup);
    m_checkerboardCheck->setChecked(true);
    m_checkerboardCheck->setToolTip(tr("Display standard checkerboard pattern behind transparent pixels"));

    m_flipHBtn = new QPushButton(tr("Flip H"), bgGroup);
    m_flipHBtn->setCheckable(true);
    m_flipHBtn->setToolTip(tr("Flip Horizontally"));

    m_flipVBtn = new QPushButton(tr("Flip V"), bgGroup);
    m_flipVBtn->setCheckable(true);
    m_flipVBtn->setToolTip(tr("Flip Vertically"));

    bgLayout->addWidget(m_checkerboardCheck);
    bgLayout->addWidget(m_flipHBtn);
    bgLayout->addWidget(m_flipVBtn);
    bgLayout->addStretch();
    rootLayout->addWidget(bgGroup);

    // =========================================================================
    // 5. 360° Panoramic View & Lookdev
    // =========================================================================
    auto* panoGroup = new QGroupBox(tr("360° Panoramic View"), this);
    auto* panoLayout = new QVBoxLayout(panoGroup);
    panoLayout->setContentsMargins(8, 12, 8, 8);
    panoLayout->setSpacing(8);

    auto* checkRow = new QHBoxLayout();
    m_mode360Check = new QCheckBox(tr("Enable 360° View"), panoGroup);
    m_mode360Check->setChecked(false);
    m_mode360Check->setToolTip(tr("Enable 360° equirectangular panoramic viewport navigation (3)"));

    m_spheresCheck = new QCheckBox(tr("Lookdev Spheres"), panoGroup);
    m_spheresCheck->setChecked(false);
    m_spheresCheck->setToolTip(tr("Render reflective Chrome and diffuse Grey preview spheres (L)"));

    checkRow->addWidget(m_mode360Check);
    checkRow->addWidget(m_spheresCheck);
    panoLayout->addLayout(checkRow);

    auto* camGrid = new QGridLayout();
    camGrid->setContentsMargins(0, 0, 0, 0);
    camGrid->setSpacing(6);

    // Yaw: -180° to 180°
    auto* yawLabel = new QLabel(tr("Yaw:"), panoGroup);
    m_yawSlider = new QSlider(Qt::Horizontal, panoGroup);
    m_yawSlider->setRange(-180, 180);
    m_yawSlider->setSingleStep(1);
    m_yawSlider->setPageStep(15);
    m_yawSlider->setValue(0);
    m_yawSlider->setToolTip(tr("Horizontal camera rotation (-180° to 180°)"));

    m_yawSpin = new QDoubleSpinBox(panoGroup);
    m_yawSpin->setRange(-180.0, 180.0);
    m_yawSpin->setSingleStep(5.0);
    m_yawSpin->setDecimals(1);
    m_yawSpin->setValue(0.0);
    m_yawSpin->setSuffix(QStringLiteral("°"));
    m_yawSpin->setFixedWidth(72);
    m_yawSpin->setToolTip(tr("Horizontal camera rotation in degrees"));

    m_yawValLabel = new QLabel(QStringLiteral("0.0°"), panoGroup);
    m_yawValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_yawValLabel->setFixedWidth(48);

    // Pitch: -90° to 90° (spinbox -89.9 to 89.9)
    auto* pitchLabel = new QLabel(tr("Pitch:"), panoGroup);
    m_pitchSlider = new QSlider(Qt::Horizontal, panoGroup);
    m_pitchSlider->setRange(-90, 90);
    m_pitchSlider->setSingleStep(1);
    m_pitchSlider->setPageStep(10);
    m_pitchSlider->setValue(0);
    m_pitchSlider->setToolTip(tr("Vertical camera elevation (-90° to 90°)"));

    m_pitchSpin = new QDoubleSpinBox(panoGroup);
    m_pitchSpin->setRange(-89.9, 89.9);
    m_pitchSpin->setSingleStep(5.0);
    m_pitchSpin->setDecimals(1);
    m_pitchSpin->setValue(0.0);
    m_pitchSpin->setSuffix(QStringLiteral("°"));
    m_pitchSpin->setFixedWidth(72);
    m_pitchSpin->setToolTip(tr("Vertical camera elevation in degrees"));

    m_pitchValLabel = new QLabel(QStringLiteral("0.0°"), panoGroup);
    m_pitchValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_pitchValLabel->setFixedWidth(48);

    // FOV: 15° to 120°
    auto* fovLabel = new QLabel(tr("FOV:"), panoGroup);
    m_fovSlider = new QSlider(Qt::Horizontal, panoGroup);
    m_fovSlider->setRange(15, 120);
    m_fovSlider->setSingleStep(1);
    m_fovSlider->setPageStep(5);
    m_fovSlider->setValue(35);
    m_fovSlider->setToolTip(tr("Field of view (15° to 120°)"));

    m_fovSpin = new QDoubleSpinBox(panoGroup);
    m_fovSpin->setRange(15.0, 120.0);
    m_fovSpin->setSingleStep(5.0);
    m_fovSpin->setDecimals(1);
    m_fovSpin->setValue(35.0);
    m_fovSpin->setSuffix(QStringLiteral("°"));
    m_fovSpin->setFixedWidth(72);
    m_fovSpin->setToolTip(tr("Field of view in degrees (15° to 120°)"));

    m_fovValLabel = new QLabel(QStringLiteral("35.0°"), panoGroup);
    m_fovValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_fovValLabel->setFixedWidth(48);

    m_resetCamera360Btn = new QPushButton(tr("Reset View"), panoGroup);
    m_resetCamera360Btn->setToolTip(tr("Reset camera yaw to 0°, pitch to 0°, and FOV to 35° (R)"));

    camGrid->addWidget(yawLabel, 0, 0);
    camGrid->addWidget(m_yawSlider, 0, 1);
    camGrid->addWidget(m_yawSpin, 0, 2);
    camGrid->addWidget(m_yawValLabel, 0, 3);

    camGrid->addWidget(pitchLabel, 1, 0);
    camGrid->addWidget(m_pitchSlider, 1, 1);
    camGrid->addWidget(m_pitchSpin, 1, 2);
    camGrid->addWidget(m_pitchValLabel, 1, 3);

    camGrid->addWidget(fovLabel, 2, 0);
    camGrid->addWidget(m_fovSlider, 2, 1);
    camGrid->addWidget(m_fovSpin, 2, 2);
    camGrid->addWidget(m_fovValLabel, 2, 3);

    camGrid->addWidget(m_resetCamera360Btn, 3, 0, 1, 4);

    const bool init360 = m_mode360Check->isChecked();
    m_yawSlider->setEnabled(init360);
    m_yawSpin->setEnabled(init360);
    m_yawValLabel->setEnabled(init360);
    m_pitchSlider->setEnabled(init360);
    m_pitchSpin->setEnabled(init360);
    m_pitchValLabel->setEnabled(init360);
    m_fovSlider->setEnabled(init360);
    m_fovSpin->setEnabled(init360);
    m_fovValLabel->setEnabled(init360);
    m_resetCamera360Btn->setEnabled(init360);

    panoLayout->addLayout(camGrid);
    rootLayout->addWidget(panoGroup);

    // =========================================================================
    // 6. Lookdev Rig — taille, coin d'ancrage, matité, color checker
    // =========================================================================
    auto* rigGroup = new QGroupBox(tr("Lookdev Rig"), this);
    auto* rigGrid = new QGridLayout(rigGroup);
    rigGrid->setContentsMargins(8, 12, 8, 8);
    rigGrid->setSpacing(6);

    m_rigCornerCombo = new QComboBox(rigGroup);
    m_rigCornerCombo->addItems({tr("Bottom-Left"), tr("Bottom-Right"),
                                tr("Top-Left"), tr("Top-Right")});
    m_rigCornerCombo->setToolTip(tr("Corner where the grey/chrome spheres and the color checker sit"));

    m_chartCheck = new QCheckBox(tr("Color Checker (Macbeth)"), rigGroup);
    m_chartCheck->setChecked(true);
    m_chartCheck->setToolTip(tr("Show the 24-patch chart next to the spheres"));

    rigGrid->addWidget(new QLabel(tr("Corner:"), rigGroup), 0, 0);
    rigGrid->addWidget(m_rigCornerCombo, 0, 1, 1, 2);
    rigGrid->addWidget(m_chartCheck, 1, 0, 1, 3);

    rootLayout->addWidget(rigGroup);

    rootLayout->addStretch(1);
}

void AnalysisDock::setupConnections()
{
    // Exposition
    connect(m_exposureSlider, &QSlider::valueChanged, this, &AnalysisDock::onExposureSliderChanged);
    connect(m_exposureSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AnalysisDock::onExposureSpinBoxChanged);
    connect(m_exposureResetBtn, &QPushButton::clicked, this, &AnalysisDock::onExposureResetClicked);

    // Gamma
    connect(m_gammaSlider, &QSlider::valueChanged, this, &AnalysisDock::onGammaSliderChanged);
    connect(m_gammaSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AnalysisDock::onGammaSpinBoxChanged);
    connect(m_gammaResetBtn, &QPushButton::clicked, this, &AnalysisDock::onGammaResetClicked);

    // Channels / AOV & Color Space
    connect(m_aovCombo, qOverload<int>(&QComboBox::activated), this, &AnalysisDock::onAovComboActivated);
    connect(m_ocioCombo, qOverload<int>(&QComboBox::activated), this, &AnalysisDock::onOcioComboActivated);

    // Wipe
    connect(m_wipeToggleBtn, &QPushButton::toggled, this, &AnalysisDock::onWipeToggled);
    connect(m_wipeModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AnalysisDock::onWipeModeChanged);
    connect(m_wipePosSlider, &QSlider::valueChanged, this, &AnalysisDock::onWipePosSliderChanged);
    connect(m_loadBBtn, &QPushButton::clicked, this, &AnalysisDock::onLoadBClicked);

    // Checkerboard & Flip
    connect(m_checkerboardCheck, &QCheckBox::toggled, this, &AnalysisDock::onCheckerboardToggled);
    connect(m_flipHBtn, &QPushButton::toggled, this, &AnalysisDock::onFlipHToggled);
    connect(m_flipVBtn, &QPushButton::toggled, this, &AnalysisDock::onFlipVToggled);

    // 360° & Lookdev
    connect(m_mode360Check, &QCheckBox::toggled, this, &AnalysisDock::onMode360Toggled);
    connect(m_spheresCheck, &QCheckBox::toggled, this, &AnalysisDock::onSpheresToggled);
    connect(m_yawSlider, &QSlider::valueChanged, this, &AnalysisDock::onYawSliderChanged);
    connect(m_pitchSlider, &QSlider::valueChanged, this, &AnalysisDock::onPitchSliderChanged);
    connect(m_fovSlider, &QSlider::valueChanged, this, &AnalysisDock::onFovSliderChanged);    connect(m_yawSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AnalysisDock::onYawSpinChanged);
    connect(m_pitchSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AnalysisDock::onPitchSpinChanged);
    connect(m_fovSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &AnalysisDock::onFovSpinChanged);
    connect(m_resetCamera360Btn, &QPushButton::clicked, this, &AnalysisDock::onResetCamera360Clicked);

    // Lookdev rig
    connect(m_rigCornerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AnalysisDock::onRigCornerChanged);
    connect(m_chartCheck, &QCheckBox::toggled, this, &AnalysisDock::onColorChartToggled);

    // ViewModel -> UI notifications
    if (m_vm) {
        connect(m_vm, &ViewerViewModel::displayChanged, this, &AnalysisDock::onDisplayChanged);
        connect(m_vm, &ViewerViewModel::wipeChanged, this, &AnalysisDock::onDisplayChanged);
        connect(m_vm, &ViewerViewModel::gammaChanged, this, &AnalysisDock::onDisplayChanged);
        connect(m_vm, &ViewerViewModel::checkerboardChanged, this, &AnalysisDock::onDisplayChanged);
        connect(m_vm, &ViewerViewModel::imageChanged, this, &AnalysisDock::onImageChanged);
        connect(m_vm, &ViewerViewModel::aovsChanged, this, &AnalysisDock::onImageChanged);
        connect(m_vm, &ViewerViewModel::imageBChanged, this, &AnalysisDock::onImageBChanged);
        connect(m_vm, &ViewerViewModel::mode360Changed, this, &AnalysisDock::onMode360Changed);
        connect(m_vm, &ViewerViewModel::previewSpheresChanged, this, &AnalysisDock::onSpheresChanged);
        connect(m_vm, &ViewerViewModel::camera360Changed, this, &AnalysisDock::onCamera360Changed);
    }
}

void AnalysisDock::refreshFromViewModel()
{
    if (!m_vm)
        return;

    onDisplayChanged();
    populateAovCombo();
    onImageBChanged();
    onMode360Changed(m_vm->is360Mode());
    onSpheresChanged(m_vm->showPreviewSpheres());
    onCamera360Changed(m_vm->yaw(), m_vm->pitch(), m_vm->fov());

    if (m_ocioCombo) {
        m_updatingOcio = true;
        const QString current = m_vm->currentColorSpace();
        const QStringList spaces = m_vm->availableColorSpaces();
        m_ocioCombo->clear();
        m_ocioCombo->addItems(spaces);
        const int idx = m_ocioCombo->findText(current);
        if (idx >= 0) {
            m_ocioCombo->setCurrentIndex(idx);
        }
        m_updatingOcio = false;
    }
}

void AnalysisDock::populateAovCombo()
{
    m_updatingAov = true;
    m_aovCombo->clear();

    // Canaux fixes standards
    m_aovCombo->addItem(tr("RGBA"), int(ChannelMode::RGB));
    m_aovCombo->addItem(tr("Red"), int(ChannelMode::Red));
    m_aovCombo->addItem(tr("Green"), int(ChannelMode::Green));
    m_aovCombo->addItem(tr("Blue"), int(ChannelMode::Blue));
    m_aovCombo->addItem(tr("Alpha"), int(ChannelMode::Alpha));
    m_aovCombo->addItem(tr("Luminance"), int(ChannelMode::Luma));

    // Couches AOV dynamiques
    if (m_vm) {
        const QStringList aovs = m_vm->availableAovs();
        bool hasCustomAovs = false;
        for (const QString& aov : aovs) {
            if (aov.compare("RGBA", Qt::CaseInsensitive) != 0) {
                hasCustomAovs = true;
                break;
            }
        }

        if (hasCustomAovs) {
            m_aovCombo->insertSeparator(m_aovCombo->count());
            for (const QString& aov : aovs) {
                if (aov.compare("RGBA", Qt::CaseInsensitive) != 0) {
                    m_aovCombo->addItem(QStringLiteral("AOV: %1").arg(aov), aov);
                }
            }
        }

        // Synchroniser la sélection
        const QString currentAov = m_vm->currentAov();
        if (!currentAov.isEmpty() && currentAov.compare("RGBA", Qt::CaseInsensitive) != 0) {
            const int idx = m_aovCombo->findData(currentAov);
            if (idx >= 0) {
                m_aovCombo->setCurrentIndex(idx);
                m_updatingAov = false;
                return;
            }
        }

        const int chIdx = m_aovCombo->findData(int(m_vm->channel()));
        if (chIdx >= 0) {
            m_aovCombo->setCurrentIndex(chIdx);
        }
    }
    m_updatingAov = false;
}

// -----------------------------------------------------------------------------
// Exposition Slots
// -----------------------------------------------------------------------------
void AnalysisDock::onExposureSliderChanged(int intValue)
{
    if (m_updatingExposure)
        return;
    m_updatingExposure = true;
    const double ev = static_cast<double>(intValue) / kExposureScale;
    m_exposureSpin->setValue(ev);
    if (m_vm) {
        m_vm->setExposure(ev);
    }
    m_updatingExposure = false;
}

void AnalysisDock::onExposureSpinBoxChanged(double value)
{
    if (m_updatingExposure)
        return;
    m_updatingExposure = true;
    const int sliderTicks = std::clamp(static_cast<int>(std::round(value * kExposureScale)), -1000, 1000);
    m_exposureSlider->setValue(sliderTicks);
    if (m_vm) {
        m_vm->setExposure(value);
    }
    m_updatingExposure = false;
}

void AnalysisDock::onExposureResetClicked()
{
    if (m_vm) {
        m_vm->resetExposure();
    } else {
        m_exposureSlider->setValue(0);
        m_exposureSpin->setValue(0.0);
    }
}

// -----------------------------------------------------------------------------
// Gamma Slots
// -----------------------------------------------------------------------------
void AnalysisDock::onGammaSliderChanged(int intValue)
{
    if (m_updatingGamma)
        return;
    m_updatingGamma = true;
    const double g = static_cast<double>(intValue) / kGammaScale;
    m_gammaSpin->setValue(g);
    if (m_vm) {
        m_vm->setGamma(g);
    }
    m_updatingGamma = false;
}

void AnalysisDock::onGammaSpinBoxChanged(double value)
{
    if (m_updatingGamma)
        return;
    m_updatingGamma = true;
    const int sliderTicks = std::clamp(static_cast<int>(std::round(value * kGammaScale)), 20, 500);
    m_gammaSlider->setValue(sliderTicks);
    if (m_vm) {
        m_vm->setGamma(value);
    }
    m_updatingGamma = false;
}

void AnalysisDock::onGammaResetClicked()
{
    if (m_vm) {
        m_vm->resetGamma();
    } else {
        m_gammaSlider->setValue(100);
        m_gammaSpin->setValue(1.0);
    }
}

// -----------------------------------------------------------------------------
// Channel / AOV Slots
// -----------------------------------------------------------------------------
void AnalysisDock::onAovComboActivated(int index)
{
    if (m_updatingAov || !m_vm || index < 0)
        return;

    const QVariant data = m_aovCombo->itemData(index);
    if (data.userType() == QMetaType::Int) {
        m_vm->setAov(QString()); // rétablit RGBA
        m_vm->setChannel(static_cast<ChannelMode>(data.toInt()));
    } else if (data.userType() == QMetaType::QString) {
        m_vm->setAov(data.toString());
    }
}

void AnalysisDock::onOcioComboActivated(int index)
{
    if (m_updatingOcio || !m_vm || index < 0 || !m_ocioCombo)
        return;

    const QString space = m_ocioCombo->itemText(index);
    m_vm->setColorSpace(space);
}

// -----------------------------------------------------------------------------
// A/B Wipe Slots
// -----------------------------------------------------------------------------
void AnalysisDock::onWipeToggled(bool checked)
{
    if (m_updatingWipe)
        return;
    m_wipeToggleBtn->setText(checked ? tr("Disable A/B Wipe") : tr("Enable A/B Wipe"));
    if (m_vm) {
        m_vm->setWipeEnabled(checked);
    }
}

void AnalysisDock::onWipeModeChanged(int index)
{
    if (m_updatingWipe)
        return;
    if (m_wipePosLabel) {
        m_wipePosLabel->setText(index == 2 ? tr("Opacity:") : tr("Position:"));
    }
    if (m_vm) {
        m_vm->setWipeMode(index);
    }
}

void AnalysisDock::onWipePosSliderChanged(int intValue)
{
    if (m_updatingWipe)
        return;
    const float pos = static_cast<float>(intValue) / static_cast<float>(kMaxWipeTicks);
    if (m_vm) {
        m_vm->setWipePos(pos);
    }
}

void AnalysisDock::onLoadBClicked()
{
    emit requestLoadImageB();
}

void AnalysisDock::onCheckerboardToggled(bool checked)
{
    if (m_vm) {
        m_vm->setCheckerboardEnabled(checked);
    }
}

void AnalysisDock::onFlipHToggled(bool checked)
{
    if (m_vm) {
        m_vm->setFlipH(checked);
    }
}

void AnalysisDock::onFlipVToggled(bool checked)
{
    if (m_vm) {
        m_vm->setFlipV(checked);
    }
}

// -----------------------------------------------------------------------------
// ViewModel -> UI Slots
// -----------------------------------------------------------------------------
void AnalysisDock::onDisplayChanged()
{
    if (!m_vm)
        return;

    // Synchronisation Exposition
    if (!m_updatingExposure) {
        m_updatingExposure = true;
        const double ev = m_vm->exposureStops();
        m_exposureSpin->setValue(ev);
        m_exposureSlider->setValue(std::clamp(static_cast<int>(std::round(ev * kExposureScale)), -1000, 1000));
        m_updatingExposure = false;
    }

    // Synchronisation Gamma
    if (!m_updatingGamma) {
        m_updatingGamma = true;
        const double g = m_vm->gamma();
        m_gammaSpin->setValue(g);
        m_gammaSlider->setValue(std::clamp(static_cast<int>(std::round(g * kGammaScale)), 20, 500));
        m_updatingGamma = false;
    }

    // Synchronisation Wipe
    if (!m_updatingWipe) {
        m_updatingWipe = true;
        const bool wipeOn = m_vm->wipeEnabled();
        m_wipeToggleBtn->setChecked(wipeOn);
        m_wipeToggleBtn->setText(wipeOn ? tr("Disable A/B Wipe") : tr("Enable A/B Wipe"));
        m_wipeModeCombo->setCurrentIndex(m_vm->wipeMode());
        if (m_wipePosLabel) {
            m_wipePosLabel->setText(m_vm->wipeMode() == 2 ? tr("Opacity:") : tr("Position:"));
        }
        m_wipePosSlider->setValue(std::clamp(static_cast<int>(std::round(m_vm->wipePos() * kMaxWipeTicks)), 0, kMaxWipeTicks));
        m_updatingWipe = false;
    }

    // Synchronisation Checkerboard & Flip
    m_checkerboardCheck->setChecked(m_vm->checkerboardEnabled());
    
    QSignalBlocker bH(m_flipHBtn);
    QSignalBlocker bV(m_flipVBtn);
    m_flipHBtn->setChecked(m_vm->flipH());
    m_flipVBtn->setChecked(m_vm->flipV());

    // Synchronisation Color Space (OCIO)
    if (m_ocioCombo && !m_updatingOcio) {
        m_updatingOcio = true;
        const QString current = m_vm->currentColorSpace();
        const int idx = m_ocioCombo->findText(current);
        if (idx >= 0 && idx != m_ocioCombo->currentIndex()) {
            m_ocioCombo->setCurrentIndex(idx);
        }
        m_updatingOcio = false;
    }
}

void AnalysisDock::onImageChanged()
{
    populateAovCombo();
}

void AnalysisDock::onImageBChanged()
{
    if (!m_vm)
        return;

    const ImagePtr imgB = m_vm->imageB();
    if (imgB && !imgB->path.isEmpty()) {
        m_imageBLabel->setText(QStringLiteral("Image B: %1").arg(QFileInfo(imgB->path).fileName()));
        m_imageBLabel->setStyleSheet(QStringLiteral("color: #4db6ac; font-weight: bold;"));
    } else {
        m_imageBLabel->setText(tr("Image B: (None)"));
        m_imageBLabel->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));
    }
}

void AnalysisDock::onMode360Toggled(bool checked)
{
    if (m_vm)
        m_vm->set360Mode(checked);
}

void AnalysisDock::onSpheresToggled(bool checked)
{
    if (m_vm)
        m_vm->setShowPreviewSpheres(checked);
}

void AnalysisDock::onYawSliderChanged(int value)
{
    if (m_updatingCamera360)
        return;
    m_updatingCamera360 = true;
    const double valDeg = static_cast<double>(value);
    if (m_yawSpin) {
        QSignalBlocker b(m_yawSpin);
        m_yawSpin->setValue(valDeg);
    }
    if (m_yawValLabel) {
        m_yawValLabel->setText(QString::asprintf("%.1f°", valDeg));
    }
    if (m_vm) {
        m_vm->setYaw(float(valDeg * 3.14159265358979323846 / 180.0));
    }
    m_updatingCamera360 = false;
}

void AnalysisDock::onPitchSliderChanged(int value)
{
    if (m_updatingCamera360)
        return;
    m_updatingCamera360 = true;
    const double valDeg = static_cast<double>(value);
    if (m_pitchSpin) {
        QSignalBlocker b(m_pitchSpin);
        m_pitchSpin->setValue(valDeg);
    }
    if (m_pitchValLabel) {
        m_pitchValLabel->setText(QString::asprintf("%.1f°", valDeg));
    }
    if (m_vm) {
        m_vm->setPitch(float(valDeg * 3.14159265358979323846 / 180.0));
    }
    m_updatingCamera360 = false;
}

void AnalysisDock::onFovSliderChanged(int value)
{
    if (m_updatingCamera360)
        return;
    m_updatingCamera360 = true;
    const double valDeg = static_cast<double>(value);
    if (m_fovSpin) {
        QSignalBlocker b(m_fovSpin);
        m_fovSpin->setValue(valDeg);
    }
    if (m_fovValLabel) {
        m_fovValLabel->setText(QString::asprintf("%.1f°", valDeg));
    }
    if (m_vm) {
        m_vm->setFov(float(valDeg * 3.14159265358979323846 / 180.0));
    }
    m_updatingCamera360 = false;
}

void AnalysisDock::onYawSpinChanged(double value)
{
    if (m_updatingCamera360 || !m_vm)
        return;
    m_updatingCamera360 = true;
    if (m_yawSlider) {
        QSignalBlocker b(m_yawSlider);
        m_yawSlider->setValue(qRound(value));
    }
    if (m_yawValLabel) {
        m_yawValLabel->setText(QString::asprintf("%.1f°", value));
    }
    m_vm->setYaw(float(value * 3.14159265358979323846 / 180.0));
    m_updatingCamera360 = false;
}

void AnalysisDock::onPitchSpinChanged(double value)
{
    if (m_updatingCamera360 || !m_vm)
        return;
    m_updatingCamera360 = true;
    if (m_pitchSlider) {
        QSignalBlocker b(m_pitchSlider);
        m_pitchSlider->setValue(qRound(value));
    }
    if (m_pitchValLabel) {
        m_pitchValLabel->setText(QString::asprintf("%.1f°", value));
    }
    m_vm->setPitch(float(value * 3.14159265358979323846 / 180.0));
    m_updatingCamera360 = false;
}

void AnalysisDock::onFovSpinChanged(double value)
{
    if (m_updatingCamera360 || !m_vm)
        return;
    m_updatingCamera360 = true;
    if (m_fovSlider) {
        QSignalBlocker b(m_fovSlider);
        m_fovSlider->setValue(qRound(value));
    }
    if (m_fovValLabel) {
        m_fovValLabel->setText(QString::asprintf("%.1f°", value));
    }
    m_vm->setFov(float(value * 3.14159265358979323846 / 180.0));
    m_updatingCamera360 = false;
}

void AnalysisDock::onResetCamera360Clicked()
{
    if (m_vm)
        m_vm->resetCamera360();
}

// -----------------------------------------------------------------------------
// Lookdev rig
// -----------------------------------------------------------------------------
void AnalysisDock::onRigCornerChanged(int index)
{
    if (m_vm) {
        m_vm->setRigCorner(index);
    }
}

void AnalysisDock::onColorChartToggled(bool checked)
{
    if (m_vm) {
        m_vm->setShowColorChart(checked);
    }
}

void AnalysisDock::onMode360Changed(bool enabled)
{
    if (m_mode360Check && m_mode360Check->isChecked() != enabled) {
        QSignalBlocker b(m_mode360Check);
        m_mode360Check->setChecked(enabled);
    }
    const bool active = enabled;
    if (m_yawSlider) m_yawSlider->setEnabled(active);
    if (m_yawSpin) m_yawSpin->setEnabled(active);
    if (m_yawValLabel) m_yawValLabel->setEnabled(active);
    if (m_pitchSlider) m_pitchSlider->setEnabled(active);
    if (m_pitchSpin) m_pitchSpin->setEnabled(active);
    if (m_pitchValLabel) m_pitchValLabel->setEnabled(active);
    if (m_fovSlider) m_fovSlider->setEnabled(active);
    if (m_fovSpin) m_fovSpin->setEnabled(active);
    if (m_fovValLabel) m_fovValLabel->setEnabled(active);
    if (m_resetCamera360Btn) m_resetCamera360Btn->setEnabled(active);
}

void AnalysisDock::onSpheresChanged(bool enabled)
{
    if (m_spheresCheck && m_spheresCheck->isChecked() != enabled) {
        QSignalBlocker b(m_spheresCheck);
        m_spheresCheck->setChecked(enabled);
    }
}

void AnalysisDock::onCamera360Changed(float yaw, float pitch, float fov)
{
    if (m_updatingCamera360)
        return;
    m_updatingCamera360 = true;
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    const double yawDeg = double(yaw) * kRadToDeg;
    const double pitchDeg = double(pitch) * kRadToDeg;
    const double fovDeg = double(fov) * kRadToDeg;

    if (m_yawSpin) m_yawSpin->setValue(yawDeg);
    if (m_yawSlider) m_yawSlider->setValue(qRound(yawDeg));
    if (m_yawValLabel) m_yawValLabel->setText(QString::asprintf("%.1f°", yawDeg));

    if (m_pitchSpin) m_pitchSpin->setValue(pitchDeg);
    if (m_pitchSlider) m_pitchSlider->setValue(qRound(pitchDeg));
    if (m_pitchValLabel) m_pitchValLabel->setText(QString::asprintf("%.1f°", pitchDeg));

    if (m_fovSpin) m_fovSpin->setValue(fovDeg);
    if (m_fovSlider) m_fovSlider->setValue(qRound(fovDeg));
    if (m_fovValLabel) m_fovValLabel->setText(QString::asprintf("%.1f°", fovDeg));

    m_updatingCamera360 = false;
}

}  // namespace sview
