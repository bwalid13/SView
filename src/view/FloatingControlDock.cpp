#include "FloatingControlDock.h"
#include "../model/PlaybackController.h"
#include "../model/SequenceModel.h"
#include "../model/SettingsModel.h"
#include "../viewmodel/ViewerViewModel.h"

#include <QCursor>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionSlider>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace sview {

// =============================================================================
// HUDScrubberSlider Implementation
// =============================================================================
HUDScrubberSlider::HUDScrubberSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(22);
}

int HUDScrubberSlider::pixelPosToValue(int posX) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    const int handleWidth = handle.width();
    const int availableWidth = groove.width() - handleWidth;
    if (availableWidth <= 0)
        return minimum();

    int clampedX = std::clamp(posX - groove.left() - handleWidth / 2, 0, availableWidth);
    const double ratio = static_cast<double>(clampedX) / static_cast<double>(availableWidth);
    return minimum() + static_cast<int>(std::round(ratio * (maximum() - minimum())));
}

void HUDScrubberSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int val = pixelPosToValue(static_cast<int>(event->position().x()));
        setValue(val);
        emit frameJumped(val);
        emit sliderMoved(val);
    }
    QSlider::mousePressEvent(event);
}

// =============================================================================
// FloatingControlDock Implementation
// =============================================================================
FloatingControlDock::FloatingControlDock(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("floatingControlDock"));
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUi();
    setupStyling();
    setupAnimation();

    connect(&ThemeManager::instance(), &ThemeManager::themeApplied,
            this, qOverload<ThemeManager::Theme>(&FloatingControlDock::updateThemeStyle));

    if (parent) {
        parent->installEventFilter(this);
    }

    setDockEnabled(SettingsModel::instance().isFloatingDockEnabled());
    reposition();
}

FloatingControlDock::FloatingControlDock(QWidget* parent, PlaybackController* controller, ViewerViewModel* vm)
    : FloatingControlDock(parent)
{
    setPlaybackController(controller);
    setViewModel(vm);
}

void FloatingControlDock::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(6);

    auto createBtn = [this](const QIcon& icon, const QString& objName, const QString& tip) {
        auto* btn = new QPushButton(this);
        btn->setObjectName(objName);
        btn->setIcon(icon);
        btn->setIconSize(QSize(22, 22));
        btn->setToolTip(tip);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    // 0. Menu Bar Toggle
    m_menuBtn = createBtn(QIcon(QStringLiteral(":/icons/menu.png")), QStringLiteral("floatingMenuBtn"), tr("Toggle Menu Bar (Alt / Ctrl+M)"));
    m_menuBtn->setFixedSize(40, 40);
    layout->addWidget(m_menuBtn);

    connect(m_menuBtn, &QPushButton::clicked, this, [this]() {
        emit menuButtonClicked();
        emit toggleMenuRequested();
    });

    auto* sep0 = new QFrame(this);
    sep0->setObjectName(QStringLiteral("floatingSeparator"));
    sep0->setFrameShape(QFrame::VLine);
    layout->addWidget(sep0);

    // 1. Navigation Cluster (Previous / Next File in folder)
    m_prevFileBtn = createBtn(QIcon(QStringLiteral(":/icons/prev_file.png")), QStringLiteral("floatingPrevFileBtn"), tr("Previous File in Folder (Left / PageUp)"));
    m_nextFileBtn = createBtn(QIcon(QStringLiteral(":/icons/next_file.png")), QStringLiteral("floatingNextFileBtn"), tr("Next File in Folder (Right / PageDown)"));

    m_prevFileBtn->setFixedSize(40, 40);
    m_nextFileBtn->setFixedSize(40, 40);

    layout->addWidget(m_prevFileBtn);
    layout->addWidget(m_nextFileBtn);

    connect(m_prevFileBtn, &QPushButton::clicked, this, &FloatingControlDock::onPrevFileClicked);
    connect(m_nextFileBtn, &QPushButton::clicked, this, &FloatingControlDock::onNextFileClicked);

    // Separator 1
    auto* sep1 = new QFrame(this);
    sep1->setObjectName(QStringLiteral("floatingSeparator"));
    sep1->setFrameShape(QFrame::VLine);
    layout->addWidget(sep1);

    // 2. Viewport Transform Cluster: Flip H, Flip V, Pivot, 360°
    m_flipHBtn = createBtn(QIcon(QStringLiteral(":/icons/flip_h.png")), QStringLiteral("floatingFlipHBtn"), tr("Flip Viewport Horizontally (⇄)"));
    m_flipHBtn->setCheckable(true);
    m_flipHBtn->setFixedSize(40, 40);

    m_flipVBtn = createBtn(QIcon(QStringLiteral(":/icons/flip_v.png")), QStringLiteral("floatingFlipVBtn"), tr("Flip Viewport Vertically (⇅)"));
    m_flipVBtn->setCheckable(true);
    m_flipVBtn->setFixedSize(40, 40);

    m_pivotBtn = createBtn(QIcon(QStringLiteral(":/icons/pivot.png")), QStringLiteral("floatingPivotBtn"), tr("Rotate 90° Clockwise (Pivot) (↻) [Ctrl+R]"));
    m_pivotBtn->setFixedSize(40, 40);

    m_btn360 = createBtn(QIcon(QStringLiteral(":/icons/mode_360.png")), QStringLiteral("floating360Btn"), tr("Toggle 360° Equirectangular View (🌐) [3]"));
    m_btn360->setCheckable(true);
    m_btn360->setFixedSize(40, 40);

    layout->addWidget(m_flipHBtn);
    layout->addWidget(m_flipVBtn);
    layout->addWidget(m_pivotBtn);
    layout->addWidget(m_btn360);

    connect(m_flipHBtn, &QPushButton::toggled, this, &FloatingControlDock::onFlipHToggled);
    connect(m_flipVBtn, &QPushButton::toggled, this, &FloatingControlDock::onFlipVToggled);
    connect(m_pivotBtn, &QPushButton::clicked, this, &FloatingControlDock::onPivotClicked);
    connect(m_btn360, &QPushButton::toggled, this, &FloatingControlDock::onMode360Toggled);

    for (auto* btn : {m_menuBtn, m_prevFileBtn, m_nextFileBtn, m_flipHBtn, m_flipVBtn, m_pivotBtn, m_btn360}) {
        if (btn) btn->installEventFilter(this);
    }
}

void FloatingControlDock::setupStyling()
{
    updateThemeStyle(ThemeManager::currentTheme());
}

void FloatingControlDock::updateThemeStyle()
{
    updateThemeStyle(ThemeManager::currentTheme());
}

void FloatingControlDock::updateThemeStyle(ThemeManager::Theme theme)
{
    const auto tokens = ThemeManager::paletteTokens(theme);

    QColor bg = tokens.bgPanel;
    bg.setAlpha(230);

    QColor border = tokens.accent;
    border.setAlpha(150);

    QColor accent = tokens.accent;
    QColor accentBg = tokens.accent;
    accentBg.setAlpha(125);

    setStyleSheet(QStringLiteral(
        "#floatingControlDock {"
        "  background-color: rgba(%1, %2, %3, %4);"
        "  border: 1px solid rgba(%5, %6, %7, %8);"
        "  border-radius: 26px;"
        "}"
        "#floatingControlDock QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 14px;"
        "  padding: 0px;"
        "  margin: 0px;"
        "  color: %15;"
        "}"
        "#floatingControlDock QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.18);"
        "}"
        "#floatingControlDock QPushButton:pressed {"
        "  background-color: rgba(255, 255, 255, 0.28);"
        "}"
        "#floatingControlDock QPushButton:checked {"
        "  background-color: rgba(%9, %10, %11, %12);"
        "  border: 1px solid %13;"
        "  color: #ffffff;"
        "}"
        "#floatingControlDock QPushButton:disabled {"
        "  opacity: 0.35;"
        "}"
        "#floatingControlDock QFrame#floatingSeparator {"
        "  background-color: rgba(255, 255, 255, 0.20);"
        "  border: none;"
        "  width: 1px;"
        "  max-width: 1px;"
        "  min-width: 1px;"
        "  margin: 8px 3px;"
        "}"
    ).arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(bg.alphaF(), 0, 'f', 2)
     .arg(border.red()).arg(border.green()).arg(border.blue()).arg(border.alphaF(), 0, 'f', 2)
     .arg(accentBg.red()).arg(accentBg.green()).arg(accentBg.blue()).arg(accentBg.alphaF(), 0, 'f', 2)
     .arg(accent.name())
     .arg(tokens.text.name()));

    update();
}

void FloatingControlDock::setupAnimation()
{
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnimation->setDuration(kFadeDurationMs);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &FloatingControlDock::onFadeAnimationFinished);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(kIdleTimeoutMs);
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, &FloatingControlDock::onIdleTimeout);
}

void FloatingControlDock::setPlaybackController(PlaybackController* controller)
{
    m_controller = controller;
}

void FloatingControlDock::setViewModel(ViewerViewModel* vm)
{
    if (m_vm == vm)
        return;

    if (m_vm) {
        disconnect(m_vm, nullptr, this, nullptr);
    }

    m_vm = vm;

    if (m_vm) {
        connect(m_vm, &ViewerViewModel::displayChanged, this, &FloatingControlDock::onDisplayChanged);
        connect(m_vm, &ViewerViewModel::mode360Changed, this, &FloatingControlDock::onMode360Changed);
        onDisplayChanged();
        onMode360Changed(m_vm->is360Mode());
    }
}

void FloatingControlDock::reposition()
{
    QWidget* p = parentWidget();
    if (!p)
        return;

    const int vW = p->width();
    const int vH = p->height();

    const int dockW = std::clamp(vW - 32, 280, kDefaultWidth);
    const int dockH = kDefaultHeight;
    const int x = (vW - dockW) / 2;
    const int y = vH - dockH - kBottomMargin;

    setGeometry(x, y, dockW, dockH);
}

qreal FloatingControlDock::opacity() const
{
    return m_opacityEffect ? m_opacityEffect->opacity() : 1.0;
}

void FloatingControlDock::setOpacity(qreal opacity)
{
    if (m_opacityEffect) {
        m_opacityEffect->setOpacity(opacity);
    }
}

bool FloatingControlDock::isHovered() const
{
    if (m_isHovered)
        return true;
    if (!isVisible())
        return false;
    return underMouse();
}

bool FloatingControlDock::isInteracting() const
{
    return m_isInteracting ||
           (m_prevFileBtn && m_prevFileBtn->isDown()) ||
           (m_nextFileBtn && m_nextFileBtn->isDown()) ||
           (m_menuBtn && m_menuBtn->isDown()) ||
           (m_flipHBtn && m_flipHBtn->isDown()) ||
           (m_flipVBtn && m_flipVBtn->isDown()) ||
           (m_pivotBtn && m_pivotBtn->isDown()) ||
           (m_btn360 && m_btn360->isDown());
}

void FloatingControlDock::setInteracting(bool interacting)
{
    m_isInteracting = interacting;
}

bool FloatingControlDock::isAutoHiding() const
{
    return (m_fadeAnimation && m_fadeAnimation->state() == QAbstractAnimation::Running &&
            m_fadeAnimation->endValue().toReal() < 0.5) || (opacity() < 0.99);
}

void FloatingControlDock::setFrameRange(int /*start*/, int /*end*/)
{
}

void FloatingControlDock::notifyUserActivity()
{
    if (!m_dockEnabled)
        return;

    if (!isVisible() || opacity() < 0.99) {
        fadeIn();
    }

    if (m_autoFadeEnabled && !isHovered() && !isInteracting()) {
        m_idleTimer->start(kIdleTimeoutMs);
    }
}

void FloatingControlDock::notifyKeyboardNavigation()
{
    if (!m_dockEnabled || !m_autoFadeEnabled)
        return;

    // Immediately fade out if cursor is not hovering directly over the dock
    if (!isHovered()) {
        m_idleTimer->stop();
        fadeOut();
    }
}

void FloatingControlDock::wakeUp()
{
    if (!m_dockEnabled)
        return;

    m_fadeAnimation->stop();
    setOpacity(1.0);
    show();
    raise();

    if (m_autoFadeEnabled && m_idleTimer) {
        m_idleTimer->start(kIdleTimeoutMs);
    }
}

void FloatingControlDock::startFadeOut()
{
    fadeOut();
}

void FloatingControlDock::stopTimers()
{
    if (m_idleTimer)
        m_idleTimer->stop();
    if (m_fadeAnimation)
        m_fadeAnimation->stop();
}

void FloatingControlDock::onKeyboardActivity()
{
    notifyKeyboardNavigation();
}

void FloatingControlDock::setAutoFadeEnabled(bool enabled)
{
    m_autoFadeEnabled = enabled;
    if (!m_autoFadeEnabled) {
        m_idleTimer->stop();
        wakeUp();
    } else {
        m_idleTimer->start(kIdleTimeoutMs);
    }
}

void FloatingControlDock::setDockEnabled(bool enabled)
{
    m_dockEnabled = enabled;
    if (!m_dockEnabled) {
        m_idleTimer->stop();
        m_fadeAnimation->stop();
        hide();
    } else {
        reposition();
        wakeUp();
    }
}

void FloatingControlDock::fadeIn()
{
    if (!m_dockEnabled)
        return;

    if (!isVisible()) {
        show();
    }
    raise();

    if (opacity() >= 0.99 && m_fadeAnimation->state() != QAbstractAnimation::Running)
        return;

    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(opacity());
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->setDuration(200);
    m_fadeAnimation->start();
}

void FloatingControlDock::fadeOut()
{
    if (!m_dockEnabled || isHovered() || isInteracting())
        return;

    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(opacity());
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->setDuration(kFadeDurationMs);
    m_fadeAnimation->start();
}

void FloatingControlDock::resetIdleTimer()
{
    if (m_dockEnabled && m_autoFadeEnabled) {
        m_idleTimer->start(kIdleTimeoutMs);
    }
}

void FloatingControlDock::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void FloatingControlDock::enterEvent(QEnterEvent* event)
{
    m_isHovered = true;
    m_idleTimer->stop();
    fadeIn();
    QWidget::enterEvent(event);
}

void FloatingControlDock::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    if (!isInteracting() && m_autoFadeEnabled) {
        m_idleTimer->start(kIdleTimeoutMs);
    }
    QWidget::leaveEvent(event);
}

void FloatingControlDock::mouseMoveEvent(QMouseEvent* event)
{
    notifyUserActivity();
    QWidget::mouseMoveEvent(event);
}

void FloatingControlDock::wheelEvent(QWheelEvent* event)
{
    // The scroll wheel must only zoom the viewport. Ignore so it propagates to ViewerWidget.
    event->ignore();
}

bool FloatingControlDock::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize) {
            reposition();
        } else if (event->type() == QEvent::MouseMove) {
            notifyUserActivity();
        } else if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            switch (ke->key()) {
            case Qt::Key_Space:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                notifyKeyboardNavigation();
                break;
            default:
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FloatingControlDock::onIdleTimeout()
{
    if (isHovered() || isInteracting()) {
        m_idleTimer->start(kIdleTimeoutMs);
        return;
    }
    fadeOut();
}

void FloatingControlDock::onFadeAnimationFinished()
{
    if (opacity() <= 0.01) {
        hide();
        emit visibilityFadeChanged(false);
    } else if (opacity() >= 0.99) {
        emit visibilityFadeChanged(true);
    }
}

void FloatingControlDock::onPrevFileClicked()
{
    emit prevFileClicked();
}

void FloatingControlDock::onNextFileClicked()
{
    emit nextFileClicked();
}

void FloatingControlDock::onPivotClicked()
{
    emit pivotClicked();
}

void FloatingControlDock::onFlipHToggled(bool checked)
{
    emit flipHChanged(checked);
    if (m_vm) {
        m_vm->setFlipH(checked);
    }
}

void FloatingControlDock::onFlipVToggled(bool checked)
{
    emit flipVChanged(checked);
    if (m_vm) {
        m_vm->setFlipV(checked);
    }
}

void FloatingControlDock::onDisplayChanged()
{
    if (!m_vm)
        return;

    QSignalBlocker bH(m_flipHBtn);
    QSignalBlocker bV(m_flipVBtn);
    m_flipHBtn->setChecked(m_vm->flipH());
    m_flipVBtn->setChecked(m_vm->flipV());
    if (m_btn360) {
        QSignalBlocker b360(m_btn360);
        m_btn360->setChecked(m_vm->is360Mode());
    }
}

void FloatingControlDock::onMode360Toggled(bool checked)
{
    emit mode360Clicked(checked);
    emit mode360Toggled(checked);
    if (m_vm) {
        m_vm->set360Mode(checked);
    }
}

void FloatingControlDock::onMode360Changed(bool enabled)
{
    if (m_btn360 && m_btn360->isChecked() != enabled) {
        QSignalBlocker b(m_btn360);
        m_btn360->setChecked(enabled);
    }
}

}  // namespace sview
