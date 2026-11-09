#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>

namespace sview {

enum class IconAspectRatioMode {
    LetterboxTransparent,  // Fit within square canvas with transparent letterbox borders (default)
    CropToSquare,          // Center-crop to 1:1 aspect ratio then scale
    StretchToSquare        // Non-uniform stretch to square
};

struct IconConvertResult {
    bool success = false;
    QString errorMessage;
    int framesGenerated = 0;
    qint64 fileSizeBytes = 0;
};

class IconConverter {
public:
    /// Standard Windows icon resolutions: {16, 32, 48, 64, 128, 256}
    static const QList<int>& defaultResolutions();
    static QList<int> standardResolutions() { return defaultResolutions(); }

    /// Primary interface matching Master Project Spec (PROJECT.md line 50)
    static bool saveMultiResIco(const QImage& sourceImage,
                                const QString& outputPath,
                                const QList<int>& resolutions = defaultResolutions());

    /// Full-featured conversion from on-disk image file (PNG, JPG, BMP, EXR, etc.)
    static bool convertImageToIco(const QString& inputImagePath,
                                  const QString& outputPath,
                                  const QList<int>& resolutions = defaultResolutions());

    /// Detailed conversion result from in-memory QImage
    static IconConvertResult convertToIco(const QImage& sourceImage,
                                          const QString& outputPath,
                                          const QList<int>& resolutions = defaultResolutions(),
                                          IconAspectRatioMode mode = IconAspectRatioMode::LetterboxTransparent);

    /// Detailed conversion result from file path
    static IconConvertResult convertFileToIco(const QString& inputImagePath,
                                              const QString& outputPath,
                                              const QList<int>& resolutions = defaultResolutions(),
                                              IconAspectRatioMode mode = IconAspectRatioMode::LetterboxTransparent);

    /// In-memory binary encoder returning raw .ico file bytes
    static QByteArray createIcoData(const QImage& sourceImage,
                                    const QList<int>& resolutions = defaultResolutions(),
                                    IconAspectRatioMode mode = IconAspectRatioMode::LetterboxTransparent,
                                    QString* errorMessage = nullptr);

    /// Prepares an individual square mipmap layer of the requested dimension
    static QImage prepareFrame(const QImage& sourceImage,
                               int targetDimension,
                               IconAspectRatioMode mode = IconAspectRatioMode::LetterboxTransparent);

    /// Validates an existing .ico file on disk
    static bool isValidIco(const QString& icoPath, int* frameCount = nullptr);
    static bool isValidIcoFile(const QString& icoPath, int* frameCount = nullptr) {
        return isValidIco(icoPath, frameCount);
    }

    /// Validates an in-memory byte buffer as valid Windows .ico data
    static bool isValidIcoData(const QByteArray& icoData, int* frameCount = nullptr);
};

} // namespace sview
