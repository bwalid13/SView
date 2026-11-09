#include "CustomTitleBar.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QWindow>

namespace sview {

CustomTitleBar::CustomTitleBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("customTitleBar"));
    setFixedHeight(32);
    setupUi();
    updateThemeStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeApplied, this, [this]() {
        updateThemeStyle();
    });
}

void CustomTitleBar::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(4);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName(QStringLiteral("titleIcon"));
    m_iconLabel->setFixedSize(18, 18);
    m_iconLabel->setScaledContents(true);
    QIcon appIcon = window() ? window()->windowIcon() : QApplication::windowIcon();
    if (!appIcon.isNull()) {
        m_iconLabel->setPixmap(appIcon.pixmap(18, 18));
    }
    layout->addWidget(m_iconLabel);

    m_titleLabel = new QLabel(QStringLiteral("SView"), this);
    m_titleLabel->setObjectName(QStringLiteral("titleText"));
    layout->addWidget(m_titleLabel);

    layout->addStretch(1);

    m_minBtn = new QPushButton(QString::fromUtf8("—"), this);
    m_minBtn->setObjectName(QStringLiteral("titleMinBtn"));
    m_minBtn->setFixedSize(38, 32);
    m_minBtn->setToolTip(tr("Minimize"));
    layout->addWidget(m_minBtn);

    m_maxBtn = new QPushButton(QString::fromUtf8("□"), this);
    m_maxBtn->setObjectName(QStringLiteral("titleMaxBtn"));
    m_maxBtn->setFixedSize(38, 32);
    m_maxBtn->setToolTip(tr("Maximize / Restore"));
    layout->addWidget(m_maxBtn);

    m_closeBtn = new QPushButton(QString::fromUtf8("✕"), this);
    m_closeBtn->setObjectName(QStringLiteral("titleCloseBtn"));
    m_closeBtn->setFixedSize(38, 32);
    m_closeBtn->setToolTip(tr("Close"));
    layout->addWidget(m_closeBtn);

    connect(m_minBtn, &QPushButton::clicked, this, [this]() {
        emit minimizeRequested();
    });

    connect(m_maxBtn, &QPushButton::clicked, this, [this]() {
        emit maximizeRequested();
    });

    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit closeRequested();
    });
}

void CustomTitleBar::setTitle(const QString& title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

QString CustomTitleBar::title() const
{
    return m_titleLabel ? m_titleLabel->text() : QString();
}

void CustomTitleBar::updateMaximizeButtonState()
{
    if (!m_maxBtn || !window())
        return;
    if (window()->isMaximized()) {
        m_maxBtn->setText(QString::fromUtf8("❐"));
        m_maxBtn->setToolTip(tr("Restore Down"));
    } else {
        m_maxBtn->setText(QString::fromUtf8("□"));
        m_maxBtn->setToolTip(tr("Maximize"));
    }
}

void CustomTitleBar::updateThemeStyle()
{
    const auto currentTheme = ThemeManager::currentTheme();
    const auto tokens = ThemeManager::paletteTokens(currentTheme);
    const QString bgCss = (currentTheme == ThemeManager::Theme::Houdini)
        ? QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #444444, stop:1 #2d2d2d)")
        : tokens.bgTitleBar.name();
    const QColor textCol = tokens.text;

    QString qss = QStringLiteral(
        "#customTitleBar {"
        "  background: %1;"
        "  border-bottom: 1px solid rgba(0, 0, 0, 0.4);"
        "}"
        "#titleText {"
        "  color: %2;"
        "  font-weight: 500;"
        "  font-size: 12px;"
        "  padding-left: 4px;"
        "}"
        "#titleMinBtn, #titleMaxBtn {"
        "  background: transparent;"
        "  border: none;"
        "  color: %2;"
        "  font-size: 12px;"
        "}"
        "#titleMinBtn:hover, #titleMaxBtn:hover {"
        "  background: rgba(255, 255, 255, 0.15);"
        "}"
        "#titleMinBtn:pressed, #titleMaxBtn:pressed {"
        "  background: rgba(255, 255, 255, 0.25);"
        "}"
        "#titleCloseBtn {"
        "  background: transparent;"
        "  border: none;"
        "  color: %2;"
        "  font-size: 12px;"
        "}"
        "#titleCloseBtn:hover {"
        "  background: #e81123;"
        "  color: #ffffff;"
        "}"
        "#titleCloseBtn:pressed {"
        "  background: #b80d19;"
        "  color: #ffffff;"
        "}"
    ).arg(bgCss, textCol.name());

    setStyleSheet(qss);
    updateMaximizeButtonState();
}

void CustomTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (window() && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (window()) {
            if (window()->isMaximized()) {
                window()->showNormal();
            } else {
                window()->showMaximized();
            }
            updateMaximizeButtonState();
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

}  // namespace sview
