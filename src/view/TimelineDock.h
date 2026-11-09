#pragma once

#include "../model/PlaybackController.h"
#include "../model/SequenceModel.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace sview {

class ViewerViewModel;

// -----------------------------------------------------------------------------
// TimelineSlider: High-precision horizontal scrubber slider
// Features:
//   - Click-to-jump: clicking anywhere on the groove instantly relocates playhead
//   - Drag-scrub: real-time smooth scrubbing with continuous sliderMoved signals
//   - Missing frames visualizer: renders red/amber hash marks along groove
//   - Wheel scrub: mouse wheel over slider steps forward/backward
// -----------------------------------------------------------------------------
class TimelineSlider : public QSlider {
    Q_OBJECT
public:
    explicit TimelineSlider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    ~TimelineSlider() override = default;

    void setMissingFrames(const QList<int>& missing, int startFrame, int endFrame);
    void clearMissingFrames();
    const QList<int>& missingFrames() const { return m_missingFrames; }
    int startFrame() const { return m_startFrame; }
    int endFrame() const { return m_endFrame; }

    void setCachedFrames(const QList<bool>& cached);
    void clearCachedFrames();
    const QList<bool>& cachedFrames() const { return m_cachedFrames; }

    int pixelPosToValue(int posX) const;

signals:
    void frameJumped(int frameIndex);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QList<int> m_missingFrames;
    QList<bool> m_cachedFrames;
    int m_startFrame = 0;
    int m_endFrame = 0;
};

// -----------------------------------------------------------------------------
// TimelineDock: Transport controls and sequence scrubber dock widget
// Coordinates bidirectional synchronization between:
//   - PlaybackController (transport engine, timer, FPS, loop mode)
//   - SequenceModel (frame list, frame numbers, missing frame gaps)
//   - ViewerViewModel (image display, cache lookahead requests)
// -----------------------------------------------------------------------------
class TimelineDock : public QWidget {
    Q_OBJECT
public:
    explicit TimelineDock(PlaybackController* controller,
                          ViewerViewModel* vm = nullptr,
                          QWidget* parent = nullptr);
    ~TimelineDock() override = default;

    // Direct access to controls (for tests, custom styling, or layout hooks)
    TimelineSlider* slider() const { return m_slider; }
    TimelineSlider* timelineSlider() const { return m_slider; }
    QPushButton* playPauseButton() const { return m_playPauseButton; }
    QPushButton* stopButton() const { return m_stopButton; }
    QPushButton* stepBackButton() const { return m_stepBackButton; }
    QPushButton* stepForwardButton() const { return m_stepForwardButton; }
    QPushButton* firstFrameButton() const { return m_firstFrameButton; }
    QPushButton* lastFrameButton() const { return m_lastFrameButton; }
    QSpinBox* frameSpinBox() const { return m_frameSpinBox; }
    QLabel* frameRangeLabel() const { return m_frameRangeLabel; }
    QLabel* missingBadge() const { return m_missingBadge; }
    QComboBox* fpsComboBox() const { return m_fpsCombo; }
    QPushButton* loopButton() const { return m_loopButton; }

    PlaybackController* controller() const { return m_controller; }
    ViewerViewModel* viewModel() const { return m_vm; }

    // Helper for QtADS integration
    static ads::CDockWidget* createDockWidget(ads::CDockManager* dockManager,
                                              PlaybackController* controller,
                                              ViewerViewModel* vm = nullptr,
                                              QWidget* parent = nullptr);

    // Install application/window-wide playback shortcuts on a target widget
    void installShortcutsOn(QWidget* target);

    // Updates the RAM cache indicator bar along the scrubber groove
    void updateCacheIndicator();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // Slots responding to PlaybackController signals
    void onFrameChanged(int index, const QString& path);
    void onPlayStateChanged(PlaybackController::PlayState state);
    void onFpsChanged(double fps);
    void onLoopModeChanged(PlaybackController::LoopMode mode);

    // Slots responding to SequenceModel signals
    void onSequenceLoaded();
    void onSequenceCleared();

    // UI interactive slots
    void onSliderMoved(int index);
    void onSliderValueChanged(int index);
    void onFrameSpinBoxChanged(int frameNumber);
    void onFpsComboActivated(int comboIndex);
    void onLoopButtonClicked();

private:
    void setupUi();
    void setupShortcuts();
    void wireSignals();
    void updateUiForSequence();
    void updateTransportButtonsState();
    void updatePlayPauseIcon(PlaybackController::PlayState state);
    void updateFpsCombo(double fps);
    void updateLoopButton(PlaybackController::LoopMode mode);
    double parseFpsFromText(const QString& text) const;

    PlaybackController* m_controller = nullptr;
    ViewerViewModel* m_vm = nullptr;

    // UI Controls
    TimelineSlider* m_slider = nullptr;
    QPushButton* m_firstFrameButton = nullptr;
    QPushButton* m_stepBackButton = nullptr;
    QPushButton* m_playPauseButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_stepForwardButton = nullptr;
    QPushButton* m_lastFrameButton = nullptr;

    QSpinBox* m_frameSpinBox = nullptr;
    QLabel* m_frameRangeLabel = nullptr;
    QLabel* m_missingBadge = nullptr;

    QComboBox* m_fpsCombo = nullptr;
    QPushButton* m_loopButton = nullptr;

    // Recursion / feedback loop guard
    bool m_isUpdatingFromEngine = false;
};

}  // namespace sview
