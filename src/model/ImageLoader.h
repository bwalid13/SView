#pragma once

#include "ImageData.h"

#include <QList>
#include <QString>
#include <string>
#include <vector>

namespace sview {

struct LoadResult {
    QString path;
    ImagePtr image;   // nul en cas d'erreur
    QString error;
};

// Recherche prioritaire de canal (exact, couches préférées, préfixes)
int findChannel(const std::vector<std::string>& channelNames, const char* name);

// Groupe une liste de noms de canaux d'une spec OIIO en couches AovLayer
QList<AovLayer> groupChannelsToAovs(const std::vector<std::string>& channelNames,
                                    int subimage = 0,
                                    const QString& subimageName = QString());

// Découvre l'ensemble des couches AOV d'un fichier (sans lire l'intégralité des pixels)
QList<AovLayer> discoverAovLayers(const QString& path);

// Thread-safe : charge l'image en extrayant la couche AOV spécifiée (par défaut "RGBA")
LoadResult loadImage(const QString& path, const QString& aovName = QString());

// Extraction à la demande d'une couche AOV spécifique
LoadResult loadAov(const QString& path, const QString& aovName);

// Sauvegarde une image vers un autre format via OIIO (avec option demi-résolution 50% proxy)
bool saveImage(const QString& inputPath, const QString& outputPath, bool halfRes = false);

}  // namespace sview
