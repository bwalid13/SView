#include "IconConverter.h"

#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>

#include <algorithm>

namespace sview {

const QList<int>& IconConverter::defaultResolutions()
{
    static const QList<int> s_resolutions = {16, 32, 48, 64, 128, 256};
    return s_resolutions;
}

bool IconConverter::saveMultiResIco(const QImage& sourceImage,
                                    const QString& outputPath,
                                    const QList<int>& resolutions)
{
    return convertToIco(sourceImage, outputPath, resolutions, IconAspectRatioMode::LetterboxTransparent).success;
}

bool IconConverter::convertImageToIco(const QString& inputImagePath,
                                      const QString& outputPath,
                                      const QList<int>& resolutions)
{
    return convertFileToIco(inputImagePath, outputPath, resolutions, IconAspectRatioMode::LetterboxTransparent).success;
}

QImage IconConverter::prepareFrame(const QImage& sourceImage,
                                   int targetDimension,
                                   IconAspectRatioMode mode)
{
    if (sourceImage.isNull() || targetDimension <= 0)
        return QImage();

    // Ensure 32-bit ARGB with pre-multiplied alpha for clean blitting
    QImage src = sourceImage;
    if (src.format() != QImage::Format_ARGB32_Premultiplied &&
        src.format() != QImage::Format_ARGB32) {
        src = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    if (mode == IconAspectRatioMode::CropToSquare) {
        const int minDim = std::min(src.width(), src.height());
        const int cx = (src.width() - minDim) / 2;
        const int cy = (src.height() - minDim) / 2;
        QImage cropped = src.copy(cx, cy, minDim, minDim);
        return cropped.scaled(targetDimension, targetDimension,
                              Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (mode == IconAspectRatioMode::StretchToSquare) {
        return src.scaled(targetDimension, targetDimension,
                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // Default: LetterboxTransparent
    QImage canvas(targetDimension, targetDimension, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QImage scaled = src.scaled(targetDimension, targetDimension,
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int x = (targetDimension - scaled.width()) / 2;
    const int y = (targetDimension - scaled.height()) / 2;
    painter.drawImage(x, y, scaled);
    painter.end();

    return canvas;
}

QByteArray IconConverter::createIcoData(const QImage& sourceImage,
                                        const QList<int>& resolutions,
                                        IconAspectRatioMode mode,
                                        QString* errorMessage)
{
    if (sourceImage.isNull() || sourceImage.width() <= 0 || sourceImage.height() <= 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Source image is null or has zero dimensions.");
        return QByteArray();
    }

    // Deduplicate, filter (1..256), and sort resolutions ascending
    QList<int> validResolutions;
    for (int r : resolutions) {
        if (r > 0 && r <= 256 && !validResolutions.contains(r)) {
            validResolutions.append(r);
        }
    }
    std::sort(validResolutions.begin(), validResolutions.end());

    if (validResolutions.isEmpty()) {
        validResolutions = defaultResolutions();
    }

    struct EncodedFrame {
        int dimension;
        QByteArray pngData;
    };
    QList<EncodedFrame> frames;
    frames.reserve(validResolutions.size());

    for (int r : validResolutions) {
        QImage frameImg = prepareFrame(sourceImage, r, mode);
        if (frameImg.isNull()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to prepare frame %1x%1.").arg(r);
            return QByteArray();
        }

        QByteArray pngBuffer;
        QBuffer buffer(&pngBuffer);
        buffer.open(QIODevice::WriteOnly);
        if (!frameImg.save(&buffer, "PNG")) {
            if (errorMessage)
                *errorMessage = QStringLiteral("PNG encoding failed for resolution %1x%1.").arg(r);
            return QByteArray();
        }
        frames.append({r, pngBuffer});
    }

    if (frames.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No frames were successfully encoded.");
        return QByteArray();
    }

    // Binary assembly with Little-Endian byte ordering
    QByteArray icoBytes;
    QDataStream stream(&icoBytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // 1. ICONDIR Header (6 bytes)
    stream << quint16(0);                                   // idReserved = 0
    stream << quint16(1);                                   // idType = 1 (Icon)
    stream << quint16(static_cast<quint16>(frames.size())); // idCount

    // 2. ICONDIRENTRY Table (16 bytes per frame)
    quint32 currentDataOffset = 6 + static_cast<quint32>(frames.size() * 16);

    for (const auto& frame : frames) {
        const quint8 bDim = (frame.dimension >= 256) ? 0 : static_cast<quint8>(frame.dimension);
        stream << quint8(bDim);                  // bWidth (0 = 256)
        stream << quint8(bDim);                  // bHeight (0 = 256)
        stream << quint8(0);                     // bColorCount (0 for >=8bpp)
        stream << quint8(0);                     // bReserved (0)
        stream << quint16(1);                    // wPlanes (1)
        stream << quint16(32);                   // wBitCount (32bpp)
        stream << quint32(frame.pngData.size()); // dwBytesInRes
        stream << quint32(currentDataOffset);    // dwImageOffset

        currentDataOffset += static_cast<quint32>(frame.pngData.size());
    }

    // 3. Image Payloads (PNG byte streams)
    for (const auto& frame : frames) {
        stream.writeRawData(frame.pngData.constData(), frame.pngData.size());
    }

    return icoBytes;
}

IconConvertResult IconConverter::convertToIco(const QImage& sourceImage,
                                              const QString& outputPath,
                                              const QList<int>& resolutions,
                                              IconAspectRatioMode mode)
{
    IconConvertResult result;

    if (sourceImage.isNull() || sourceImage.width() <= 0 || sourceImage.height() <= 0) {
        result.errorMessage = QStringLiteral("Source image is null or has zero dimensions.");
        return result;
    }
    if (outputPath.isEmpty()) {
        result.errorMessage = QStringLiteral("Output path is empty.");
        return result;
    }

    // Auto-create destination directory tree
    QFileInfo fileInfo(outputPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        result.errorMessage = QStringLiteral("Failed to create directory: %1").arg(dir.path());
        return result;
    }

    QString genError;
    const QByteArray icoData = createIcoData(sourceImage, resolutions, mode, &genError);
    if (icoData.isEmpty()) {
        result.errorMessage = genError.isEmpty() ? QStringLiteral("Failed to create ICO data.") : genError;
        return result;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        result.errorMessage = QStringLiteral("Failed to open output file: %1").arg(file.errorString());
        return result;
    }

    const qint64 written = file.write(icoData);
    file.close();

    if (written != icoData.size()) {
        result.errorMessage = QStringLiteral("Incomplete write to output file.");
        return result;
    }

    result.success = true;
    result.framesGenerated = resolutions.isEmpty() ? defaultResolutions().size() : resolutions.size();
    result.fileSizeBytes = written;
    return result;
}

IconConvertResult IconConverter::convertFileToIco(const QString& inputImagePath,
                                                  const QString& outputPath,
                                                  const QList<int>& resolutions,
                                                  IconAspectRatioMode mode)
{
    IconConvertResult result;

    if (!QFile::exists(inputImagePath)) {
        result.errorMessage = QStringLiteral("Input file does not exist: %1").arg(inputImagePath);
        return result;
    }

    QImageReader reader(inputImagePath);
    reader.setAutoTransform(true);
    QImage source = reader.read();

    if (source.isNull()) {
        result.errorMessage = QStringLiteral("Failed to read image '%1': %2")
                                  .arg(inputImagePath, reader.errorString());
        return result;
    }

    return convertToIco(source, outputPath, resolutions, mode);
}

bool IconConverter::isValidIcoData(const QByteArray& icoData, int* frameCount)
{
    if (icoData.size() < 6)
        return false;

    QDataStream stream(icoData);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 idReserved = 0;
    quint16 idType = 0;
    quint16 idCount = 0;
    stream >> idReserved >> idType >> idCount;

    if (idReserved != 0 || idType != 1 || idCount == 0)
        return false;

    const qint64 minHeaderSize = 6 + static_cast<qint64>(idCount) * 16;
    if (icoData.size() < minHeaderSize)
        return false;

    if (frameCount)
        *frameCount = static_cast<int>(idCount);

    return true;
}

bool IconConverter::isValidIco(const QString& icoPath, int* frameCount)
{
    QFile file(icoPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray header = file.read(6);
    if (header.size() < 6)
        return false;

    QDataStream stream(header);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 idReserved = 0;
    quint16 idType = 0;
    quint16 idCount = 0;
    stream >> idReserved >> idType >> idCount;

    if (idReserved != 0 || idType != 1 || idCount == 0)
        return false;

    const qint64 totalDirSize = 6 + static_cast<qint64>(idCount) * 16;
    if (file.size() < totalDirSize)
        return false;

    if (frameCount)
        *frameCount = static_cast<int>(idCount);

    return true;
}

} // namespace sview
