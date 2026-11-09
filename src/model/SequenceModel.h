#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace sview {

struct FrameItem {
    int frameNumber = 0;
    QString filePath;
};

enum class TokenFormat {
    Hash,      // e.g. "shot.####.exr"
    Printf,    // e.g. "shot.%04d.exr"
    DollarF    // e.g. "shot.$F4.exr"
};

struct SequenceInfo {
    bool isSequence = false;
    QString prefix;
    QString secondarySuffix;
    QString extension;
    int padding = 0;
    int startFrame = 0;
    int endFrame = 0;
    int frameCount = 0;
};

// Represents a sequence of numbered frames (or a 1-frame degenerate sequence for single images).
// Automatically parses frame numbers from filename patterns like 'render.0001.exr'.
class SequenceModel : public QObject {
    Q_OBJECT
public:
    explicit SequenceModel(QObject* parent = nullptr);
    ~SequenceModel() override = default;

    // Detects sequence pattern from filePath and scans its containing directory for matching frames.
    // Single images without frame numbers are cleanly loaded as a 1-frame degenerate sequence.
    bool detectSequence(const QString& filePath);

    // Clears all sequence information.
    void clear();

    // Sets frames explicitly (for programmatic / testing purposes)
    void setFrames(const QList<FrameItem>& frames);

    // Sequence properties
    bool isSequence() const;
    QString prefix() const;
    QString secondarySuffix() const;
    QString extension() const;
    int padding() const;
    int startFrame() const;
    int endFrame() const;
    int frameCount() const;
    bool isEmpty() const;
    SequenceInfo sequenceInfo() const;

    // Frame queries
    QString framePath(int index) const;
    int frameNumber(int index) const;
    int indexOfFrame(int frameNumber) const;
    int indexOfPath(const QString& path) const;
    QList<int> missingFrames() const;

    // Direct access to all frame items
    const QList<FrameItem>& frames() const;

    // Formatted representation (supports Hash, Printf, and DollarF)
    QString formatPattern(TokenFormat format = TokenFormat::Hash) const;

signals:
    void sequenceLoaded();
    void sequenceCleared();

private:
    void setupSingleImage(const QString& filePath, int frameNumber = 1);

    QList<FrameItem> m_frames;
    QString m_prefix;
    QString m_secondarySuffix;
    QString m_extension;
    int m_padding = 0;
    bool m_isSequence = false;
};

}  // namespace sview
