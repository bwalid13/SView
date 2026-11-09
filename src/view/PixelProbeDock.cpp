#include "PixelProbeDock.h"
#include "../viewmodel/ViewerViewModel.h"

#include <DockManager.h>
#include <DockWidget.h>

#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QShortcut>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace sview {

// =============================================================================
// ColorSwatchWidget Implementation
// =============================================================================
ColorSwatchWidget::ColorSwatchWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(64, 64);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
}

void ColorSwatchWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        emit clicked();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ColorSwatchWidget::setColor(const QColor& color, float alphaFloat)
{
    m_hasColor = true;
    m_color = color;
    m_alpha = std::clamp(alphaFloat, 0.0f, 1.0f);
    update();
}

void ColorSwatchWidget::clearColor()
{
    m_hasColor = false;
    m_color = QColor();
    m_alpha = 1.0f;
    update();
}

void ColorSwatchWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(1, 1, -1, -1);

    if (!m_hasColor) {
        p.fillRect(r, QColor(0x1a, 0x1a, 0x1a));
        p.setPen(QColor(0x3a, 0x3a, 0x3a));
        p.drawRoundedRect(r, 4, 4);
        p.setPen(QColor(0x55, 0x55, 0x55));
        p.drawLine(r.topLeft(), r.bottomRight());
        return;
    }

    // 1. Damier de transparence
    const int cellSize = 8;
    for (int y = r.top(); y < r.bottom(); y += cellSize) {
        for (int x = r.left(); x < r.right(); x += cellSize) {
            const bool isLight = (((x / cellSize) + (y / cellSize)) % 2 == 0);
            p.fillRect(QRect(x, y, cellSize, cellSize).intersected(r),
                       isLight ? QColor(0x38, 0x38, 0x38) : QColor(0x22, 0x22, 0x22));
        }
    }

    // 2. Échantillon couleur avec alpha
    QColor fill = m_color;
    fill.setAlphaF(m_alpha);
    p.fillRect(r, fill);

    // 3. Bordure
    p.setPen(QColor(0x55, 0x55, 0x55));
    p.drawRoundedRect(r, 4, 4);
}

// =============================================================================
// PixelProbeDock Implementation
// =============================================================================
PixelProbeDock::PixelProbeDock(ViewerViewModel* vm, QWidget* parent)
    : QWidget(parent)
    , m_vm(vm)
{
    setupUi();
    setupShortcuts();
    setupConnections();
    clearProbe();
}

ads::CDockWidget* PixelProbeDock::createDockWidget(ads::CDockManager* dockManager,
                                                   ViewerViewModel* vm,
                                                   QWidget* parent)
{
    auto* dockContent = new PixelProbeDock(vm, parent);
    auto* dockWidget = new ads::CDockWidget(dockManager, tr("Pixel Probe"));
    dockWidget->setObjectName(QStringLiteral("PixelProbeDockWidget"));
    dockWidget->setWidget(dockContent);
    dockWidget->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);
    return dockWidget;
}

void PixelProbeDock::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    const QString monoStyle = QStringLiteral(
        "font-family: Consolas, 'DejaVu Sans Mono', monospace; font-size: 11px;");

    // =========================================================================
    // 1. Coordinates Group
    // =========================================================================
    auto* coordGroup = new QGroupBox(tr("Coordinates"), this);
    auto* coordLayout = new QVBoxLayout(coordGroup);
    coordLayout->setContentsMargins(8, 10, 8, 8);
    coordLayout->setSpacing(4);

    m_coordLabel = new QLabel(tr("X: ---   Y: ---"), coordGroup);
    m_coordLabel->setStyleSheet(monoStyle + QStringLiteral(" font-weight: bold; color: #f1f5f9;"));

    m_normCoordLabel = new QLabel(tr("U: ---   V: --- (Normalized)"), coordGroup);
    m_normCoordLabel->setStyleSheet(monoStyle + QStringLiteral(" color: #94a3b8;"));

    coordLayout->addWidget(m_coordLabel);
    coordLayout->addWidget(m_normCoordLabel);
    rootLayout->addWidget(coordGroup);

    // =========================================================================
    // 2. RGBA Unclipped Floats Group
    // =========================================================================
    auto* rgbaGroup = new QGroupBox(tr("RGBA 32-bit Float Values"), this);
    auto* rgbaLayout = new QGridLayout(rgbaGroup);
    rgbaLayout->setContentsMargins(8, 10, 8, 8);
    rgbaLayout->setSpacing(6);

    auto makeChannelRow = [&](int row, const QString& name, const QString& colorHex,
                              QLabel*& valLabel, QLabel*& sciLabel) {
        auto* tag = new QLabel(name, rgbaGroup);
        tag->setStyleSheet(QStringLiteral("font-weight: bold; color: %1; min-width: 16px;").arg(colorHex));

        valLabel = new QLabel(QStringLiteral("0.0000"), rgbaGroup);
        valLabel->setStyleSheet(monoStyle + QStringLiteral(" font-weight: bold; color: #f1f5f9;"));

        sciLabel = new QLabel(QStringLiteral("(0.0000e+00)"), rgbaGroup);
        sciLabel->setStyleSheet(monoStyle + QStringLiteral(" color: #888;"));

        rgbaLayout->addWidget(tag, row, 0);
        rgbaLayout->addWidget(valLabel, row, 1);
        rgbaLayout->addWidget(sciLabel, row, 2);
    };

    makeChannelRow(0, QStringLiteral("R:"), QStringLiteral("#ff5252"), m_rValLabel, m_rSciLabel);
    makeChannelRow(1, QStringLiteral("G:"), QStringLiteral("#69f0ae"), m_gValLabel, m_gSciLabel);
    makeChannelRow(2, QStringLiteral("B:"), QStringLiteral("#448aff"), m_bValLabel, m_bSciLabel);
    makeChannelRow(3, QStringLiteral("A:"), QStringLiteral("#e0e0e0"), m_aValLabel, m_aSciLabel);

    rootLayout->addWidget(rgbaGroup);

    // =========================================================================
    // 3. Color Swatch & Metrics Group
    // =========================================================================
    auto* previewGroup = new QGroupBox(tr("Color Swatch & Metrics"), this);
    auto* previewLayout = new QHBoxLayout(previewGroup);
    previewLayout->setContentsMargins(8, 10, 8, 8);
    previewLayout->setSpacing(12);

    m_swatch = new ColorSwatchWidget(previewGroup);

    auto* metricLayout = new QVBoxLayout();
    metricLayout->setSpacing(4);

    m_luminanceLabel = new QLabel(tr("Luma (Y):  ---"), previewGroup);
    m_luminanceLabel->setStyleSheet(monoStyle + QStringLiteral(" color: #f1f5f9;"));

    m_hexLabel = new QLabel(tr("Hex:  #------"), previewGroup);
    m_hexLabel->setStyleSheet(monoStyle + QStringLiteral(" font-weight: bold; color: #38bdf8;"));

    metricLayout->addWidget(m_luminanceLabel);
    metricLayout->addWidget(m_hexLabel);
    metricLayout->addStretch(1);

    previewLayout->addWidget(m_swatch);
    previewLayout->addLayout(metricLayout, 1);
    rootLayout->addWidget(previewGroup);

    // =========================================================================
    // 4. Copy Action
    // =========================================================================
    m_copyButton = new QPushButton(tr("Copy Values (Ctrl+C)"), this);
    m_copyButton->setToolTip(tr("Copy current pixel coordinates and RGBA floats to clipboard"));
    rootLayout->addWidget(m_copyButton);

    rootLayout->addStretch(1);
}

void PixelProbeDock::setupShortcuts()
{
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &PixelProbeDock::copyToClipboard);
    connect(m_copyButton, &QPushButton::clicked, this, &PixelProbeDock::copyToClipboard);
}

void PixelProbeDock::setupConnections()
{
    if (m_swatch) {
        connect(m_swatch, &ColorSwatchWidget::clicked, this, &PixelProbeDock::copyRgbaFloatToClipboard);
    }
    if (m_vm) {
        connect(m_vm, &ViewerViewModel::probeUpdated, this, &PixelProbeDock::onProbeUpdated);
    }
}

void PixelProbeDock::onProbeUpdated(const PixelProbeData& data)
{
    setProbeData(data);
}

void PixelProbeDock::setProbeData(const PixelProbeData& data)
{
    m_currentData = data;

    if (!data.hasPixel) {
        clearProbe();
        return;
    }

    // Coordonnées
    m_coordLabel->setText(QStringLiteral("X: %1   Y: %2").arg(data.x).arg(data.y));
    m_normCoordLabel->setText(
        QStringLiteral("U: %1   V: %2 (Normalized)")
            .arg(QString::asprintf("%1.4f", data.u))
            .arg(QString::asprintf("%1.4f", data.v)));

    // Valeurs RGBA Float (%1.4f)
    m_rValLabel->setText(QString::asprintf("%1.4f", data.r));
    m_gValLabel->setText(QString::asprintf("%1.4f", data.g));
    m_bValLabel->setText(QString::asprintf("%1.4f", data.b));
    m_aValLabel->setText(QString::asprintf("%1.4f", data.a));

    // Notation scientifique (%1.4e)
    m_rSciLabel->setText(QStringLiteral("(%1)").arg(QString::asprintf("%1.4e", data.r)));
    m_gSciLabel->setText(QStringLiteral("(%1)").arg(QString::asprintf("%1.4e", data.g)));
    m_bSciLabel->setText(QStringLiteral("(%1)").arg(QString::asprintf("%1.4e", data.b)));
    m_aSciLabel->setText(QStringLiteral("(%1)").arg(QString::asprintf("%1.4e", data.a)));

    // Luminance & Hex
    m_luminanceLabel->setText(
        QStringLiteral("Luma (Y):  %1").arg(QString::asprintf("%1.4f", data.luminance)));
    m_hexLabel->setText(QStringLiteral("Hex:  %1").arg(data.hexCode));

    // Swatch : conversion linéaire HDR vers sRGB avec gamma 2.2 approximé
    int ir = std::clamp(static_cast<int>(std::round(std::pow(std::clamp(data.r, 0.0f, 1.0f), 1.0f / 2.2f) * 255.0f)), 0, 255);
    int ig = std::clamp(static_cast<int>(std::round(std::pow(std::clamp(data.g, 0.0f, 1.0f), 1.0f / 2.2f) * 255.0f)), 0, 255);
    int ib = std::clamp(static_cast<int>(std::round(std::pow(std::clamp(data.b, 0.0f, 1.0f), 1.0f / 2.2f) * 255.0f)), 0, 255);
    m_swatch->setColor(QColor(ir, ig, ib), data.a);
}

void PixelProbeDock::clearProbe()
{
    m_currentData = PixelProbeData{};
    m_coordLabel->setText(tr("X: ---   Y: ---"));
    m_normCoordLabel->setText(tr("U: ---   V: --- (Normalized)"));

    m_rValLabel->setText(QStringLiteral("---"));
    m_gValLabel->setText(QStringLiteral("---"));
    m_bValLabel->setText(QStringLiteral("---"));
    m_aValLabel->setText(QStringLiteral("---"));

    m_rSciLabel->setText(QStringLiteral("(---)"));
    m_gSciLabel->setText(QStringLiteral("(---)"));
    m_bSciLabel->setText(QStringLiteral("(---)"));
    m_aSciLabel->setText(QStringLiteral("(---)"));

    m_luminanceLabel->setText(tr("Luma (Y):  ---"));
    m_hexLabel->setText(tr("Hex:  #------"));
    m_swatch->clearColor();
}

QString PixelProbeDock::copyCurrentPixel()
{
    QString text;
    if (m_currentData.hasPixel) {
        text = QStringLiteral("X: %1  Y: %2  (U: %3, V: %4)\n"
                              "R: %5  (%6)\n"
                              "G: %7  (%8)\n"
                              "B: %9  (%10)\n"
                              "A: %11 (%12)\n"
                              "Luma: %13\n"
                              "Hex: %14")
                   .arg(m_currentData.x).arg(m_currentData.y)
                   .arg(QString::asprintf("%1.4f", m_currentData.u))
                   .arg(QString::asprintf("%1.4f", m_currentData.v))
                   .arg(QString::asprintf("%1.4f", m_currentData.r)).arg(QString::asprintf("%1.4e", m_currentData.r))
                   .arg(QString::asprintf("%1.4f", m_currentData.g)).arg(QString::asprintf("%1.4e", m_currentData.g))
                   .arg(QString::asprintf("%1.4f", m_currentData.b)).arg(QString::asprintf("%1.4e", m_currentData.b))
                   .arg(QString::asprintf("%1.4f", m_currentData.a)).arg(QString::asprintf("%1.4e", m_currentData.a))
                   .arg(QString::asprintf("%1.4f", m_currentData.luminance))
                   .arg(m_currentData.hexCode);

        QToolTip::showText(QCursor::pos(),
                           tr("Pixel Copied (X: %1, Y: %2)\nR: %3 G: %4 B: %5 A: %6")
                               .arg(m_currentData.x).arg(m_currentData.y)
                               .arg(QString::asprintf("%1.4f", m_currentData.r))
                               .arg(QString::asprintf("%1.4f", m_currentData.g))
                               .arg(QString::asprintf("%1.4f", m_currentData.b))
                               .arg(QString::asprintf("%1.4f", m_currentData.a)),
                           this, QRect(), 1500);
    } else {
        text = tr("No pixel under cursor.");
    }

    QGuiApplication::clipboard()->setText(text);

    // Feedback visuel sur le bouton
    if (m_copyButton) {
        m_copyButton->setText(tr("✔ Copied!"));
        QTimer::singleShot(1500, this, [this]() {
            if (m_copyButton) {
                m_copyButton->setText(tr("Copy Values (Ctrl+C)"));
            }
        });
    }

    return text;
}

void PixelProbeDock::copyToClipboard()
{
    copyCurrentPixel();
}

QString PixelProbeDock::formattedRgbaFloat() const
{
    if (!m_currentData.hasPixel) {
        return tr("No pixel under cursor.");
    }
    return QString::asprintf("R: %1.4f  G: %1.4f  B: %1.4f  A: %1.4f",
                             m_currentData.r, m_currentData.g, m_currentData.b, m_currentData.a);
}

void PixelProbeDock::copyRgbaFloatToClipboard()
{
    const QString text = formattedRgbaFloat();
    QGuiApplication::clipboard()->setText(text);

    if (m_copyButton) {
        m_copyButton->setText(tr("✔ Copied!"));
        QTimer::singleShot(1500, this, [this]() {
            if (m_copyButton) {
                m_copyButton->setText(tr("Copy Values (Ctrl+C)"));
            }
        });
    }
}

void PixelProbeDock::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        copyRgbaFloatToClipboard();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PixelProbeDock::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy)) {
        copyToClipboard();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace sview
