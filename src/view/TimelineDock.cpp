#include "TimelineDock.h"
#include "../viewmodel/ViewerViewModel.h"

#include <DockManager.h>
#include <DockWidget.h>

#include <QApplication>
#include <QBoxLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace sview {

// =============================================================================
// TimelineSlider Implementation
// =============================================================================

TimelineSlider::TimelineSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
{
    setObjectName(QStringLiteral("timelineSlider"));
    setFocusPolicy(Qt::StrongFocus);
    setTracking(true);
    setMouseTracking(true);
}

void TimelineSlider::setMissingFrames(const QList<int>& missing, int startFrame, int endFrame)
{
    m_missingFrames = missing;
    m_startFrame = startFrame;
    m_endFrame = endFrame;
    update();
}

void TimelineSlider::clearMissingFrames()
{
    m_missingFrames.clear();
    m_startFrame = 0;
    m_endFrame = 0;
    update();
}

void TimelineSlider::setCachedFrames(const QList<bool>& cached)
{
    m_cachedFrames = cached;
    update();
}

void TimelineSlider::clearCachedFrames()
{
    m_cachedFrames.clear();
    update();
}

int TimelineSlider::pixelPosToValue(int posX) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    const int handleWidth = std::max(8, handle.width());
    const int left = groove.left() + handleWidth / 2;
    const int right = groove.right() - handleWidth / 2;
    const int grooveWidth = right - left;

    if (grooveWidth <= 0 || maximum() <= minimum())
        return minimum();

    const double ratio = std::clamp(static_cast<double>(posX - left) / static_cast<double>(grooveWidth), 0.0, 1.0);
    const int val = minimum() + static_cast<int>(std::round(ratio * (maximum() - minimum())));
    return std::clamp(val, minimum(), maximum());
}

void TimelineSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int val = pixelPosToValue(static_cast<int>(event->position().x()));
        setValue(val);
        emit sliderMoved(val);
        emit frameJumped(val);
        event->accept();
    }
    QSlider::mousePressEvent(event);
}

void TimelineSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        const int val = pixelPosToValue(static_cast<int>(event->position().x()));
        setValue(val);
        emit sliderMoved(val);
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void TimelineSlider::wheelEvent(QWheelEvent* event)
{
    const int numDegrees = event->angleDelta().y();
    if (numDegrees != 0) {
        const int step = (numDegrees > 0) ? 1 : -1;
        const int next = std::clamp(value() + step, minimum(), maximum());
        if (next != value()) {
            setValue(next);
            emit sliderMoved(next);
        }
        event->accept();
        return;
    }
    QSlider::wheelEvent(event);
}

void TimelineSlider::paintEvent(QPaintEvent* event)
{
    // Draw standard styled slider groove and handle (respects theme QSS)
    QSlider::paintEvent(event);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    const int handleWidth = std::max(8, handle.width());
    const int left = groove.left() + handleWidth / 2;
    const int right = groove.right() - handleWidth / 2;
    const int grooveWidth = right - left;

    if (grooveWidth <= 0)
        return;

    // Feature 1: RAM Cache Indicator along bottom of slider groove
    if (!m_cachedFrames.isEmpty()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        // VFX cached indicator: semi-translucent green #4caf50 (QColor(76, 175, 80, 210))
        const QColor cachedColor(76, 175, 80, 210);
        const int barHeight = 5;
        const int barY = groove.bottom() - barHeight;
        const int n = m_cachedFrames.size();

        if (n == 1) {
            if (m_cachedFrames[0]) {
                painter.fillRect(QRect(left, barY, grooveWidth, barHeight), cachedColor);
            }
        } else {
            // Contiguous range coalescing for high performance with 1,000+ frames
            int runStart = -1;
            for (int i = 0; i < n; ++i) {
                if (m_cachedFrames[i]) {
                    if (runStart < 0) {
                        runStart = i;
                    }
                } else {
                    if (runStart >= 0) {
                        const int x1 = left + static_cast<int>(std::round(static_cast<double>(runStart) / static_cast<double>(n) * grooveWidth));
                        const int x2 = left + static_cast<int>(std::round(static_cast<double>(i) / static_cast<double>(n) * grooveWidth));
                        const int w = std::max(1, x2 - x1);
                        painter.fillRect(QRect(x1, barY, w, barHeight), cachedColor);
                        runStart = -1;
                    }
                }
            }
            if (runStart >= 0) {
                const int x1 = left + static_cast<int>(std::round(static_cast<double>(runStart) / static_cast<double>(n) * grooveWidth));
                const int x2 = left + grooveWidth;
                const int w = std::max(1, x2 - x1);
                painter.fillRect(QRect(x1, barY, w, barHeight), cachedColor);
            }
        }
    }

    // If sequence has missing frames, draw visual warning hash marks along the groove
    if (m_missingFrames.isEmpty() || m_endFrame <= m_startFrame)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    // Vibrant VFX warning color for missing frames
    const QColor missingColor(240, 70, 70, 210);
    painter.setPen(QPen(missingColor, 2));

    const double frameSpan = static_cast<double>(m_endFrame - m_startFrame);
    const int tickTop = groove.top() + 1;
    const int tickBottom = groove.bottom() - 1;

    for (int frameNum : m_missingFrames) {
        if (frameNum < m_startFrame || frameNum > m_endFrame)
            continue;
        const double frac = static_cast<double>(frameNum - m_startFrame) / frameSpan;
        const int x = left + static_cast<int>(std::round(frac * grooveWidth));
        painter.drawLine(x, tickTop, x, tickBottom);
    }
}

// =============================================================================
// TimelineDock Implementation
// =============================================================================

TimelineDock::TimelineDock(PlaybackController* controller, ViewerViewModel* vm, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_vm(vm)
{
    setObjectName(QStringLiteral("TimelineDockWidgetContent"));
    setupUi();
    wireSignals();
    setupShortcuts();
    updateUiForSequence();
}

void TimelineDock::setupUi()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);

    // 1. Scrubber Slider (Top Row)
    m_slider = new TimelineSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 0);
    m_slider->setValue(0);
    m_slider->setEnabled(false);
    m_slider->setMinimumHeight(28);
    mainLayout->addWidget(m_slider);

    // 2. Controls Bar (Bottom Row)
    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(3);

    // Helper to configure standard compact VFX transport buttons
    auto createTransportButton = [this](const QString& text, const QString& objName, const QString& tooltip) {
        auto* btn = new QPushButton(text, this);
        btn->setObjectName(objName);
        btn->setToolTip(tooltip);
        btn->setFixedSize(30, 24);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    m_firstFrameButton = createTransportButton(QStringLiteral("|◀"), QStringLiteral("timelineFirstFrameButton"), tr("First Frame (Home)"));
    m_stepBackButton = createTransportButton(QStringLiteral("◀"), QStringLiteral("timelineStepBackButton"), tr("Step Backward 1 Frame (Left)"));
    m_playPauseButton = createTransportButton(QStringLiteral("▶"), QStringLiteral("timelinePlayPauseButton"), tr("Play / Pause (Space)"));
    m_stopButton = createTransportButton(QStringLiteral("⏹"), QStringLiteral("timelineStopButton"), tr("Stop and Rewind"));
    m_stepForwardButton = createTransportButton(QStringLiteral("▶|"), QStringLiteral("timelineStepForwardButton"), tr("Step Forward 1 Frame (Right)"));
    m_lastFrameButton = createTransportButton(QStringLiteral("▶|"), QStringLiteral("timelineLastFrameButton"), tr("Last Frame (End)"));

    controlsLayout->addWidget(m_firstFrameButton);
    controlsLayout->addWidget(m_stepBackButton);
    controlsLayout->addWidget(m_playPauseButton);
    controlsLayout->addWidget(m_stopButton);
    controlsLayout->addWidget(m_stepForwardButton);
    controlsLayout->addWidget(m_lastFrameButton);

    // Separator line
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    controlsLayout->addWidget(sep1);

    // Frame spinbox for direct frame typing
    m_frameSpinBox = new QSpinBox(this);
    m_frameSpinBox->setObjectName(QStringLiteral("timelineFrameSpinBox"));
    m_frameSpinBox->setRange(0, 0);
    m_frameSpinBox->setValue(0);
    m_frameSpinBox->setAlignment(Qt::AlignRight);
    m_frameSpinBox->setMinimumWidth(68);
    m_frameSpinBox->setFixedHeight(30);
    m_frameSpinBox->setToolTip(tr("Current Frame (Type frame number and press Enter)"));
    controlsLayout->addWidget(m_frameSpinBox);

    // Frame range label: e.g. "/ 100 [1001..1100]"
    m_frameRangeLabel = new QLabel(QStringLiteral("0 / 0"), this);
    m_frameRangeLabel->setObjectName(QStringLiteral("timelineFrameRangeLabel"));
    m_frameRangeLabel->setStyleSheet(QStringLiteral("QLabel { color: #a0a0a0; font-size: 13px; padding-left: 2px; }"));
    controlsLayout->addWidget(m_frameRangeLabel);

    // Missing frames badge
    m_missingBadge = new QLabel(this);
    m_missingBadge->setObjectName(QStringLiteral("timelineMissingBadge"));
    m_missingBadge->setAlignment(Qt::AlignCenter);
    m_missingBadge->setFixedHeight(26);
    m_missingBadge->hide(); // Shown when sequence is active
    controlsLayout->addWidget(m_missingBadge);

    // Flexible stretch separating left transport from right FPS/Loop
    controlsLayout->addStretch(1);

    // FPS Selector ComboBox
    m_fpsCombo = new QComboBox(this);
    m_fpsCombo->setObjectName(QStringLiteral("timelineFpsCombo"));
    m_fpsCombo->setFixedHeight(30);
    m_fpsCombo->setMinimumWidth(82);
    m_fpsCombo->setToolTip(tr("Playback Frame Rate (FPS)"));
    m_fpsCombo->addItem(QStringLiteral("24 fps"), 24.0);
    m_fpsCombo->addItem(QStringLiteral("25 fps"), 25.0);
    m_fpsCombo->addItem(QStringLiteral("29.97 fps"), 29.97);
    m_fpsCombo->addItem(QStringLiteral("30 fps"), 30.0);
    m_fpsCombo->addItem(QStringLiteral("48 fps"), 48.0);
    m_fpsCombo->addItem(QStringLiteral("50 fps"), 50.0);
    m_fpsCombo->addItem(QStringLiteral("60 fps"), 60.0);
    m_fpsCombo->addItem(QStringLiteral("120 fps"), 120.0);
    controlsLayout->addWidget(m_fpsCombo);

    // Loop Mode Button (Loop vs Once)
    m_loopButton = new QPushButton(QStringLiteral("🔁 Loop"), this);
    m_loopButton->setObjectName(QStringLiteral("timelineLoopButton"));
    m_loopButton->setCheckable(true);
    m_loopButton->setChecked(true);
    m_loopButton->setFixedHeight(30);
    m_loopButton->setMinimumWidth(72);
    m_loopButton->setToolTip(tr("Toggle Loop Mode (Loop continuous vs Play once)"));
    controlsLayout->addWidget(m_loopButton);

    mainLayout->addLayout(controlsLayout);

    // Disable transport initially if controller has no sequence
    updateTransportButtonsState();
    show();
}

void TimelineDock::wireSignals()
{
    // --- 1. UI Controls -> PlaybackController ---
    connect(m_playPauseButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->togglePlay();
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->stop();
    });
    connect(m_stepBackButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->stepBackward();
    });
    connect(m_stepForwardButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->stepForward();
    });
    connect(m_firstFrameButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->goToFirstFrame();
    });
    connect(m_lastFrameButton, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->goToLastFrame();
    });

    // Slider scrubbing: continuous real-time scrubbing
    connect(m_slider, &QSlider::sliderMoved, this, &TimelineDock::onSliderMoved);
    connect(m_slider, &QSlider::valueChanged, this, &TimelineDock::onSliderValueChanged);

    // Frame spinbox editing
    connect(m_frameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimelineDock::onFrameSpinBoxChanged);

    // FPS combo
    connect(m_fpsCombo, QOverload<int>::of(&QComboBox::activated), this, &TimelineDock::onFpsComboActivated);

    // Loop toggle
    connect(m_loopButton, &QPushButton::clicked, this, &TimelineDock::onLoopButtonClicked);

    // --- 2. PlaybackController -> TimelineDock ---
    if (m_controller) {
        connect(m_controller, &PlaybackController::frameChanged, this, &TimelineDock::onFrameChanged);
        connect(m_controller, &PlaybackController::playStateChanged, this, &TimelineDock::onPlayStateChanged);
        connect(m_controller, &PlaybackController::fpsChanged, this, &TimelineDock::onFpsChanged);
        connect(m_controller, &PlaybackController::loopModeChanged, this, &TimelineDock::onLoopModeChanged);

        // Sequence Model hooks
        if (auto* seq = m_controller->sequence()) {
            connect(seq, &SequenceModel::sequenceLoaded, this, &TimelineDock::onSequenceLoaded);
            connect(seq, &SequenceModel::sequenceCleared, this, &TimelineDock::onSequenceCleared);
        }

        updateFpsCombo(m_controller->fps());
        updateLoopButton(m_controller->loopMode());
        updatePlayPauseIcon(m_controller->playState());
    }

    // RAM Cache indicator hook: refresh cached bar on cache loads
    if (m_vm) {
        connect(&m_vm->cache(), &ImageCache::loaded, this, &TimelineDock::updateCacheIndicator);
    }
}

namespace {
bool widgetOrChildHasFocus(QWidget* w)
{
    if (!w) return false;
    if (w->hasFocus()) return true;

    QWidget* fw = QApplication::focusWidget();
    if (fw && (fw == w || w->isAncestorOf(fw))) return true;

    QWidget* winFw = w->window() ? w->window()->focusWidget() : nullptr;
    if (winFw && (winFw == w || w->isAncestorOf(winFw))) return true;

    auto* le = w->findChild<QLineEdit*>();
    if (le && (le->hasFocus() || le == winFw || (fw && le == fw))) return true;

    return false;
}
} // namespace

void TimelineDock::setupShortcuts()
{
    // Shortcuts are now handled globally by MainWindow's QActions
    // to avoid Qt Ambiguous Shortcut errors which break playback.
}

void TimelineDock::installShortcutsOn(QWidget* target)
{
    if (!target)
        return;

    new QShortcut(QKeySequence(Qt::Key_Home), target, [this]() {
        if (m_controller) m_controller->goToFirstFrame();
    }, Qt::WidgetWithChildrenShortcut);

    new QShortcut(QKeySequence(Qt::Key_End), target, [this]() {
        if (m_controller) m_controller->goToLastFrame();
    }, Qt::WidgetWithChildrenShortcut);
}

void TimelineDock::keyPressEvent(QKeyEvent* event)
{
    // If spinbox or combo has direct focus, allow standard text navigation
    if (widgetOrChildHasFocus(m_frameSpinBox) || widgetOrChildHasFocus(m_fpsCombo)) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (m_controller) {
        switch (event->key()) {
        case Qt::Key_Right:
            m_controller->stepForward();
            event->accept();
            return;
        case Qt::Key_Left:
            m_controller->stepBackward();
            event->accept();
            return;
        case Qt::Key_Home:
            m_controller->goToFirstFrame();
            event->accept();
            return;
        case Qt::Key_End:
            m_controller->goToLastFrame();
            event->accept();
            return;
        case Qt::Key_Space:
            m_controller->togglePlay();
            event->accept();
            return;
        default:
            break;
        }
    }

    QWidget::keyPressEvent(event);
}

void TimelineDock::updateUiForSequence()
{
    if (!m_controller) {
        updateTransportButtonsState();
        return;
    }

    auto* seq = m_controller->sequence();
    if (seq) {
        // Ensure sequence model signals are hooked
        disconnect(seq, &SequenceModel::sequenceLoaded, this, &TimelineDock::onSequenceLoaded);
        disconnect(seq, &SequenceModel::sequenceCleared, this, &TimelineDock::onSequenceCleared);
        connect(seq, &SequenceModel::sequenceLoaded, this, &TimelineDock::onSequenceLoaded);
        connect(seq, &SequenceModel::sequenceCleared, this, &TimelineDock::onSequenceCleared);
    }

    const bool hasSequence = (seq != nullptr && seq->frameCount() > 0);
    const int count = hasSequence ? seq->frameCount() : 0;

    m_isUpdatingFromEngine = true;

    if (hasSequence) {
        m_slider->setEnabled(count > 1);
        m_slider->setRange(0, std::max(0, count - 1));
        m_slider->setValue(m_controller->currentIndex());

        const int startFrame = seq->startFrame();
        const int endFrame = seq->endFrame();
        const int curFrame = m_controller->currentFrameNumber();

        m_frameSpinBox->setEnabled(true);
        m_frameSpinBox->setRange(startFrame, endFrame);
        const bool validFrame = (curFrame >= startFrame && curFrame <= endFrame);
        const int targetFrame = validFrame ? curFrame : startFrame;
        if (m_frameSpinBox->value() != targetFrame) {
            QSignalBlocker blocker(m_frameSpinBox);
            m_frameSpinBox->setValue(targetFrame);
        }

        // Update range label
        m_frameRangeLabel->setText(QStringLiteral("/ %1 [%2..%3] (%4 fr)")
                                       .arg(count)
                                       .arg(startFrame)
                                       .arg(endFrame)
                                       .arg(count));

        // Missing frames analysis
        const QList<int> missing = seq->missingFrames();
        if (missing.isEmpty()) {
            m_missingBadge->setText(QStringLiteral("✔ Contiguous"));
            m_missingBadge->setStyleSheet(QStringLiteral(
                "QLabel { color: #4ade80; background: rgba(34, 197, 94, 0.15);"
                " border: 1px solid rgba(34, 197, 94, 0.4); border-radius: 4px; padding: 1px 6px; font-size: 13px; }"));
            m_missingBadge->setToolTip(tr("Sequence has no missing frames."));
            m_slider->clearMissingFrames();
        } else {
            m_missingBadge->setText(tr("⚠ %1 Missing").arg(missing.size()));
            m_missingBadge->setStyleSheet(QStringLiteral(
                "QLabel { color: #f87171; background: rgba(239, 68, 68, 0.18);"
                " border: 1px solid rgba(239, 68, 68, 0.45); border-radius: 4px; padding: 1px 6px; font-size: 13px; font-weight: bold; }"));

            // Build detailed tooltip listing missing frames
            QStringList missingStr;
            const int limit = std::min(static_cast<int>(missing.size()), 12);
            for (int i = 0; i < limit; ++i) {
                missingStr << QString::number(missing[i]);
            }
            if (missing.size() > limit) {
                missingStr << tr("... (+%1 more)").arg(missing.size() - limit);
            }
            m_missingBadge->setToolTip(tr("Missing frame numbers:\n%1").arg(missingStr.join(QStringLiteral(", "))));

            m_slider->setMissingFrames(missing, startFrame, endFrame);
        }
        m_missingBadge->show();
    } else {
        m_slider->setEnabled(false);
        m_slider->setRange(0, 0);
        m_slider->setValue(0);
        m_slider->clearMissingFrames();

        m_frameSpinBox->setEnabled(false);
        m_frameSpinBox->setRange(0, 0);
        m_frameSpinBox->setValue(0);

        m_frameRangeLabel->setText(QStringLiteral("0 / 0"));
        m_missingBadge->hide();
    }

    m_isUpdatingFromEngine = false;

    updateTransportButtonsState();
}

void TimelineDock::updateTransportButtonsState()
{
    const bool hasFrames = (m_controller && m_controller->sequence() && m_controller->sequence()->frameCount() > 0);
    const bool canStep = (m_controller && m_controller->sequence() && m_controller->sequence()->frameCount() > 1);

    m_firstFrameButton->setEnabled(hasFrames);
    m_stepBackButton->setEnabled(canStep);
    m_playPauseButton->setEnabled(canStep);
    m_stopButton->setEnabled(hasFrames);
    m_stepForwardButton->setEnabled(canStep);
    m_lastFrameButton->setEnabled(hasFrames);
}

void TimelineDock::updatePlayPauseIcon(PlaybackController::PlayState state)
{
    if (state == PlaybackController::PlayState::Playing) {
        m_playPauseButton->setText(QStringLiteral("⏸"));
        m_playPauseButton->setToolTip(tr("Pause (Space)"));
    } else {
        m_playPauseButton->setText(QStringLiteral("▶"));
        m_playPauseButton->setToolTip(tr("Play (Space)"));
    }
}

void TimelineDock::updateFpsCombo(double fps)
{
    int bestIdx = -1;
    double bestDiff = 0.005; // Tight epsilon to distinguish 29.97 from 30.0
    for (int i = 0; i < m_fpsCombo->count(); ++i) {
        const double diff = std::abs(m_fpsCombo->itemData(i).toDouble() - fps);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0) {
        m_fpsCombo->setCurrentIndex(bestIdx);
        return;
    }
    // Custom FPS not in standard list
    const QString customText = QStringLiteral("%1 fps").arg(fps, 0, 'f', 2);
    m_fpsCombo->setEditText(customText);
}

void TimelineDock::updateLoopButton(PlaybackController::LoopMode mode)
{
    if (mode == PlaybackController::LoopMode::Loop) {
        m_loopButton->setText(QStringLiteral("🔁 Loop"));
        m_loopButton->setChecked(true);
        m_loopButton->setToolTip(tr("Loop Mode: Loop playback continuously"));
    } else {
        m_loopButton->setText(QStringLiteral("➡ Once"));
        m_loopButton->setChecked(false);
        m_loopButton->setToolTip(tr("Loop Mode: Play once and stop at last frame"));
    }
}

double TimelineDock::parseFpsFromText(const QString& text) const
{
    QString cleaned = text;
    cleaned = cleaned.remove(QStringLiteral("fps"), Qt::CaseInsensitive).trimmed();
    bool ok = false;
    double val = cleaned.toDouble(&ok);
    return (ok && val > 0.0) ? val : 24.0;
}

// =============================================================================
// Engine Signals Slots
// =============================================================================

void TimelineDock::onFrameChanged(int index, const QString& path)
{
    if (!m_controller)
        return;

    auto* seq = m_controller->sequence();
    if (seq) {
        const int expectedMax = std::max(0, seq->frameCount() - 1);
        if (m_slider->maximum() != expectedMax || m_frameSpinBox->maximum() != seq->endFrame()) {
            updateUiForSequence();
        }
    }

    // Synchronize ViewerViewModel if bound
    if (m_vm && !path.isEmpty()) {
        m_vm->showFrame(path);
    }

    // Guard against feedback loop and avoid fighting the mouse if user is scrubbing
    m_isUpdatingFromEngine = true;

    if (!m_slider->isSliderDown()) {
        QSignalBlocker blocker(m_slider);
        m_slider->setValue(index);
    }

    const int frameNum = m_controller->currentFrameNumber();
    const bool validFrame = seq ? (frameNum >= seq->startFrame() && frameNum <= seq->endFrame()) : (frameNum >= 0);
    if (validFrame && m_frameSpinBox->value() != frameNum) {
        QSignalBlocker blocker(m_frameSpinBox);
        m_frameSpinBox->setValue(frameNum);
    }

    if (seq) {
        m_frameRangeLabel->setText(QStringLiteral("/ %1 [%2..%3] (%4 fr)")
                                       .arg(seq->frameCount())
                                       .arg(seq->startFrame())
                                       .arg(seq->endFrame())
                                       .arg(seq->frameCount()));
    }

    m_isUpdatingFromEngine = false;

    updateCacheIndicator();
}

void TimelineDock::onPlayStateChanged(PlaybackController::PlayState state)
{
    updatePlayPauseIcon(state);
}

void TimelineDock::onFpsChanged(double fps)
{
    m_isUpdatingFromEngine = true;
    updateFpsCombo(fps);
    m_isUpdatingFromEngine = false;
}

void TimelineDock::onLoopModeChanged(PlaybackController::LoopMode mode)
{
    m_isUpdatingFromEngine = true;
    updateLoopButton(mode);
    m_isUpdatingFromEngine = false;
}

void TimelineDock::onSequenceLoaded()
{
    updateUiForSequence();
    updateCacheIndicator();
}

void TimelineDock::onSequenceCleared()
{
    updateUiForSequence();
    if (m_slider) {
        m_slider->clearCachedFrames();
    }
}

void TimelineDock::updateCacheIndicator()
{
    if (!m_controller || !m_vm || !m_slider)
        return;

    auto* seq = m_controller->sequence();
    if (!seq || seq->frameCount() == 0) {
        m_slider->clearCachedFrames();
        return;
    }

    const int count = seq->frameCount();
    QList<bool> cached;
    cached.reserve(count);
    for (int i = 0; i < count; ++i) {
        cached.append(m_vm->cache().isCached(seq->framePath(i)));
    }
    m_slider->setCachedFrames(cached);
}

// =============================================================================
// UI Interactive Slots
// =============================================================================

void TimelineDock::onSliderMoved(int index)
{
    if (m_isUpdatingFromEngine || !m_controller)
        return;

    m_controller->setCurrentIndex(index);
}

void TimelineDock::onSliderValueChanged(int index)
{
    // When value changes due to mouse click or keyboard on slider
    if (m_isUpdatingFromEngine || !m_controller)
        return;

    if (m_slider->isSliderDown() || m_slider->hasFocus()) {
        m_controller->setCurrentIndex(index);
    }
}

void TimelineDock::onFrameSpinBoxChanged(int frameNumber)
{
    if (m_isUpdatingFromEngine || !m_controller)
        return;

    auto* seq = m_controller->sequence();
    if (!seq || seq->frameCount() == 0)
        return;

    const int idx = seq->indexOfFrame(frameNumber);
    if (idx >= 0) {
        m_controller->setCurrentIndex(idx);
    }
}

void TimelineDock::onFpsComboActivated(int comboIndex)
{
    if (m_isUpdatingFromEngine || !m_controller)
        return;

    double fps = m_fpsCombo->itemData(comboIndex).toDouble();
    if (fps <= 0.0) {
        fps = parseFpsFromText(m_fpsCombo->currentText());
    }
    m_controller->setFps(fps);
}

void TimelineDock::onLoopButtonClicked()
{
    if (!m_controller)
        return;

    const auto nextMode = (m_controller->loopMode() == PlaybackController::LoopMode::Loop)
                              ? PlaybackController::LoopMode::Once
                              : PlaybackController::LoopMode::Loop;
    m_controller->setLoopMode(nextMode);
}

// =============================================================================
// QtADS Factory Helper
// =============================================================================

ads::CDockWidget* TimelineDock::createDockWidget(ads::CDockManager* dockManager,
                                                 PlaybackController* controller,
                                                 ViewerViewModel* vm,
                                                 QWidget* parent)
{
    if (!dockManager)
        return nullptr;

    auto* content = new TimelineDock(controller, vm, parent ? parent : dockManager);
    auto* dockWidget = new ads::CDockWidget(dockManager, QStringLiteral("Timeline"));
    dockWidget->setObjectName(QStringLiteral("TimelineDockWidget"));
    dockWidget->setWidget(content);
    dockWidget->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);
    return dockWidget;
}

QSize TimelineDock::sizeHint() const
{
    return QSize(800, 90);
}

QSize TimelineDock::minimumSizeHint() const
{
    return QSize(400, 90);
}

}  // namespace sview
