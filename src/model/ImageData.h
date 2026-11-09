#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <memory>
#include <vector>

namespace sview {

// Espace couleur supposé du fichier source (sert à choisir l'entrée OCIO).
enum class SourceSpace {
    SceneLinear,  // EXR, HDR, TIFF float...
    SRGB          // PNG, JPEG, BMP... (8/16 bits)
};

// Structure représentant une couche AOV (Arbitrary Output Variable) ou groupe de canaux
struct AovLayer {
    QString name;             // e.g. "RGBA", "diffuse", "specular", "depth", "N"
    QStringList channels;     // Noms complets des canaux : ["diffuse.R", "diffuse.G", "diffuse.B"]
    int subimage = 0;         // Index de la sous-image dans le fichier (0 par défaut)
};

// Image décodée, toujours convertie en RGBA float32 (origine en haut à gauche).
struct ImageData {
    QString path;
    QString formatName;          // "openexr", "png", ...
    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    QStringList channelNames;    // Ensemble des canaux du fichier
    QList<AovLayer> aovLayers;   // Couches AOV découvertes dans l'image
    QString activeAov = QStringLiteral("RGBA"); // Couche AOV actuellement chargée
    bool hasAlpha = false;
    SourceSpace space = SourceSpace::SRGB;
    // Espace d'entrée déduit des chromaticités EXR ("ACEScg", "ACES2065-1",
    // "Rec.2020", "P3-D65", "Rec.709"). Vide si le fichier n'en déclare pas.
    QString sourceColorSpace;
    std::vector<float> rgba;     // width * height * 4
    QMap<QString, QString> metadata; // Métadonnées OIIO/EXIF/techniques

    std::size_t byteSize() const { return rgba.size() * sizeof(float); }
};

using ImagePtr = std::shared_ptr<const ImageData>;

}  // namespace sview
