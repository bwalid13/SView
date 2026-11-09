#pragma once

#include "ImageCache.h"
#include "SequenceModel.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace sview {

class PlaybackController : public QObject {
    Q_OBJECT
public:
    enum class PlayState { Stopped, Playing, Paused };
    Q_ENUM(PlayState)

    enum class LoopMode { Loop, Once };
    Q_ENUM(LoopMode)

    explicit PlaybackController(QObject* parent = nullptr);
    ~PlaybackController() override = default;

    // Sequence model binding
    void setSequence(SequenceModel* seq);
    SequenceModel* sequence() const;

    // Cache binding for lookahead prefetching
    void setImageCache(ImageCache* cache);
    ImageCache* imageCache() const;

    // FPS configuration (e.g. 24.0, 30.0, 60.0)
    void setFps(double fps);
    double fps() const;
    int timerIntervalMs() const;

    // Loop mode (Loop, Once)
    void setLoopMode(LoopMode mode);
    LoopMode loopMode() const;

    // Prefetch lookahead count (default 4 frames, clamped between 1 and 10)
    void setLookaheadCount(int count);
    int lookaheadCount() const;

    // State queries
    PlayState playState() const;
    bool isPlaying() const;
    int currentIndex() const;
    int currentFrameNumber() const;
    QString currentFramePath() const;

    // Transport controls
    void play();
    void pause();
    void stop();
    void togglePlay();

    // Single-frame stepping (keyboard navigation: Left/Right arrows)
    void stepForward();
    void stepBackward();

    // Direct scrubbing / navigation
    void setCurrentIndex(int index);
    void goToFirstFrame();
    void goToLastFrame();

signals:
    void frameChanged(int index, const QString& path);
    void playStateChanged(PlayState state);
    void fpsChanged(double fps);
    void loopModeChanged(LoopMode mode);
    void prefetchRequested(const QStringList& paths);

private slots:
    void onTimerTick();
    void onSequenceLoaded();
    void onSequenceCleared();

private:
    void updateTimerInterval();
    void triggerPrefetch();

    QTimer m_timer;
    QPointer<SequenceModel> m_sequence;
    QPointer<ImageCache> m_cache;

    PlayState m_state = PlayState::Stopped;
    LoopMode m_loopMode = LoopMode::Loop;

    double m_fps = 24.0;
    int m_currentIndex = 0;
    int m_lookaheadCount = 4;
};

}  // namespace sview
