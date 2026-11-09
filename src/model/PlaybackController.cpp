#include "PlaybackController.h"

#include <algorithm>
#include <cmath>

namespace sview {

PlaybackController::PlaybackController(QObject* parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    updateTimerInterval();
    connect(&m_timer, &QTimer::timeout, this, &PlaybackController::onTimerTick);
}

void PlaybackController::setSequence(SequenceModel* seq)
{
    if (m_sequence == seq)
        return;

    if (m_state == PlayState::Playing) {
        pause();
    }

    if (m_sequence) {
        disconnect(m_sequence, nullptr, this, nullptr);
    }

    m_sequence = seq;
    m_currentIndex = 0;

    if (m_sequence) {
        connect(m_sequence, &SequenceModel::sequenceLoaded, this, &PlaybackController::onSequenceLoaded);
        connect(m_sequence, &SequenceModel::sequenceCleared, this, &PlaybackController::onSequenceCleared);
    }

    if (m_sequence && m_sequence->frameCount() > 0) {
        emit frameChanged(0, m_sequence->framePath(0));
        triggerPrefetch();
    }
}

SequenceModel* PlaybackController::sequence() const
{
    return m_sequence.data();
}

void PlaybackController::setImageCache(ImageCache* cache)
{
    m_cache = cache;
}

ImageCache* PlaybackController::imageCache() const
{
    return m_cache.data();
}

void PlaybackController::updateTimerInterval()
{
    const double safeFps = (m_fps > 0.0) ? m_fps : 24.0;
    const int intervalMs = std::max(1, static_cast<int>(std::round(1000.0 / safeFps)));
    m_timer.setInterval(intervalMs);
}

void PlaybackController::setFps(double fps)
{
    if (fps <= 0.0)
        fps = 24.0;

    if (std::abs(m_fps - fps) > 0.001) {
        m_fps = fps;
        updateTimerInterval();
        emit fpsChanged(m_fps);
    }
}

double PlaybackController::fps() const
{
    return m_fps;
}

int PlaybackController::timerIntervalMs() const
{
    return m_timer.interval();
}

void PlaybackController::setLoopMode(LoopMode mode)
{
    if (m_loopMode != mode) {
        m_loopMode = mode;
        emit loopModeChanged(m_loopMode);
    }
}

PlaybackController::LoopMode PlaybackController::loopMode() const
{
    return m_loopMode;
}

void PlaybackController::setLookaheadCount(int count)
{
    m_lookaheadCount = std::clamp(count, 1, 10);
}

int PlaybackController::lookaheadCount() const
{
    return m_lookaheadCount;
}

PlaybackController::PlayState PlaybackController::playState() const
{
    return m_state;
}

bool PlaybackController::isPlaying() const
{
    return m_state == PlayState::Playing;
}

int PlaybackController::currentIndex() const
{
    return m_currentIndex;
}

int PlaybackController::currentFrameNumber() const
{
    if (m_sequence && m_sequence->frameCount() > 0)
        return m_sequence->frameNumber(m_currentIndex);
    return -1;
}

QString PlaybackController::currentFramePath() const
{
    if (m_sequence && m_sequence->frameCount() > 0)
        return m_sequence->framePath(m_currentIndex);
    return {};
}

void PlaybackController::play()
{
    if (!m_sequence || m_sequence->frameCount() == 0)
        return;

    // In Once mode, if playback was at the end, restart from beginning
    if (m_loopMode == LoopMode::Once && m_currentIndex >= m_sequence->frameCount() - 1) {
        m_currentIndex = 0;
        emit frameChanged(0, m_sequence->framePath(0));
    }

    if (m_state != PlayState::Playing) {
        m_state = PlayState::Playing;
        m_timer.start();
        emit playStateChanged(PlayState::Playing);
        triggerPrefetch();
    }
}

void PlaybackController::pause()
{
    if (m_state == PlayState::Playing) {
        m_timer.stop();
        m_state = PlayState::Paused;
        emit playStateChanged(PlayState::Paused);
    }
}

void PlaybackController::stop()
{
    m_timer.stop();
    const bool stateChanged = (m_state != PlayState::Stopped);
    m_state = PlayState::Stopped;

    if (m_sequence && m_sequence->frameCount() > 0) {
        m_currentIndex = 0;
        emit frameChanged(0, m_sequence->framePath(0));
    }

    if (stateChanged) {
        emit playStateChanged(PlayState::Stopped);
    }
}

void PlaybackController::togglePlay()
{
    if (m_state == PlayState::Playing)
        pause();
    else
        play();
}

void PlaybackController::stepForward()
{
    if (!m_sequence || m_sequence->frameCount() == 0)
        return;

    if (m_state == PlayState::Playing) {
        pause();
    }

    const int count = m_sequence->frameCount();
    int next = m_currentIndex + 1;
    if (next >= count) {
        if (m_loopMode == LoopMode::Loop)
            next = 0;
        else
            next = count - 1;
    }
    setCurrentIndex(next);
}

void PlaybackController::stepBackward()
{
    if (!m_sequence || m_sequence->frameCount() == 0)
        return;

    if (m_state == PlayState::Playing) {
        pause();
    }

    const int count = m_sequence->frameCount();
    int prev = m_currentIndex - 1;
    if (prev < 0) {
        if (m_loopMode == LoopMode::Loop)
            prev = count - 1;
        else
            prev = 0;
    }
    setCurrentIndex(prev);
}

void PlaybackController::setCurrentIndex(int index)
{
    if (!m_sequence || m_sequence->frameCount() == 0) {
        m_currentIndex = 0;
        return;
    }

    const int count = m_sequence->frameCount();
    const int clamped = std::clamp(index, 0, count - 1);

    if (m_currentIndex != clamped || m_state == PlayState::Stopped) {
        m_currentIndex = clamped;
        emit frameChanged(m_currentIndex, m_sequence->framePath(m_currentIndex));
        triggerPrefetch();
    }
}

void PlaybackController::goToFirstFrame()
{
    setCurrentIndex(0);
}

void PlaybackController::goToLastFrame()
{
    if (m_sequence && m_sequence->frameCount() > 0)
        setCurrentIndex(m_sequence->frameCount() - 1);
}

void PlaybackController::onTimerTick()
{
    if (!m_sequence || m_sequence->frameCount() <= 1)
        return;

    const int count = m_sequence->frameCount();
    const int next = m_currentIndex + 1;

    if (next >= count) {
        if (m_loopMode == LoopMode::Loop) {
            setCurrentIndex(0);
        } else {
            setCurrentIndex(count - 1);
            pause();
        }
    } else {
        setCurrentIndex(next);
    }
}

void PlaybackController::onSequenceLoaded()
{
    m_currentIndex = 0;
    if (m_sequence && m_sequence->frameCount() > 0) {
        emit frameChanged(0, m_sequence->framePath(0));
        triggerPrefetch();
    }
}

void PlaybackController::onSequenceCleared()
{
    stop();
    m_currentIndex = 0;
}

void PlaybackController::triggerPrefetch()
{
    if (!m_sequence || m_sequence->frameCount() <= 1)
        return;

    const int count = m_sequence->frameCount();
    QStringList paths;
    paths.reserve(m_lookaheadCount);

    for (int i = 1; i <= m_lookaheadCount; ++i) {
        int targetIdx = m_currentIndex + i;
        if (targetIdx >= count) {
            if (m_loopMode == LoopMode::Loop) {
                targetIdx = targetIdx % count;
            } else {
                break;
            }
        }
        const QString p = m_sequence->framePath(targetIdx);
        if (!p.isEmpty() && !paths.contains(p)) {
            paths.append(p);
        }
    }

    emit prefetchRequested(paths);

    if (m_cache && !paths.isEmpty()) {
        m_cache->prefetch(paths);
    }
}

}  // namespace sview
