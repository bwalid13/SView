#include "SequenceModel.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace sview {

SequenceModel::SequenceModel(QObject* parent)
    : QObject(parent)
{
}

void SequenceModel::clear()
{
    m_frames.clear();
    m_prefix.clear();
    m_secondarySuffix.clear();
    m_extension.clear();
    m_padding = 0;
    m_isSequence = false;
    emit sequenceCleared();
}

void SequenceModel::setFrames(const QList<FrameItem>& frames)
{
    m_frames = frames;
    m_isSequence = (m_frames.size() > 1);
    if (!m_frames.isEmpty()) {
        const QFileInfo fi(m_frames.first().filePath);
        m_prefix = fi.completeBaseName();
        m_extension = fi.suffix().isEmpty() ? QString() : (QStringLiteral(".") + fi.suffix());
    }
    emit sequenceLoaded();
}

bool SequenceModel::detectSequence(const QString& filePath)
{
    if (filePath.isEmpty()) {
        clear();
        return false;
    }

    const QFileInfo fi(filePath);
    if (fi.isDir() || !fi.exists()) {
        clear();
        return false;
    }

    const QString dirPath = fi.absolutePath();
    const QDir dir(dirPath);
    if (!dir.exists()) {
        clear();
        return false;
    }

    // Sequence auto-detection neutralized per user directive: always open single image only
    setupSingleImage(QDir::cleanPath(fi.absoluteFilePath()), 1);
    m_isSequence = false;
    return true;
}

void SequenceModel::setupSingleImage(const QString& filePath, int frameNumber)
{
    const QFileInfo fi(filePath);
    m_frames.clear();

    FrameItem item;
    item.frameNumber = frameNumber;
    item.filePath = filePath;
    m_frames.append(item);

    m_prefix = fi.completeBaseName();
    m_secondarySuffix.clear();
    m_extension = fi.suffix().isEmpty() ? QString() : (QStringLiteral(".") + fi.suffix());
    m_padding = 0;
    m_isSequence = false;

    emit sequenceLoaded();
}

bool SequenceModel::isSequence() const
{
    return m_isSequence && m_frames.size() > 1;
}

QString SequenceModel::prefix() const
{
    return m_prefix;
}

QString SequenceModel::secondarySuffix() const
{
    return m_secondarySuffix;
}

QString SequenceModel::extension() const
{
    return m_extension;
}

int SequenceModel::padding() const
{
    return m_padding;
}

int SequenceModel::startFrame() const
{
    return m_frames.isEmpty() ? 0 : m_frames.first().frameNumber;
}

int SequenceModel::endFrame() const
{
    return m_frames.isEmpty() ? 0 : m_frames.last().frameNumber;
}

int SequenceModel::frameCount() const
{
    return static_cast<int>(m_frames.size());
}

bool SequenceModel::isEmpty() const
{
    return m_frames.isEmpty();
}

SequenceInfo SequenceModel::sequenceInfo() const
{
    SequenceInfo info;
    info.isSequence = isSequence();
    info.prefix = m_prefix;
    info.secondarySuffix = m_secondarySuffix;
    info.extension = m_extension;
    info.padding = m_padding;
    info.startFrame = startFrame();
    info.endFrame = endFrame();
    info.frameCount = frameCount();
    return info;
}

QString SequenceModel::framePath(int index) const
{
    if (index >= 0 && index < m_frames.size())
        return m_frames[index].filePath;
    return {};
}

int SequenceModel::frameNumber(int index) const
{
    if (index >= 0 && index < m_frames.size())
        return m_frames[index].frameNumber;
    return -1;
}

int SequenceModel::indexOfFrame(int frameNum) const
{
    auto it = std::lower_bound(m_frames.begin(), m_frames.end(), frameNum,
        [](const FrameItem& item, int val) {
            return item.frameNumber < val;
        });
    if (it != m_frames.end() && it->frameNumber == frameNum) {
        return static_cast<int>(std::distance(m_frames.begin(), it));
    }
    return -1;
}

int SequenceModel::indexOfPath(const QString& path) const
{
    if (path.isEmpty() || m_frames.isEmpty())
        return -1;
    const QString absTarget = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    for (int i = 0; i < m_frames.size(); ++i) {
        if (QDir::cleanPath(m_frames[i].filePath).compare(absTarget, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

QList<int> SequenceModel::missingFrames() const
{
    QList<int> missing;
    if (m_frames.size() <= 1)
        return missing;

    int expected = m_frames.first().frameNumber;
    for (const FrameItem& item : m_frames) {
        while (expected < item.frameNumber) {
            missing.append(expected);
            ++expected;
        }
        expected = item.frameNumber + 1;
    }
    return missing;
}

const QList<FrameItem>& SequenceModel::frames() const
{
    return m_frames;
}

QString SequenceModel::formatPattern(TokenFormat format) const
{
    if (m_frames.isEmpty())
        return {};
    if (!m_isSequence)
        return QFileInfo(m_frames.first().filePath).fileName();

    const int pad = (m_padding > 0) ? m_padding : 1;
    QString token;
    switch (format) {
    case TokenFormat::Hash:
        token = QString(pad, QLatin1Char('#'));
        break;
    case TokenFormat::Printf:
        token = (pad > 1) ? (QStringLiteral("%0") + QString::number(pad) + QLatin1Char('d')) : QStringLiteral("%d");
        break;
    case TokenFormat::DollarF:
        token = (pad > 1) ? (QStringLiteral("$F") + QString::number(pad)) : QStringLiteral("$F");
        break;
    }

    return m_prefix + token + m_secondarySuffix + m_extension;
}

}  // namespace sview
