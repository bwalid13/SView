#include "ImageLoader.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace sview {

namespace {

// Lit l'attribut "chromaticities" (8 floats : rx,ry,gx,gy,bx,by,wx,wy).
bool readChromaticities(const OIIO::ImageSpec& spec, float out[8])
{
    for (const auto& attr : spec.extra_attribs) {
        if (attr.name() != "chromaticities")
            continue;
        if (attr.type().elementtype() != OIIO::TypeDesc::FLOAT)
            continue;
        if (int(attr.type().numelements()) < 8)
            continue;
        const float* v = static_cast<const float*>(attr.data());
        if (!v)
            return false;
        for (int i = 0; i < 8; ++i)
            out[i] = v[i];
        return true;
    }
    return false;
}

// Reconnaît les primaires d'un espace couleur usuel à partir des chromaticités.
QString classifyPrimaries(const float c[8])
{
    struct Reference {
        const char* name;
        float xy[8];
    };
    // Ordre : du plus spécifique au plus générique.
    static const Reference kReferences[] = {
        {"ACES2065-1", {0.7347f, 0.2653f, 0.0f, 1.0f, 0.0001f, -0.0770f, 0.32168f, 0.33767f}},
        {"ACEScg",     {0.713f, 0.293f, 0.165f, 0.830f, 0.128f, 0.044f, 0.32168f, 0.33767f}},
        {"Rec.2020",   {0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f}},
        {"P3-D65",     {0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.3127f, 0.3290f}},
        {"Rec.709",    {0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f, 0.3127f, 0.3290f}},
    };

    constexpr float kTolerance = 0.0025f;
    for (const auto& ref : kReferences) {
        bool match = true;
        for (int i = 0; i < 8; ++i) {
            if (std::abs(c[i] - ref.xy[i]) > kTolerance) {
                match = false;
                break;
            }
        }
        if (match)
            return QString::fromLatin1(ref.name);
    }
    return {};
}

SourceSpace guessSpace(const OIIO::ImageBuf& src)
{
    // 1) Métadonnée explicite si présente
    const QString tag =
        QString::fromStdString(src.spec().get_string_attribute("oiio:ColorSpace")).toLower();
    if (tag.contains("srgb"))
        return SourceSpace::SRGB;
    if (tag.contains("linear") || tag.startsWith("lin_") || tag.contains("acescg"))
        return SourceSpace::SceneLinear;

    // 2) Sinon : un format natif flottant est supposé linéaire, un entier est supposé sRGB
    return src.nativespec().format.is_floating_point() ? SourceSpace::SceneLinear
                                                       : SourceSpace::SRGB;
}

bool isMainChannel(const std::string& ch)
{
    if (ch.size() == 1) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ch[0])));
        return (c == 'r' || c == 'g' || c == 'b' || c == 'a' || c == 'y' || c == 'i');
    }
    std::string lower = ch;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return (lower == "red" || lower == "green" || lower == "blue" || lower == "alpha");
}

}  // namespace

int findChannel(const std::vector<std::string>& channelNames, const char* name)
{
    const char targetChar = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));

    // Pass 1: Exact full name match (e.g. "R", "r")
    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string& chName = channelNames[i];
        if (chName == name)
            return int(i);
        if (chName.size() == 1 && std::tolower(static_cast<unsigned char>(chName[0])) == targetChar)
            return int(i);
    }

    // Pass 1b: Standard English channel names ("red", "green", "blue", "alpha")
    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string& chName = channelNames[i];
        if (targetChar == 'r' && (chName == "red" || chName == "Red" || chName == "RED"))
            return int(i);
        if (targetChar == 'g' && (chName == "green" || chName == "Green" || chName == "GREEN"))
            return int(i);
        if (targetChar == 'b' && (chName == "blue" || chName == "Blue" || chName == "BLUE"))
            return int(i);
        if (targetChar == 'a' && (chName == "alpha" || chName == "Alpha" || chName == "ALPHA"))
            return int(i);
    }

    // Pass 2: Preferred beauty/RGBA layers ("RGBA.R", "beauty.R")
    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string& chName = channelNames[i];
        const auto dotPos = chName.rfind('.');
        if (dotPos != std::string::npos) {
            const std::string layer = chName.substr(0, dotPos);
            const std::string base = chName.substr(dotPos + 1);
            if ((layer == "RGBA" || layer == "beauty" || layer == "Beauty") &&
                (base == name || (base.size() == 1 && std::tolower(static_cast<unsigned char>(base[0])) == targetChar)))
                return int(i);
        }
    }

    // Pass 3: Any layer dot-suffix ("<layer>.R")
    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string& chName = channelNames[i];
        const auto dotPos = chName.rfind('.');
        if (dotPos != std::string::npos) {
            const std::string base = chName.substr(dotPos + 1);
            if (base == name || (base.size() == 1 && std::tolower(static_cast<unsigned char>(base[0])) == targetChar))
                return int(i);
        }
    }

    return -1;
}

QList<AovLayer> groupChannelsToAovs(const std::vector<std::string>& channelNames,
                                    int subimage,
                                    const QString& subimageName)
{
    QList<AovLayer> layers;
    if (channelNames.empty())
        return layers;

    QStringList beautyChannels;
    QHash<QString, QStringList> grouped;
    QStringList groupOrder;

    for (const auto& rawCh : channelNames) {
        const QString ch = QString::fromStdString(rawCh);
        const int dotIdx = ch.lastIndexOf('.');

        if (dotIdx != -1) {
            const QString prefix = ch.left(dotIdx);
            if (prefix.compare("RGBA", Qt::CaseInsensitive) == 0 ||
                prefix.compare("beauty", Qt::CaseInsensitive) == 0) {
                beautyChannels << ch;
            } else {
                if (!grouped.contains(prefix))
                    groupOrder << prefix;
                grouped[prefix] << ch;
            }
        } else {
            if (isMainChannel(rawCh)) {
                beautyChannels << ch;
            } else if (ch.compare("Z", Qt::CaseInsensitive) == 0 ||
                       ch.compare("depth", Qt::CaseInsensitive) == 0) {
                const QString depthName = QStringLiteral("depth");
                if (!grouped.contains(depthName))
                    groupOrder << depthName;
                grouped[depthName] << ch;
            } else {
                if (!grouped.contains(ch))
                    groupOrder << ch;
                grouped[ch] << ch;
            }
        }
    }

    // 1. Couche Beauty / RGBA toujours en première position si détectée
    if (!beautyChannels.isEmpty()) {
        AovLayer beautyLayer;
        beautyLayer.name = subimageName.isEmpty() ? QStringLiteral("RGBA") : subimageName;
        beautyLayer.channels = beautyChannels;
        beautyLayer.subimage = subimage;
        layers.append(beautyLayer);
    } else if (subimageName.compare("RGBA", Qt::CaseInsensitive) == 0 ||
               subimageName.compare("beauty", Qt::CaseInsensitive) == 0) {
        AovLayer beautyLayer;
        beautyLayer.name = QStringLiteral("RGBA");
        for (const auto& ch : channelNames)
            beautyLayer.channels << QString::fromStdString(ch);
        beautyLayer.subimage = subimage;
        layers.append(beautyLayer);
    }

    // 2. Couches préfixées et utilitaires dans leur ordre d'apparition
    for (const QString& name : groupOrder) {
        AovLayer layer;
        layer.name = name;
        layer.channels = grouped.value(name);
        layer.subimage = subimage;
        layers.append(layer);
    }

    // Si aucune couche n'a été isolée, tout regrouper en "RGBA"
    if (layers.isEmpty()) {
        AovLayer fallback;
        fallback.name = QStringLiteral("RGBA");
        for (const auto& ch : channelNames)
            fallback.channels << QString::fromStdString(ch);
        fallback.subimage = subimage;
        layers.append(fallback);
    }

    return layers;
}

QList<AovLayer> discoverAovLayers(const QString& path)
{
    QList<AovLayer> layers;
    try {
        const std::string nativePath = QDir::toNativeSeparators(path).toStdString();
        OIIO::ImageBuf src(nativePath);
        if (!src.init_spec(nativePath, 0, 0))
            return layers;

        const int numSubs = std::max(1, src.nsubimages());
        for (int s = 0; s < numSubs; ++s) {
            OIIO::ImageSpec subSpec;
            if (s == 0) {
                subSpec = src.spec();
            } else {
                OIIO::ImageBuf subBuf(nativePath);
                if (subBuf.init_spec(nativePath, s, 0))
                    subSpec = subBuf.spec();
                else
                    continue;
            }

            QString subName = QString::fromStdString(subSpec.get_string_attribute("name"));
            if (subName.isEmpty())
                subName = QString::fromStdString(subSpec.get_string_attribute("subimagename"));

            QList<AovLayer> subLayers = groupChannelsToAovs(subSpec.channelnames, s, subName);
            for (const auto& l : subLayers) {
                bool exists = false;
                for (const auto& existing : layers) {
                    if (existing.name.compare(l.name, Qt::CaseInsensitive) == 0) {
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                    layers.append(l);
            }
        }
    } catch (...) {
        // En cas d'erreur de lecture, renvoyer ce qui a été trouvé
    }
    return layers;
}

LoadResult loadImage(const QString& path, const QString& aovName)
{
    LoadResult res;
    res.path = path;

    try {
        const std::string nativePath = QDir::toNativeSeparators(path).toStdString();

        // 1. Découverte des couches AOV.
        //    Uniquement si une couche précise est demandée : pour le chemin
        //    courant (RGBA), la liste est construite plus bas à partir de la
        //    spec déjà lue, ce qui évite une seconde ouverture du fichier —
        //    coûteuse sur les gros EXR multi-parties.
        const bool wantsSpecificAov =
            !aovName.isEmpty() && aovName.compare("RGBA", Qt::CaseInsensitive) != 0;

        QList<AovLayer> allLayers;
        AovLayer targetLayer;
        bool targetFound = false;

        if (wantsSpecificAov) {
            allLayers = discoverAovLayers(path);
            for (const auto& l : allLayers) {
                if (l.name.compare(aovName, Qt::CaseInsensitive) == 0) {
                    targetLayer = l;
                    targetFound = true;
                    break;
                }
            }
        }

        const int subimageToRead = targetFound ? targetLayer.subimage : 0;

        // 2. Lecture de l'image (ou sous-image) en float
        OIIO::ImageBuf src(nativePath);
        if (!src.read(subimageToRead, 0, /*force=*/true, OIIO::TypeDesc::FLOAT)) {
            res.error = QString::fromStdString(src.geterror());
            if (res.error.isEmpty())
                res.error = QStringLiteral("Unreadable file");
            return res;
        }

        const SourceSpace space = guessSpace(src);
        const QString formatName = QString::fromUtf8(src.file_format_name());

        // Orientation EXIF : reorient() ne duplique les pixels que si une
        // rotation est réellement demandée. Sinon une copie superficielle suffit.
        OIIO::ImageBuf buf = src;
        if (src.spec().get_int_attribute("Orientation", 1) != 1) {
            buf = OIIO::ImageBufAlgo::reorient(src);
            if (buf.has_error()) {
                res.error = QString::fromStdString(buf.geterror());
                return res;
            }
        }

        const OIIO::ImageSpec& spec = buf.spec();
        const int w = spec.width;
        const int h = spec.height;
        const int nc = spec.nchannels;
        if (w <= 0 || h <= 0 || nc <= 0) {
            res.error = QStringLiteral("Empty image");
            return res;
        }

        // Accès direct aux pixels déjà décodés par ImageBuf : évite la copie
        // pleine résolution supplémentaire dans `raw`. Repli sur get_pixels si
        // le buffer n'est pas local (adossé au cache tuilé) ou dans un format
        // inattendu.
        const void* localPixels = buf.localpixels();
        const size_t pixelStride = buf.pixel_stride();
        const bool useLocalPixels = (localPixels != nullptr) &&
                                    (pixelStride >= size_t(nc) * sizeof(float)) &&
                                    (spec.format == OIIO::TypeDesc::FLOAT);

        std::vector<float> raw;
        if (!useLocalPixels) {
            raw.resize(size_t(w) * size_t(h) * size_t(nc));
            if (!buf.get_pixels(buf.roi(), OIIO::TypeDesc::FLOAT, raw.data())) {
                res.error = QString::fromStdString(buf.geterror());
                if (res.error.isEmpty())
                    res.error = QStringLiteral("Cannot read pixels");
                return res;
            }
        }
        const char* localBase = static_cast<const char*>(localPixels);

        if (allLayers.isEmpty()) {
            QString subName = QString::fromStdString(spec.get_string_attribute("name"));
            if (subName.isEmpty()) {
                subName = QString::fromStdString(spec.get_string_attribute("subimagename"));
            }
            allLayers = groupChannelsToAovs(spec.channelnames, subimageToRead, subName);
        }

        // 3. Mapping des canaux vers RGBA float32
        int ir = -1, ig = -1, ib = -1, ia = -1;
        int activeChannelsCount = nc;
        QString activeAovName = QStringLiteral("RGBA");

        if (targetFound) {
            activeAovName = targetLayer.name;
            activeChannelsCount = int(targetLayer.channels.size());
            const QStringList& chs = targetLayer.channels;

            if (chs.size() == 1) {
                // Couche monochrome / profondeur : répliquer sur R, G, B avec A = 1.0f
                const std::string targetCh = chs[0].toStdString();
                for (size_t c = 0; c < spec.channelnames.size(); ++c) {
                    if (spec.channelnames[c] == targetCh) {
                        ir = ig = ib = int(c);
                        break;
                    }
                }
                ia = -1;
            } else if (chs.size() >= 3) {
                for (const QString& chName : chs) {
                    const std::string s = chName.toStdString();
                    for (size_t c = 0; c < spec.channelnames.size(); ++c) {
                        if (spec.channelnames[c] == s) {
                            const int dotPos = chName.lastIndexOf('.');
                            const QString base = (dotPos != -1) ? chName.mid(dotPos + 1).toUpper()
                                                                : chName.toUpper();
                            if (base == "R" || base == "RED" || base == "X") ir = int(c);
                            else if (base == "G" || base == "GREEN" || base == "Y") ig = int(c);
                            else if (base == "B" || base == "BLUE" || base == "Z") ib = int(c);
                            else if (base == "A" || base == "ALPHA") ia = int(c);
                            break;
                        }
                    }
                }
                if (ir < 0 && chs.size() > 0) ir = findChannel(spec.channelnames, chs[0].toStdString().c_str());
                if (ig < 0 && chs.size() > 1) ig = findChannel(spec.channelnames, chs[1].toStdString().c_str());
                if (ib < 0 && chs.size() > 2) ib = findChannel(spec.channelnames, chs[2].toStdString().c_str());
                if (ia < 0 && chs.size() > 3) ia = findChannel(spec.channelnames, chs[3].toStdString().c_str());
            } else if (chs.size() == 2) {
                ir = findChannel(spec.channelnames, chs[0].toStdString().c_str());
                ig = findChannel(spec.channelnames, chs[1].toStdString().c_str());
                ib = -1;
                ia = -1;
            }
        }

        // Fallback Beauty / standard
        if (ir < 0 || ig < 0 || ib < 0) {
            ir = findChannel(spec.channelnames, "R");
            ig = findChannel(spec.channelnames, "G");
            ib = findChannel(spec.channelnames, "B");
            ia = spec.alpha_channel;
            const bool unnamed = (ir < 0 || ig < 0 || ib < 0);
            if (unnamed) {
                if (nc >= 3) {
                    ir = 0; ig = 1; ib = 2;
                } else {
                    ir = ig = ib = 0;
                }
            }
            if (ia < 0) {
                ia = findChannel(spec.channelnames, "A");
                if (ia < 0 && unnamed) {
                    ia = (nc == 2) ? 1 : (nc >= 4 ? 3 : -1);
                }
            }
        }

        // Espace source déduit des chromaticités déclarées par le fichier
        // (les EXR de DCC en portent presque toujours).
        QString detectedSpace;
        float chromaticities[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        if (readChromaticities(spec, chromaticities)) {
            detectedSpace = classifyPrimaries(chromaticities);
        }

        auto img = std::make_shared<ImageData>();
        img->path = path;
        img->formatName = formatName;
        img->width = w;
        img->height = h;
        img->sourceChannels = activeChannelsCount;
        img->hasAlpha = (ia >= 0);
        img->space = space;
        img->sourceColorSpace = detectedSpace;
        img->aovLayers = allLayers;
        img->activeAov = activeAovName;

        for (const auto& name : spec.channelnames)
            img->channelNames << QString::fromStdString(name);

        img->rgba.resize(size_t(w) * size_t(h) * 4);
        const size_t pixelCount = size_t(w) * size_t(h);
        for (size_t i = 0; i < pixelCount; ++i) {
            const float* p = useLocalPixels
                ? reinterpret_cast<const float*>(localBase + i * pixelStride)
                : &raw[i * size_t(nc)];
            float* o = &img->rgba[i * 4];
            o[0] = (ir >= 0 && ir < nc) ? p[ir] : 0.0f;
            o[1] = (ig >= 0 && ig < nc) ? p[ig] : 0.0f;
            o[2] = (ib >= 0 && ib < nc) ? p[ib] : 0.0f;
            o[3] = (ia >= 0 && ia < nc) ? p[ia] : 1.0f;
        }

        // Feature 21: Extraction des métadonnées OIIO et spécifications de l'image
        img->metadata.insert(QStringLiteral("File Name"), QFileInfo(path).fileName());
        img->metadata.insert(QStringLiteral("File Path"), path);
        img->metadata.insert(QStringLiteral("Dimensions"), QStringLiteral("%1 x %2").arg(w).arg(h));
        img->metadata.insert(QStringLiteral("Channels"), QStringLiteral("%1 (%2)").arg(activeChannelsCount).arg(img->channelNames.join(QStringLiteral(", "))));
        img->metadata.insert(QStringLiteral("Pixel Format"), QString::fromStdString(std::string(spec.format.c_str())));
        img->metadata.insert(QStringLiteral("File Format"), formatName);
        img->metadata.insert(QStringLiteral("Color Space"),
                             (space == SourceSpace::SceneLinear) ? QStringLiteral("Scene-Linear")
                                                                 : QStringLiteral("sRGB"));
        if (!detectedSpace.isEmpty()) {
            img->metadata.insert(QStringLiteral("Source Color Space"), detectedSpace);
        }
        img->metadata.insert(QStringLiteral("Active AOV"), activeAovName);
        img->metadata.insert(QStringLiteral("Memory Footprint"), QStringLiteral("%1 MB").arg(double(img->byteSize()) / (1024.0 * 1024.0), 0, 'f', 2));

        for (const auto& param : spec.extra_attribs) {
            const QString key = QString::fromStdString(param.name().string());
            const QString val = QString::fromStdString(OIIO::ImageSpec::metadata_val(param, true));
            if (!key.isEmpty() && !val.isEmpty()) {
                img->metadata.insert(key, val);
            }
        }

        res.image = std::move(img);
    } catch (const std::exception& e) {
        res.error = QString::fromUtf8(e.what());
    } catch (...) {
        res.error = QStringLiteral("Unknown error loading image");
    }
    return res;
}

LoadResult loadAov(const QString& path, const QString& aovName)
{
    return loadImage(path, aovName);
}

bool saveImage(const QString& inputPath, const QString& outputPath, bool halfRes)
{
    try {
        const std::string inNative = QDir::toNativeSeparators(inputPath).toStdString();
        const std::string outNative = QDir::toNativeSeparators(outputPath).toStdString();
        OIIO::ImageBuf src(inNative);
        if (!src.read()) {
            return false;
        }

        if (halfRes) {
            const int newW = std::max(1, src.spec().width / 2);
            const int newH = std::max(1, src.spec().height / 2);
            OIIO::ROI roi(0, newW, 0, newH, 0, 1, 0, src.spec().nchannels);
            OIIO::ImageBuf dst = OIIO::ImageBufAlgo::resize(src, "", 0.0f, roi);
            if (dst.has_error()) {
                return false;
            }
            return dst.write(outNative);
        }

        return src.write(outNative);
    } catch (...) {
        return false;
    }
}

}  // namespace sview
