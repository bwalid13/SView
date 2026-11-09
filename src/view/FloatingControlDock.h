#pragma once

#include "../model/PlaybackController.h"
#include "ThemeManager.h"

#include <QPointer>
#include <QSlider>
#include <QWidget>

class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;
class QPushButton;
class QLabel;

namespace sview {

class ViewerViewModel;
class ViewerWidget;

// -----------------------------------------------------------------------------
// HUDScrubberSlider: Click-to-jump high precision HUD slider
// -----------------------------------------------------------------------------
class HUDScrubberSlider : public QSlider {
    Q_OBJECT
public:
    explicit HUDScrubberSlider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    ~HUDScrubberSlider() override = default;

    int pixelPosToValue(int posX) const;

signals:
    void frameJumped(int frameIndex);

protected:
    void mousePressEvent(QMouseEvent* event) override;
};

// -----------------------------------------------------------------------------
// FloatingControlDock: Translucent MacOS-style playback HUD overlay
// -----------------------------------------------------------------------------
class FloatingControlDock : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit FloatingControlDock(QWidget* parent = nullptr);
    FloatingControlDock(QWidget* parent, PlaybackController* controller, ViewerViewModel* vm);
    ~FloatingControlDock() override = default;

    // Controller and Model bindings
    void setPlaybackController(PlaybackController* controller);
    PlaybackController* playbackController() const { return m_controller.data(); }

    void setViewModel(ViewerViewModel* vm);
    ViewerViewModel* viewModel() const { return m_vm.data(); }

    // Geometry management
    void reposition();

    // Auto-hide activity notification hooks
    void notifyUserActivity();
    void notifyKeyboardNavigation();
    void wakeUp();
    void startFadeOut();
    void stopTimers();
    void onKeyboardActivity();

    void setAutoFadeEnabled(bool enabled);
    bool isAutoFadeEnabled() const { return m_autoFadeEnabled; }
    bool isAutoHiding() const;

    // Hover & interaction queries
    bool isHovered() const;
    bool isMouseHovered() const { return isHovered(); }
    bool isInteracting() const;
    void setInteracting(bool interacting);

    // Frame range configuration (legacy no-op)
    void setFrameRange(int start, int end);

    // Opacity property accessors (for QPropertyAnimation)
    qreal opacity() const;
    void setOpacity(qreal opacity);

    // Direct access to sub-widgets
    QPushButton* menuButton() const { return m_menuBtn; }
    QPushButton* prevFileButton() const { return m_prevFileBtn; }
    QPushButton* nextFileButton() const { return m_nextFileBtn; }
    QPushButton* flipHButton() const { return m_flipHBtn; }
    QPushButton* flipVButton() const { return m_flipVBtn; }
    QPushButton* pivotButton() const { return m_pivotBtn; }
    QPushButton* mode360Button() const { return m_btn360; }

    QGraphicsOpacityEffect* graphicsEffect() const { return m_opacityEffect; }
    QPropertyAnimation* fadeAnimation() const { return m_fadeAnimation; }
    QTimer* autoHideTimer() const { return m_idleTimer; }

public slots:
    void fadeIn();
    void fadeOut();
    void resetIdleTimer();
    void setDockEnabled(bool enabled);
    void updateThemeStyle();
    void updateThemeStyle(ThemeManager::Theme theme);

signals:
    void menuButtonClicked();
    void toggleMenuRequested();
    void prevFileClicked();
    void nextFileClicked();
    void pivotClicked();
    void flipHChanged(bool flip);
    void flipVChanged(bool flip);
    void mode360Clicked(bool checked);
    void mode360Toggled(bool checked);
    void visibilityFadeChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onDisplayChanged();
    void onPrevFileClicked();
    void onNextFileClicked();
    void onPivotClicked();
    void onFlipHToggled(bool checked);
    void onFlipVToggled(bool checked);
    void onMode360Toggled(bool checked);
    void onMode360Changed(bool enabled);
    void onIdleTimeout();
    void onFadeAnimationFinished();

private:
    void setupUi();
    void setupStyling();
    void setupAnimation();

    QPointer<PlaybackController> m_controller;
    QPointer<ViewerViewModel> m_vm;

    // Controls
    QPushButton* m_menuBtn = nullptr;
    QPushButton* m_prevFileBtn = nullptr;
    QPushButton* m_nextFileBtn = nullptr;
    QPushButton* m_flipHBtn = nullptr;
    QPushButton* m_flipVBtn = nullptr;
    QPushButton* m_pivotBtn = nullptr;
    QPushButton* m_btn360 = nullptr;

    // Animation & Auto-hide
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_fadeAnimation = nullptr;
    QTimer* m_idleTimer = nullptr;

    bool m_isHovered = false;
    bool m_isInteracting = false;
    bool m_autoFadeEnabled = true;
    bool m_dockEnabled = true;

    static constexpr int kDefaultWidth = 600;
    static constexpr int kDefaultHeight = 52;
    static constexpr int kBottomMargin = 20;
    static constexpr int kIdleTimeoutMs = 2500;
    static constexpr int kFadeDurationMs = 250;
};

}  // namespace sview
