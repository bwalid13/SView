#include "LoadingSpinnerWidget.h"
#include "ThemeManager.h"

#include <QColor>
#include <QPainter>
#include <QPaintEvent>

namespace sview {

LoadingSpinnerWidget::LoadingSpinnerWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("loadingSpinnerWidget"));
    setFixedSize(64, 64);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_animTimer.setInterval(16); // ~60 FPS
    connect(&m_animTimer, &QTimer::timeout, this, &LoadingSpinnerWidget::onAnimTick);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(80); // 80ms debounce
    connect(&m_debounceTimer, &QTimer::timeout, this, &LoadingSpinnerWidget::onDebounceTimeout);

    hide();
}

void LoadingSpinnerWidget::setLoading(bool loading)
{
    m_requestedLoading = loading;
    if (loading) {
        if (!m_debounceTimer.isActive() && !isSpinning()) {
            m_debounceTimer.start();
        }
    } else {
        m_debounceTimer.stop();
        stop();
    }
}

void LoadingSpinnerWidget::start()
{
    m_angle = 0;
    if (!m_animTimer.isActive()) {
        m_animTimer.start();
    }
    show();
    raise();
    update();
}

void LoadingSpinnerWidget::stop()
{
    m_debounceTimer.stop();
    m_animTimer.stop();
    hide();
}

void LoadingSpinnerWidget::onAnimTick()
{
    m_angle = (m_angle + 6) % 360;
    update();
}

void LoadingSpinnerWidget::onDebounceTimeout()
{
    if (m_requestedLoading) {
        start();
    }
}

void LoadingSpinnerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (!isVisible())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(8, 8, -8, -8);

    // Subtle dark circular background disc
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20, 180));
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));

    // Outer subtle track
    QPen trackPen(QColor(80, 80, 80, 120), 4.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(trackPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r);

    // Dynamic spinning arc using active theme accent color
    QColor accent = ThemeManager::paletteTokens(ThemeManager::currentTheme()).accent;
    if (!accent.isValid() || accent.alpha() == 0) {
        accent = QColor(0x33, 0x99, 0xff);
    }
    QPen arcPen(accent, 4.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(arcPen);
    p.drawArc(r, -m_angle * 16, 100 * 16);
}

}  // namespace sview
