#include "ImageLoader.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
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

// ---------------------------------------------------------------------------
// Décodeur natif Houdini .rat (TBF "fbtH" v5/v6 + RATclassic "tarH")
// ---------------------------------------------------------------------------

float halfToFloat(uint16_t h)
{
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x03FFu;
    uint32_t bits = 0;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign;
        } else {
            exp = 1;
            while ((mant & 0x0400u) == 0) {
                mant <<= 1;
                --exp;
            }
            mant &= 0x03FFu;
            bits = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        bits = sign | 0x7F800000u | (mant << 13);
    } else {
        bits = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
    }
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(float));
    return f;
}

template <typename T>
T readLE(const uint8_t* p)
{
    T val{};
    std::memcpy(&val, p, sizeof(T));
    return val;
}

QByteArray zlibDecompress(const uint8_t* src, size_t srcLen, uint32_t expectedLen)
{
    if (expectedLen == 0)
        return {};
    if (srcLen == expectedLen && (srcLen < 2 || src[0] != 0x78)) {
        return QByteArray(reinterpret_cast<const char*>(src), qsizetype(srcLen));
    }
    QByteArray prefixed;
    prefixed.resize(qsizetype(srcLen + 4));
    prefixed[0] = static_cast<char>((expectedLen >> 24) & 0xFFu);
    prefixed[1] = static_cast<char>((expectedLen >> 16) & 0xFFu);
    prefixed[2] = static_cast<char>((expectedLen >> 8) & 0xFFu);
    prefixed[3] = static_cast<char>(expectedLen & 0xFFu);
    std::memcpy(prefixed.data() + 4, src, srcLen);
    QByteArray out = qUncompress(reinterpret_cast<const uchar*>(prefixed.constData()), prefixed.size());
    if (out.isEmpty() && srcLen >= expectedLen && (srcLen < 2 || src[0] != 0x78)) {
        return QByteArray(reinterpret_cast<const char*>(src), qsizetype(expectedLen));
    }
    return out;
}

struct RatChannelHeader {
    int32_t idx = 0;
    int32_t dim = 0;
    int32_t dtype = 0;
    int32_t tsize = 0;
    int32_t cmodel = 0;
    int32_t nextAov = -1;
    int32_t nextMip = -1;
    uint64_t off1 = 0;
    uint64_t off2 = 0;
    uint64_t headerPos = 0;
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t tw = 0;
    uint32_t th = 0;
    QString rawName;
};

QString friendlyRatPlaneName(const QString& rawName, int planeIndex)
{
    if (rawName.isEmpty() || rawName == QLatin1String("Raster::C"))
        return (planeIndex == 0) ? QStringLiteral("RGBA") : QStringLiteral("Plane %1").arg(planeIndex);
    if (rawName == QLatin1String("Raster::C+x")) return QStringLiteral("Right (+X)");
    if (rawName == QLatin1String("Raster::C-x")) return QStringLiteral("Left (-X)");
    if (rawName == QLatin1String("Raster::C+y")) return QStringLiteral("Top (+Y)");
    if (rawName == QLatin1String("Raster::C-y")) return QStringLiteral("Bottom (-Y)");
    if (rawName == QLatin1String("Raster::C+z")) return QStringLiteral("Front (+Z)");
    if (rawName == QLatin1String("Raster::C-z")) return QStringLiteral("Back (-Z)");
    if (rawName.startsWith(QLatin1String("Raster::")))
        return rawName.mid(8);
    return rawName;
}

QStringList ratChannelNamesForSize(int tsize)
{
    if (tsize <= 1) return {QStringLiteral("Y")};
    if (tsize == 2) return {QStringLiteral("R"), QStringLiteral("G")};
    if (tsize == 3) return {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")};
    return {QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B"), QStringLiteral("A")};
}

QByteArray readRatDim1Payload(const uint8_t* data, size_t fileLen, uint32_t ver, const RatChannelHeader& ch)
{
    if (ch.off1 >= fileLen)
        return {};

    if (ver >= 6 || (ch.off1 + 24 <= fileLen && std::memcmp(data + ch.off1, "ztaD", 4) == 0)) {
        const uint32_t tulen = readLE<uint32_t>(data + ch.off1 + 16);
        const uint32_t tclen = readLE<uint32_t>(data + ch.off1 + 20);
        if (ch.off1 + 24 + tclen > fileLen || tulen < 8)
            return {};
        const QByteArray tt = (tclen == tulen)
            ? QByteArray(reinterpret_cast<const char*>(data + ch.off1 + 24), qsizetype(tclen))
            : zlibDecompress(data + ch.off1 + 24, tclen, tulen);
        if (tt.size() < 8)
            return {};
        const uint64_t t0 = readLE<uint64_t>(reinterpret_cast<const uint8_t*>(tt.constData()));
        if (t0 + 24 > fileLen || std::memcmp(data + t0, "ztaD", 4) != 0)
            return {};
        const uint32_t dulen = readLE<uint32_t>(data + t0 + 16);
        const uint32_t dclen = readLE<uint32_t>(data + t0 + 20);
        if (t0 + 24 + dclen > fileLen)
            return {};
        return (dclen == dulen)
            ? QByteArray(reinterpret_cast<const char*>(data + t0 + 24), qsizetype(dclen))
            : zlibDecompress(data + t0 + 24, dclen, dulen);
    }

    // Format TBF v5 (1 seule entrée dans la table de tuiles pour dim == 1)
    if (ch.off1 + 12 > fileLen || ch.headerPos <= ch.off1)
        return {};
    const uint64_t t0 = readLE<uint64_t>(data + ch.off1);
    const size_t diff = size_t(ch.headerPos - ch.off1);
    uint32_t dulen = 0;
    uint32_t dclen = 0;
    if (diff >= 16) {
        dulen = readLE<uint32_t>(data + ch.off1 + 8);
        dclen = readLE<uint32_t>(data + ch.off1 + 12);
    } else {
        dclen = readLE<uint32_t>(data + ch.off1 + 8);
        const int bpc = (ch.dtype == 6 || ch.dtype == 9) ? 2 : ((ch.dtype == 7 || ch.dtype == 10) ? 4 : 1);
        dulen = ch.w * ch.h * uint32_t(std::max(1, ch.tsize)) * uint32_t(bpc);
    }
    if (t0 + dclen > fileLen || dclen == 0)
        return {};
    if (dulen == 0 || dclen == dulen)
        return QByteArray(reinterpret_cast<const char*>(data + t0), qsizetype(dclen));
    QByteArray dec = zlibDecompress(data + t0, dclen, dulen);
    if (dec.isEmpty())
        return QByteArray(reinterpret_cast<const char*>(data + t0), qsizetype(dclen));
    return dec;
}

bool parseTbfHeaders(const uint8_t* data, size_t fileLen,
                     uint32_t& outVer,
                     std::vector<RatChannelHeader>& outChannels,
                     std::vector<int>& outTopLevel2D,
                     QString& outOptionJson)
{
    if (fileLen < 24 || std::memcmp(data, "fbtH", 4) != 0)
        return false;

    outVer = readLE<uint32_t>(data + 4);
    const uint64_t dirOff = readLE<uint64_t>(data + 8);
    if (dirOff == 0 || dirOff >= fileLen)
        return false;

    uint64_t pos = dirOff;
    int safety = 0;
    while (pos != 0 && pos + 24 <= fileLen && safety++ < 4096) {
        if (std::memcmp(data + pos, "zahC", 4) != 0)
            break;
        const uint64_t nextOff = readLE<uint64_t>(data + pos + 8);
        const uint32_t ulen = readLE<uint32_t>(data + pos + 16);
        const uint32_t clen = readLE<uint32_t>(data + pos + 20);
        if (pos + 24 + clen > fileLen || ulen < 64)
            break;

        const QByteArray dec = (clen == ulen)
            ? QByteArray(reinterpret_cast<const char*>(data + pos + 24), qsizetype(clen))
            : zlibDecompress(data + pos + 24, clen, ulen);
        if (dec.size() < 64)
            break;

        const auto* p = reinterpret_cast<const uint8_t*>(dec.constData());
        RatChannelHeader ch;
        ch.idx = readLE<int32_t>(p + 0);
        ch.dim = readLE<int32_t>(p + 4);
        ch.dtype = readLE<int32_t>(p + 8);
        ch.tsize = readLE<int32_t>(p + 12);
        ch.cmodel = readLE<int32_t>(p + 16);
        ch.nextAov = readLE<int32_t>(p + 20);
        ch.nextMip = readLE<int32_t>(p + 24);
        ch.off1 = readLE<uint64_t>(p + 32);
        ch.off2 = readLE<uint64_t>(p + 40);
        ch.headerPos = pos;
        ch.w = readLE<uint32_t>(p + 48);
        ch.h = readLE<uint32_t>(p + 52);
        ch.tw = readLE<uint32_t>(p + 56);
        ch.th = readLE<uint32_t>(p + 60);
        outChannels.push_back(ch);

        pos = nextOff;
    }

    if (outChannels.empty())
        return false;

    // Extraction des noms de canaux depuis ch[0] (StringData) et ch[1] (StringData-Depth)
    if (outChannels.size() >= 2 && outChannels[0].dim == 1 && outChannels[1].dim == 1) {
        const QByteArray strData = readRatDim1Payload(data, fileLen, outVer, outChannels[0]);
        const QByteArray strLens = readRatDim1Payload(data, fileLen, outVer, outChannels[1]);
        const size_t numLens = size_t(strLens.size()) / 2;
        size_t strPos = 0;
        for (size_t i = 0; i < outChannels.size() && i < numLens; ++i) {
            const uint16_t len = readLE<uint16_t>(reinterpret_cast<const uint8_t*>(strLens.constData()) + i * 2);
            if (strPos + len <= size_t(strData.size())) {
                outChannels[i].rawName = QString::fromUtf8(strData.constData() + strPos, qsizetype(len));
                strPos += len;
            }
        }
    }

    // Extraction de TBF_File::OptionData (JSON)
    for (const auto& ch : outChannels) {
        if (ch.dim == 1 && ch.rawName == QLatin1String("TBF_File::OptionData")) {
            const QByteArray optBytes = readRatDim1Payload(data, fileLen, outVer, ch);
            if (!optBytes.isEmpty())
                outOptionJson = QString::fromUtf8(optBytes);
        }
    }

    // Identification des canaux 2D de niveau MIP 0 (ceux qui ne sont le nextMip d'aucun autre canal)
    QSet<int> mipChildren;
    for (const auto& ch : outChannels) {
        if (ch.dim == 2 && ch.nextMip >= 0)
            mipChildren.insert(ch.nextMip);
    }
    for (size_t i = 0; i < outChannels.size(); ++i) {
        if (outChannels[i].dim == 2 && !mipChildren.contains(outChannels[i].idx)) {
            outTopLevel2D.push_back(int(i));
        }
    }

    return !outTopLevel2D.empty();
}

bool isRatFilePath(const QString& path)
{
    return path.endsWith(QLatin1String(".rat"), Qt::CaseInsensitive);
}

QList<AovLayer> discoverRatAovLayers(const QString& path)
{
    QList<AovLayer> layers;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return layers;

    const QByteArray blob = file.readAll();
    const auto* data = reinterpret_cast<const uint8_t*>(blob.constData());
    const size_t fileLen = size_t(blob.size());

    if (fileLen >= 16 && std::memcmp(data, "tarH", 4) == 0) {
        const int nchan = std::clamp<int>(data[10], 1, 4);
        AovLayer l;
        l.name = QStringLiteral("RGBA");
        l.channels = ratChannelNamesForSize(nchan);
        l.subimage = 0;
        layers.append(l);
        return layers;
    }

    uint32_t ver = 0;
    std::vector<RatChannelHeader> channels;
    std::vector<int> top2D;
    QString optionJson;
    if (!parseTbfHeaders(data, fileLen, ver, channels, top2D, optionJson))
        return layers;

    for (size_t p = 0; p < top2D.size(); ++p) {
        const RatChannelHeader& ch = channels[size_t(top2D[p])];
        AovLayer l;
        l.name = friendlyRatPlaneName(ch.rawName, int(p));
        l.channels = ratChannelNamesForSize(ch.tsize);
        l.subimage = int(p);
        layers.append(l);
    }
    return layers;
}

QString findHoudiniIconvert()
{
    QString exe = QStandardPaths::findExecutable(QStringLiteral("iconvert"));
    if (!exe.isEmpty())
        return exe;

#ifdef Q_OS_WIN
    QDir sidefxDir(QStringLiteral("C:/Program Files/Side Effects Software"));
    if (sidefxDir.exists()) {
        const QStringList subdirs = sidefxDir.entryList(QStringList() << QStringLiteral("Houdini*"),
                                                        QDir::Dirs | QDir::NoDotAndDotDot,
                                                        QDir::Name | QDir::Reversed);
        for (const QString& sub : subdirs) {
            const QString candidate = sidefxDir.filePath(sub + QStringLiteral("/bin/iconvert.exe"));
            if (QFileInfo::exists(candidate))
                return candidate;
        }
    }
#endif
    return {};
}

LoadResult loadRatViaHoudiniFallback(const QString& path, const QString& aovName)
{
    LoadResult res;
    res.path = path;
    const QString iconvert = findHoudiniIconvert();
    if (iconvert.isEmpty()) {
        res.error = QStringLiteral("Unsupported RAT format variant");
        return res;
    }

    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/sview_rat_XXXXXX.exr"));
    if (!tmp.open()) {
        res.error = QStringLiteral("Cannot create temporary file for RAT conversion");
        return res;
    }
    const QString tmpPath = tmp.fileName();
    tmp.close();

    QProcess proc;
    proc.start(iconvert, {QDir::toNativeSeparators(path), QDir::toNativeSeparators(tmpPath)});
    if (!proc.waitForFinished(15000) || proc.exitCode() != 0) {
        QFile::remove(tmpPath);
        res.error = QStringLiteral("Failed to decode RAT file");
        return res;
    }

    res = loadImage(tmpPath, aovName);
    QFile::remove(tmpPath);
    res.path = path;
    if (res.image) {
        auto mutableImg = std::make_shared<ImageData>(*res.image);
        mutableImg->path = path;
        mutableImg->formatName = QStringLiteral("Houdini RAT");
        mutableImg->metadata.insert(QStringLiteral("File Name"), QFileInfo(path).fileName());
        mutableImg->metadata.insert(QStringLiteral("File Path"), path);
        mutableImg->metadata.insert(QStringLiteral("File Format"), QStringLiteral("Houdini RAT"));
        res.image = std::move(mutableImg);
    }
    return res;
}

LoadResult loadRatImage(const QString& path, const QString& aovName)
{
    LoadResult res;
    res.path = path;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        res.error = QStringLiteral("Cannot open file: %1").arg(path);
        return res;
    }

    const QByteArray blob = file.readAll();
    const auto* data = reinterpret_cast<const uint8_t*>(blob.constData());
    const size_t fileLen = size_t(blob.size());
    if (fileLen < 16) {
        res.error = QStringLiteral("Invalid or empty RAT file");
        return res;
    }

    // -----------------------------------------------------------------------
    // Cas 1 : Format RAT classique ("tarH")
    // -----------------------------------------------------------------------
    if (std::memcmp(data, "tarH", 4) == 0) {
        const int w = int(readLE<uint16_t>(data + 4));
        const int h = int(readLE<uint16_t>(data + 6));
        const int nchan = std::clamp<int>(data[10], 1, 4);
        const int bits = int(data[11]);
        if (w <= 0 || h <= 0 || bits != 8) {
            return loadRatViaHoudiniFallback(path, aovName);
        }

        const int tw = 32;
        const int th = 32;
        const int nx = (w + tw - 1) / tw;
        const int ny = (h + th - 1) / th;
        const int nt = nx * ny;
        if (16 + size_t(nt) * 4 > fileLen) {
            return loadRatViaHoudiniFallback(path, aovName);
        }

        auto img = std::make_shared<ImageData>();
        img->path = path;
        img->formatName = QStringLiteral("Houdini RAT (Classic)");
        img->width = w;
        img->height = h;
        img->sourceChannels = nchan;
        img->hasAlpha = (nchan == 2 || nchan == 4);
        img->space = SourceSpace::SRGB;
        img->channelNames = ratChannelNamesForSize(nchan);

        AovLayer baseLayer;
        baseLayer.name = QStringLiteral("RGBA");
        baseLayer.channels = img->channelNames;
        baseLayer.subimage = 0;
        img->aovLayers.append(baseLayer);
        img->activeAov = baseLayer.name;
        img->rgba.assign(size_t(w) * size_t(h) * 4, 0.0f);

        const uint32_t expectedTileBytes = uint32_t(tw * th * nchan);
        for (int t = 0; t < nt; ++t) {
            const uint32_t off = readLE<uint32_t>(data + 16 + size_t(t) * 4);
            if (off + 2 > fileLen)
                return loadRatViaHoudiniFallback(path, aovName);
            const uint16_t clen = readLE<uint16_t>(data + off);
            if (off + 2 + clen > fileLen)
                return loadRatViaHoudiniFallback(path, aovName);

            const QByteArray tbytes = zlibDecompress(data + off + 2, clen, expectedTileBytes);
            if (uint32_t(tbytes.size()) < expectedTileBytes)
                return loadRatViaHoudiniFallback(path, aovName);

            const auto* tp = reinterpret_cast<const uint8_t*>(tbytes.constData());
            const int tx = t % nx;
            const int ty = t / nx;
            const int x0 = tx * tw;
            const int y0 = ty * th;
            const int curTw = std::min(tw, w - x0);
            const int curTh = std::min(th, h - y0);

            for (int ly = 0; ly < curTh; ++ly) {
                const int dstY = (h - 1) - (y0 + ly);
                float* dstRow = &img->rgba[size_t(dstY) * size_t(w) * 4];
                const uint8_t* srcRow = tp + size_t(ly) * size_t(tw) * size_t(nchan);
                for (int lx = 0; lx < curTw; ++lx) {
                    const uint8_t* sp = srcRow + size_t(lx) * size_t(nchan);
                    float* dp = dstRow + size_t(x0 + lx) * 4;
                    if (nchan == 1) {
                        dp[0] = dp[1] = dp[2] = float(sp[0]) / 255.0f;
                        dp[3] = 1.0f;
                    } else if (nchan == 2) {
                        dp[0] = dp[1] = dp[2] = float(sp[0]) / 255.0f;
                        dp[3] = float(sp[1]) / 255.0f;
                    } else if (nchan == 3) {
                        dp[0] = float(sp[0]) / 255.0f;
                        dp[1] = float(sp[1]) / 255.0f;
                        dp[2] = float(sp[2]) / 255.0f;
                        dp[3] = 1.0f;
                    } else {
                        dp[0] = float(sp[0]) / 255.0f;
                        dp[1] = float(sp[1]) / 255.0f;
                        dp[2] = float(sp[2]) / 255.0f;
                        dp[3] = float(sp[3]) / 255.0f;
                    }
                }
            }
        }

        img->metadata.insert(QStringLiteral("File Name"), QFileInfo(path).fileName());
        img->metadata.insert(QStringLiteral("File Path"), path);
        img->metadata.insert(QStringLiteral("Dimensions"), QStringLiteral("%1 x %2").arg(w).arg(h));
        img->metadata.insert(QStringLiteral("Channels"), QStringLiteral("%1 (%2)").arg(nchan).arg(img->channelNames.join(QStringLiteral(", "))));
        img->metadata.insert(QStringLiteral("Pixel Format"), QStringLiteral("uint8"));
        img->metadata.insert(QStringLiteral("File Format"), img->formatName);
        img->metadata.insert(QStringLiteral("Tile Size"), QStringLiteral("32 x 32"));
        img->metadata.insert(QStringLiteral("Color Space"), QStringLiteral("sRGB"));
        img->metadata.insert(QStringLiteral("Active AOV"), img->activeAov);
        img->metadata.insert(QStringLiteral("Memory Footprint"), QStringLiteral("%1 MB").arg(double(img->byteSize()) / (1024.0 * 1024.0), 0, 'f', 2));

        res.image = std::move(img);
        return res;
    }

    // -----------------------------------------------------------------------
    // Cas 2 : Format TBF ("fbtH" v5 / v6)
    // -----------------------------------------------------------------------
    uint32_t ver = 0;
    std::vector<RatChannelHeader> channels;
    std::vector<int> top2D;
    QString optionJson;
    if (!parseTbfHeaders(data, fileLen, ver, channels, top2D, optionJson)) {
        return loadRatViaHoudiniFallback(path, aovName);
    }

    QList<AovLayer> allLayers;
    int selectedPlaneIdx = 0;
    for (size_t p = 0; p < top2D.size(); ++p) {
        const RatChannelHeader& ch = channels[size_t(top2D[p])];
        AovLayer l;
        l.name = friendlyRatPlaneName(ch.rawName, int(p));
        l.channels = ratChannelNamesForSize(ch.tsize);
        l.subimage = int(p);
        allLayers.append(l);
        if (!aovName.isEmpty() && l.name.compare(aovName, Qt::CaseInsensitive) == 0) {
            selectedPlaneIdx = int(p);
        }
    }

    const RatChannelHeader& plane = channels[size_t(top2D[size_t(selectedPlaneIdx)])];
    const int w = int(plane.w);
    const int h = int(plane.h);
    const int tw = int(plane.tw);
    const int th = int(plane.th);
    const int nchan = std::clamp<int>(plane.tsize, 1, 4);
    if (w <= 0 || h <= 0 || tw <= 0 || th <= 0) {
        return loadRatViaHoudiniFallback(path, aovName);
    }

    int bpc = 0;
    QString pixelFormatStr;
    switch (plane.dtype) {
    case 1:
    case 5:
        bpc = 1;
        pixelFormatStr = QStringLiteral("uint8");
        break;
    case 2:
    case 6:
        bpc = 2;
        pixelFormatStr = QStringLiteral("uint16");
        break;
    case 3:
    case 7:
        bpc = 4;
        pixelFormatStr = QStringLiteral("uint32");
        break;
    case 9:
        bpc = 2;
        pixelFormatStr = QStringLiteral("half");
        break;
    case 10:
        bpc = 4;
        pixelFormatStr = QStringLiteral("float");
        break;
    default:
        return loadRatViaHoudiniFallback(path, aovName);
    }

    const int nx = (w + tw - 1) / tw;
    const int ny = (h + th - 1) / th;
    const int nt = nx * ny;
    const uint32_t expectedTtBytes = uint32_t(nt) * 8u;

    QByteArray ttBytes;
    if (plane.off1 + 24 <= fileLen && std::memcmp(data + plane.off1, "ztaD", 4) == 0) {
        const uint32_t tulen = readLE<uint32_t>(data + plane.off1 + 16);
        const uint32_t tclen = readLE<uint32_t>(data + plane.off1 + 20);
        if (plane.off1 + 24 + tclen > fileLen)
            return loadRatViaHoudiniFallback(path, aovName);
        ttBytes = (tclen == tulen)
            ? QByteArray(reinterpret_cast<const char*>(data + plane.off1 + 24), qsizetype(tclen))
            : zlibDecompress(data + plane.off1 + 24, tclen, tulen);
    } else if (plane.off1 < plane.headerPos && plane.headerPos <= fileLen) {
        if (nt == 1 && plane.off1 + 8 <= fileLen) {
            ttBytes = QByteArray(reinterpret_cast<const char*>(data + plane.off1), 8);
        } else {
            ttBytes = zlibDecompress(data + plane.off1, size_t(plane.headerPos - plane.off1), expectedTtBytes);
        }
    }

    if (uint32_t(ttBytes.size()) < expectedTtBytes) {
        return loadRatViaHoudiniFallback(path, aovName);
    }

    std::vector<int64_t> tileOffsets(size_t(nt), 0);
    std::vector<uint64_t> sortedPosOffsets;
    sortedPosOffsets.reserve(size_t(nt) + 1);
    sortedPosOffsets.push_back(plane.off1);

    const auto* ttPtr = reinterpret_cast<const uint8_t*>(ttBytes.constData());
    for (int t = 0; t < nt; ++t) {
        const int64_t o = readLE<int64_t>(ttPtr + size_t(t) * 8);
        tileOffsets[size_t(t)] = o;
        if (o >= 0)
            sortedPosOffsets.push_back(uint64_t(o));
    }
    std::sort(sortedPosOffsets.begin(), sortedPosOffsets.end());
    sortedPosOffsets.erase(std::unique(sortedPosOffsets.begin(), sortedPosOffsets.end()), sortedPosOffsets.end());

    std::vector<QByteArray> decodedTiles;
    decodedTiles.resize(static_cast<size_t>(nt));
    for (int t = 0; t < nt; ++t) {
        const int tx = t % nx;
        const int ty = t / nx;
        const int curTw = std::min((tx + 1) * tw, w) - tx * tw;
        const int curTh = std::min((ty + 1) * th, h) - ty * th;
        const uint32_t expectedLen = uint32_t(curTw * curTh * nchan * bpc);

        const int64_t o = tileOffsets[size_t(t)];
        if (o < 0) {
            const int32_t lo = static_cast<int32_t>(uint64_t(o) & 0xFFFFFFFFu);
            const int refIdx = ~lo;
            if (refIdx < 0 || refIdx >= t || uint32_t(decodedTiles[size_t(refIdx)].size()) < expectedLen)
                return loadRatViaHoudiniFallback(path, aovName);
            decodedTiles[size_t(t)] = decodedTiles[size_t(refIdx)];
        } else {
            const uint64_t uoff = uint64_t(o);
            if (uoff >= fileLen)
                return loadRatViaHoudiniFallback(path, aovName);

            QByteArray tbytes;
            if (uoff + 24 <= fileLen && std::memcmp(data + uoff, "ztaD", 4) == 0) {
                const uint32_t dulen = readLE<uint32_t>(data + uoff + 16);
                const uint32_t dclen = readLE<uint32_t>(data + uoff + 20);
                if (uoff + 24 + dclen > fileLen)
                    return loadRatViaHoudiniFallback(path, aovName);
                tbytes = (dclen == dulen)
                    ? QByteArray(reinterpret_cast<const char*>(data + uoff + 24), qsizetype(dclen))
                    : zlibDecompress(data + uoff + 24, dclen, dulen);
            } else {
                auto it = std::upper_bound(sortedPosOffsets.begin(), sortedPosOffsets.end(), uoff);
                const uint64_t endOff = (it != sortedPosOffsets.end()) ? *it : plane.off1;
                if (endOff <= uoff || endOff > fileLen)
                    return loadRatViaHoudiniFallback(path, aovName);
                const size_t chunkLen = size_t(endOff - uoff);
                tbytes = zlibDecompress(data + uoff, chunkLen, expectedLen);
            }

            if (uint32_t(tbytes.size()) < expectedLen)
                return loadRatViaHoudiniFallback(path, aovName);
            decodedTiles[size_t(t)] = std::move(tbytes);
        }
    }

    // Détermination de l'espace de couleur à partir de OptionData JSON et du type natif
    const bool isFloatFormat = (plane.dtype == 9 || plane.dtype == 10);
    SourceSpace space = isFloatFormat ? SourceSpace::SceneLinear : SourceSpace::SRGB;
    QString detectedSpace;
    QMap<QString, QString> extraMeta;

    if (!optionJson.isEmpty()) {
        QJsonParseError jerr{};
        const QJsonDocument doc = QJsonDocument::fromJson(optionJson.toUtf8(), &jerr);
        if (jerr.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject root = doc.object();
            const QString chanPrefix = QStringLiteral("channel-%1::").arg(plane.idx);
            for (auto it = root.begin(); it != root.end(); ++it) {
                const QString k = it.key();
                if (!k.startsWith(chanPrefix) && !k.startsWith(QLatin1String("channel-2::")))
                    continue;
                const QJsonObject entry = it.value().toObject();
                const QJsonValue val = entry.value(QStringLiteral("value"));
                const QString propName = k.mid(k.indexOf(QLatin1String("::")) + 2);
                if (propName == QLatin1String("cspace")) {
                    const QString cs = val.toString().trimmed();
                    const QString csLower = cs.toLower();
                    if (csLower.contains(QLatin1String("linear")) || csLower.startsWith(QLatin1String("lin_"))) {
                        space = SourceSpace::SceneLinear;
                        if (csLower.contains(QLatin1String("709")) || csLower.contains(QLatin1String("srgb")))
                            detectedSpace = QStringLiteral("Rec.709");
                    } else if (csLower.contains(QLatin1String("acescg"))) {
                        space = SourceSpace::SceneLinear;
                        detectedSpace = QStringLiteral("ACEScg");
                    } else if (csLower.contains(QLatin1String("srgb"))) {
                        space = SourceSpace::SRGB;
                    }
                    extraMeta.insert(QStringLiteral("RAT Color Space"), cs);
                } else if (propName == QLatin1String("SHA1")) {
                    extraMeta.insert(QStringLiteral("RAT SHA1"), val.toString());
                }
            }
        }
    }

    auto img = std::make_shared<ImageData>();
    img->path = path;
    img->formatName = QStringLiteral("Houdini RAT");
    img->width = w;
    img->height = h;
    img->sourceChannels = nchan;
    img->hasAlpha = (nchan == 2 || nchan == 4);
    img->space = space;
    img->sourceColorSpace = detectedSpace;
    img->aovLayers = allLayers;
    img->activeAov = allLayers[selectedPlaneIdx].name;
    img->channelNames = allLayers[selectedPlaneIdx].channels;
    img->rgba.assign(size_t(w) * size_t(h) * 4, 0.0f);

    // Assemblage des tuiles (origine Houdini en bas à gauche -> inversion Y vers haut à gauche)
    for (int t = 0; t < nt; ++t) {
        const int tx = t % nx;
        const int ty = t / nx;
        const int x0 = tx * tw;
        const int y0 = ty * th;
        const int curTw = std::min((tx + 1) * tw, w) - x0;
        const int curTh = std::min((ty + 1) * th, h) - y0;
        const auto* tp = reinterpret_cast<const uint8_t*>(decodedTiles[size_t(t)].constData());

        for (int ly = 0; ly < curTh; ++ly) {
            const int dstY = (h - 1) - (y0 + ly);
            float* dstRow = &img->rgba[size_t(dstY) * size_t(w) * 4];
            const uint8_t* srcRow = tp + size_t(ly) * size_t(curTw) * size_t(nchan) * size_t(bpc);

            for (int lx = 0; lx < curTw; ++lx) {
                const uint8_t* sp = srcRow + size_t(lx) * size_t(nchan) * size_t(bpc);
                float c[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                for (int chIdx = 0; chIdx < nchan; ++chIdx) {
                    const uint8_t* cp = sp + size_t(chIdx) * size_t(bpc);
                    if (plane.dtype == 1 || plane.dtype == 5) {
                        c[chIdx] = float(cp[0]) / 255.0f;
                    } else if (plane.dtype == 2 || plane.dtype == 6) {
                        c[chIdx] = float(readLE<uint16_t>(cp)) / 65535.0f;
                    } else if (plane.dtype == 3 || plane.dtype == 7) {
                        c[chIdx] = float(double(readLE<uint32_t>(cp)) / 4294967295.0);
                    } else if (plane.dtype == 9) {
                        c[chIdx] = halfToFloat(readLE<uint16_t>(cp));
                    } else if (plane.dtype == 10) {
                        c[chIdx] = readLE<float>(cp);
                    }
                }

                float* dp = dstRow + size_t(x0 + lx) * 4;
                if (nchan == 1) {
                    dp[0] = dp[1] = dp[2] = c[0];
                    dp[3] = 1.0f;
                } else if (nchan == 2) {
                    dp[0] = dp[1] = dp[2] = c[0];
                    dp[3] = c[1];
                } else if (nchan == 3) {
                    dp[0] = c[0];
                    dp[1] = c[1];
                    dp[2] = c[2];
                    dp[3] = 1.0f;
                } else {
                    dp[0] = c[0];
                    dp[1] = c[1];
                    dp[2] = c[2];
                    dp[3] = c[3];
                }
            }
        }
    }

    // Comptage des niveaux MIP pour ce plan
    int mipCount = 1;
    int curMip = plane.nextMip;
    while (curMip >= 0 && curMip < int(channels.size()) && mipCount < 32) {
        ++mipCount;
        curMip = channels[size_t(curMip)].nextMip;
    }

    img->metadata.insert(QStringLiteral("File Name"), QFileInfo(path).fileName());
    img->metadata.insert(QStringLiteral("File Path"), path);
    img->metadata.insert(QStringLiteral("Dimensions"), QStringLiteral("%1 x %2").arg(w).arg(h));
    img->metadata.insert(QStringLiteral("Channels"), QStringLiteral("%1 (%2)").arg(nchan).arg(img->channelNames.join(QStringLiteral(", "))));
    img->metadata.insert(QStringLiteral("Pixel Format"), pixelFormatStr);
    img->metadata.insert(QStringLiteral("File Format"), img->formatName);
    img->metadata.insert(QStringLiteral("TBF Version"), QStringLiteral("v%1").arg(ver));
    img->metadata.insert(QStringLiteral("Tile Size"), QStringLiteral("%1 x %2").arg(tw).arg(th));
    img->metadata.insert(QStringLiteral("MIP Levels"), QString::number(mipCount));
    if (top2D.size() > 1) {
        img->metadata.insert(QStringLiteral("TBF File Type"),
                             (top2D.size() == 6) ? QStringLiteral("Environment Cube Map (6 faces)")
                                                 : QStringLiteral("Multi-Plane (%1 planes)").arg(top2D.size()));
    } else {
        img->metadata.insert(QStringLiteral("TBF File Type"), QStringLiteral("2D Raster"));
    }
    img->metadata.insert(QStringLiteral("Color Space"),
                         (space == SourceSpace::SceneLinear) ? QStringLiteral("Scene-Linear")
                                                             : QStringLiteral("sRGB"));
    if (!detectedSpace.isEmpty()) {
        img->metadata.insert(QStringLiteral("Source Color Space"), detectedSpace);
    }
    img->metadata.insert(QStringLiteral("Active AOV"), img->activeAov);
    img->metadata.insert(QStringLiteral("Memory Footprint"),
                         QStringLiteral("%1 MB").arg(double(img->byteSize()) / (1024.0 * 1024.0), 0, 'f', 2));
    for (auto it = extraMeta.begin(); it != extraMeta.end(); ++it) {
        img->metadata.insert(it.key(), it.value());
    }

    res.image = std::move(img);
    return res;
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
    if (isRatFilePath(path))
        return discoverRatAovLayers(path);

    QList<AovLayer> layers;
    try {
        const std::string nativePath = QDir::toNativeSeparators(path).toStdString();
        OIIO::ImageBuf src(nativePath);
        if (!src.init_spec(nativePath, 0, 0))
            return discoverRatAovLayers(path);

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
    if (isRatFilePath(path))
        return loadRatImage(path, aovName);

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

        // Si l'image contient des échantillons Deep (Deep EXR), l'aplatir en buffer 2D
        // pour permettre l'affichage, l'inspection AOV et les vignettes sans crash get_pixels.
        const bool isDeepImage = src.deep();
        OIIO::ImageBuf buf = src;
        if (isDeepImage) {
            OIIO::ImageBuf flat = OIIO::ImageBufAlgo::flatten(src);
            if (flat.has_error() || !flat.initialized()) {
                res.error = QStringLiteral("Cannot flatten deep image: %1")
                                .arg(QString::fromStdString(flat.geterror()));
                return res;
            }
            buf = flat;
        }

        // Orientation EXIF : reorient() ne duplique les pixels que si une
        // rotation est réellement demandée. Sinon une copie superficielle suffit.
        if (buf.spec().get_int_attribute("Orientation", 1) != 1) {
            OIIO::ImageBuf reoriented = OIIO::ImageBufAlgo::reorient(buf);
            if (!reoriented.has_error()) {
                buf = reoriented;
            }
        }

        if (buf.deep()) {
            res.error = QStringLiteral("Deep images cannot be displayed directly as 2D buffers");
            return res;
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
        if (isDeepImage) {
            img->metadata.insert(QStringLiteral("Deep Image"), QStringLiteral("Yes (Flattened for 2D View)"));
        }
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
            // Repli via le chargeur natif Sview (ex: fichiers Houdini .rat)
            const LoadResult loaded = loadImage(inputPath);
            if (!loaded.image || loaded.image->rgba.empty())
                return false;
            OIIO::ImageSpec spec(loaded.image->width, loaded.image->height, 4, OIIO::TypeDesc::FLOAT);
            OIIO::ImageBuf bufFromImg(spec, const_cast<float*>(loaded.image->rgba.data()));
            if (halfRes) {
                const int newW = std::max(1, spec.width / 2);
                const int newH = std::max(1, spec.height / 2);
                OIIO::ROI roi(0, newW, 0, newH, 0, 1, 0, 4);
                OIIO::ImageBuf dst = OIIO::ImageBufAlgo::resize(bufFromImg, "", 0.0f, roi);
                if (dst.has_error())
                    return false;
                return dst.write(outNative);
            }
            return bufFromImg.write(outNative);
        }

        OIIO::ImageBuf buf = src;
        if (src.deep()) {
            OIIO::ImageBuf flat = OIIO::ImageBufAlgo::flatten(src);
            if (!flat.has_error() && flat.initialized()) {
                buf = flat;
            }
        }

        if (halfRes) {
            const int newW = std::max(1, buf.spec().width / 2);
            const int newH = std::max(1, buf.spec().height / 2);
            OIIO::ROI roi(0, newW, 0, newH, 0, 1, 0, buf.spec().nchannels);
            OIIO::ImageBuf dst = OIIO::ImageBufAlgo::resize(buf, "", 0.0f, roi);
            if (dst.has_error()) {
                return false;
            }
            return dst.write(outNative);
        }

        return buf.write(outNative);
    } catch (...) {
        return false;
    }
}

}  // namespace sview
