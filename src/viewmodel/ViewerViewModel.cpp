#include "ViewerViewModel.h"

#include "../model/ImageLoader.h"
#include "../model/SettingsModel.h"

#include <QDebug>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

namespace sview {

namespace {
constexpr double kMinZoom = 0.005;
constexpr double kMaxZoom = 256.0;
const char* const kChannelNames[] = {"RGB", "R", "G", "B", "A", "Luma"};

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kHalfPi = 0.5f * kPi;
constexpr float kPitchEpsilon = 0.001f;
constexpr float kMinPitch = -kHalfPi + kPitchEpsilon;
constexpr float kMaxPitch = kHalfPi - kPitchEpsilon;
constexpr float kMinFov = 15.0f * (kPi / 180.0f);
constexpr float kMaxFov = 120.0f * (kPi / 180.0f);
constexpr float kDefaultFov = 35.0f * (kPi / 180.0f);

inline float normalizeYaw(float y) {
    y = std::fmod(y + kPi, kTwoPi);
    if (y < 0.0f)
        y += kTwoPi;
    return y - kPi;
}

inline float clampPitch(float p) {
    return std::clamp(p, kMinPitch, kMaxPitch);
}

inline float clampFov(float f) {
    return std::clamp(f, kMinFov, kMaxFov);
}
}  // namespace

ViewerViewModel::ViewerViewModel(QObject* parent) : QObject(parent), m_cache(this)
{
    auto& settings = SettingsModel::instance();
    const std::size_t budgetMB = settings.cacheBudgetMB();
    m_cache.setBudgetMB(budgetMB);
    if (settings.oiioUnlimitedImageSize()) {
        ImageCache::setOIIOCacheBudgetMB(static_cast<double>(budgetMB));
    }

    connect(&settings, &SettingsModel::cacheBudgetChanged, this, [this](std::size_t mb) {
        m_cache.setBudgetMB(mb);
        ImageCache::setOIIOCacheBudgetMB(static_cast<double>(mb));
    });

    connect(&m_cache, &ImageCache::loaded, this, &ViewerViewModel::onLoaded);
    connect(&m_cache, &ImageCache::failed, this, &ViewerViewModel::onFailed);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void ViewerViewModel::openPath(const QString& path)
{
    m_fit = true;
    m_flipH = false;
    m_flipV = false;
    m_rotation = 0;
    emit rotationChanged(0);
    emit displayChanged();

    if (!m_folder.open(path) || m_folder.isEmpty()) {
        m_error = tr("No supported image in: %1").arg(path);
        m_image.reset();
        m_probeData = PixelProbeData{};
        m_probeText.clear();
        m_probeValues.clear();
        emit probeUpdated(m_probeData);
        emit imageChanged();
        emit overlayChanged();
        return;
    }
    m_currentAov = QStringLiteral("RGBA");
    showCurrent();
}

void ViewerViewModel::closeImage()
{
    m_folder.clear();
    m_target.clear();
    m_targetB.clear();
    m_image.reset();
    m_imageB.reset();
    m_error.clear();
    if (m_loading) {
        m_loading = false;
        emit loadingChanged(false);
    }
    m_probeData = PixelProbeData{};
    m_probeText.clear();
    m_probeValues.clear();
    m_flipH = false;
    m_flipV = false;
    m_rotation = 0;
    emit rotationChanged(0);
    emit probeUpdated(m_probeData);
    emit imageChanged();
    emit imageBChanged();
    emit overlayChanged();
    emit displayChanged();
}

void ViewerViewModel::showFrame(const QString& path)
{
    if (path.isEmpty() || path == m_target)
        return;
    m_target = path;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged(true);
    }
    m_error.clear();
    emit overlayChanged();

    auto cached = m_cache.get(path);
    if (cached) {
        applyImage(cached);
    } else {
        m_cache.request(path);
    }
}

void ViewerViewModel::step(int delta)
{
    if (m_folder.isEmpty())
        return;
    m_folder.step(delta);
    showCurrent();
}

void ViewerViewModel::first()
{
    if (m_folder.isEmpty())
        return;
    m_folder.first();
    showCurrent();
}

void ViewerViewModel::last()
{
    if (m_folder.isEmpty())
        return;
    m_folder.last();
    showCurrent();
}

QStringList ViewerViewModel::neighborhood() const
{
    QStringList list;
    for (int off : {0, +1, -1}) {
        const QString p = m_folder.pathAt(off);
        if (!p.isEmpty() && !list.contains(p))
            list << p;
    }
    return list;
}

void ViewerViewModel::showCurrent()
{
    const QString path = m_folder.currentPath();
    if (path.isEmpty())
        return;

    m_target = path;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged(true);
    }
    m_error.clear();
    emit overlayChanged();

    auto cached = m_cache.get(path);
    if (cached) {
        applyImage(cached);
        prefetchNeighbors();
    } else {
        m_cache.request(path);
    }
}

void ViewerViewModel::prefetchNeighbors()
{
    // Le prefetch ne doit pas manger le budget mémoire : sur une image 8K,
    // trois voisins en pleine résolution coûtent ~1,7 Go. On les charge
    // seulement s'ils tiennent dans 35 % du budget, et de toute façon sous
    // un plafond absolu de 1 Go.
    constexpr double kPrefetchShare = 0.35;
    constexpr std::size_t kPrefetchCeiling = 1024ULL * 1024ULL * 1024ULL;
    const std::size_t budget = m_cache.budgetBytes();

    if (m_image && budget > 0) {
        const std::size_t limit = std::min<std::size_t>(
            static_cast<std::size_t>(static_cast<double>(budget) * kPrefetchShare),
            kPrefetchCeiling);
        const double needed = 3.0 * static_cast<double>(m_image->byteSize());
        if (needed > static_cast<double>(limit)) {
            qDebug().nospace() << "SView: prefetch ignoré (voisins ~"
                               << qRound64(needed / (1024.0 * 1024.0)) << " Mo, plafond "
                               << qRound64(static_cast<double>(limit) / (1024.0 * 1024.0)) << " Mo)";
            return;
        }
    }

    m_cache.prefetch(neighborhood());
}

void ViewerViewModel::onLoaded(const QString& path)
{
    if (path == m_target) {
        auto img = m_cache.get(path);
        if (img)
            applyImage(img);
        prefetchNeighbors();
    }
}

void ViewerViewModel::onFailed(const QString& path, const QString& error)
{
    if (path == m_target) {
        if (m_loading) {
            m_loading = false;
            emit loadingChanged(false);
        }
        m_error = error;
        m_image.reset();
        m_probeData = PixelProbeData{};
        m_probeText.clear();
        m_probeValues.clear();
        emit probeUpdated(m_probeData);
        emit imageChanged();
        emit overlayChanged();
    }
}

void ViewerViewModel::applyImage(const ImagePtr& img)
{
    const bool isSameFile = (m_image && img && !img->path.isEmpty() && m_image->path == img->path);
    if (!isSameFile) {
        m_flipH = false;
        m_flipV = false;
        m_rotation = 0;
        emit rotationChanged(0);
        emit displayChanged();
    }

    m_image = img;
    m_error.clear();
    m_target.clear();
    if (m_loading) {
        m_loading = false;
        emit loadingChanged(false);
    }
    m_probeData = PixelProbeData{};
    m_probeText.clear();
    m_probeValues.clear();
    emit probeUpdated(m_probeData);

    if (m_fit)
        fitInternal();
    emit imageChanged();
    emit aovsChanged();
    emit overlayChanged();
}

// ---------------------------------------------------------------------------
// Géométrie (zoom / pan / fit)
// ---------------------------------------------------------------------------

void ViewerViewModel::setViewportSize(const QSizeF& size)
{
    if (size == m_viewport || size.width() <= 0 || size.height() <= 0)
        return;
    m_viewport = size;
    if (m_fit)
        fitInternal();
    emit transformChanged();
}

void ViewerViewModel::fitInternal()
{
    if (!m_image || m_viewport.width() <= 0 || m_viewport.height() <= 0)
        return;

    const bool transposed = (m_rotation == 90 || m_rotation == 270);
    const double dispW = transposed ? double(m_image->height) : double(m_image->width);
    const double dispH = transposed ? double(m_image->width) : double(m_image->height);

    const double sx = m_viewport.width() / dispW;
    const double sy = m_viewport.height() / dispH;
    m_zoom = std::clamp(std::min(sx, sy), kMinZoom, kMaxZoom);
    m_pan = QPointF(0, 0);
}

void ViewerViewModel::fitToWindow()
{
    m_fit = true;
    fitInternal();
    emit transformChanged();
    emit overlayChanged();
}

void ViewerViewModel::resetView()
{
    m_fit = true;
    fitInternal();
    emit transformChanged();
    emit overlayChanged();
}

void ViewerViewModel::actualSize()
{
    m_fit = false;
    m_zoom = 1.0;
    m_pan = QPointF(0, 0);
    emit transformChanged();
    emit overlayChanged();
}

void ViewerViewModel::toggleFitActual()
{
    if (m_fit)
        actualSize();
    else
        fitToWindow();
}

void ViewerViewModel::zoomBy(double factor, const QPointF& anchor)
{
    if (!m_image)
        return;
    const double newZoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
    const double f = newZoom / m_zoom;

    // Garde le point sous le curseur fixe
    const QPointF center(m_viewport.width() / 2.0, m_viewport.height() / 2.0);
    const QPointF rel = anchor - center;
    m_pan = rel - (rel - m_pan) * f;
    m_zoom = newZoom;
    m_fit = false;

    emit transformChanged();
    emit overlayChanged();
}

void ViewerViewModel::panBy(const QPointF& delta)
{
    if (!m_image)
        return;
    m_pan += delta;
    m_fit = false;
    emit transformChanged();
}

void ViewerViewModel::setZoom(double z)
{
    m_zoom = std::clamp(z, kMinZoom, kMaxZoom);
    m_fit = false;
    emit transformChanged();
    emit overlayChanged();
}

void ViewerViewModel::setImage(const ImagePtr& img)
{
    m_flipH = false;
    m_flipV = false;
    m_rotation = 0;
    emit rotationChanged(0);
    emit displayChanged();
    applyImage(img);
}

// ---------------------------------------------------------------------------
// Affichage : Exposition, Gamma, Canaux, AOV
// ---------------------------------------------------------------------------

float ViewerViewModel::exposureMultiplier() const
{
    return float(std::pow(2.0, m_exposure));
}

void ViewerViewModel::setExposure(double stops)
{
    m_exposure = std::clamp(stops, -16.0, 16.0);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::adjustExposure(double stops)
{
    m_exposure = std::clamp(m_exposure + stops, -16.0, 16.0);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::resetExposure()
{
    m_exposure = 0.0;
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::setGamma(double gamma)
{
    m_gamma = std::clamp(gamma, 0.1, 10.0);
    emit gammaChanged(m_gamma);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::resetGamma()
{
    m_gamma = 1.0;
    emit gammaChanged(m_gamma);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::setChannel(ChannelMode mode)
{
    m_channel = mode;
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::setAov(const QString& aovName)
{
    m_currentAov = aovName.isEmpty() ? QStringLiteral("RGBA") : aovName;
    if (m_image && !m_image->path.isEmpty()) {
        const LoadResult res = loadAov(m_image->path, m_currentAov);
        if (res.image) {
            applyImage(res.image);
        }
    }
    emit aovChanged(m_currentAov);
    emit displayChanged();
    emit overlayChanged();
}

QStringList ViewerViewModel::availableAovs() const
{
    QStringList result;
    if (!m_image)
        return result;
    for (const auto& layer : m_image->aovLayers) {
        if (!result.contains(layer.name, Qt::CaseInsensitive))
            result << layer.name;
    }
    return result;
}

QList<AovLayer> ViewerViewModel::aovLayers() const
{
    return m_image ? m_image->aovLayers : QList<AovLayer>{};
}

void ViewerViewModel::cycleView(int direction)
{
    m_colors.cycleView(direction);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::setColorSpace(const QString& name)
{
    m_colors.setColorSpace(name);
    m_gpuKey.clear();
    m_gpu.reset();
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::toggleInfo()
{
    m_infoVisible = !m_infoVisible;
    emit overlayChanged();
}

// ---------------------------------------------------------------------------
// A/B Wipe, Image B, Checkerboard
// ---------------------------------------------------------------------------

void ViewerViewModel::setWipeEnabled(bool enabled)
{
    m_wipeEnabled = enabled;
    emit wipeChanged();
    emit displayChanged();
}

void ViewerViewModel::setWipeMode(int mode)
{
    m_wipeMode = std::clamp(mode, 0, 2);
    emit wipeChanged();
    emit displayChanged();
}

void ViewerViewModel::setWipePos(float pos)
{
    m_wipePos = std::clamp(pos, 0.0f, 1.0f);
    emit wipeChanged();
    emit displayChanged();
}

void ViewerViewModel::openImageB(const QString& path)
{
    if (path.isEmpty()) {
        clearImageB();
        return;
    }
    m_targetB = path;
    const LoadResult res = loadImage(path);
    if (res.image) {
        m_error.clear();
        m_imageB = res.image;
        emit imageBChanged();
        emit displayChanged();
        emit overlayChanged();
    } else {
        m_error = res.error.isEmpty() ? tr("Cannot open Image B") : res.error;
        emit overlayChanged();
    }
}

void ViewerViewModel::clearImageB()
{
    m_imageB.reset();
    m_targetB.clear();
    emit imageBChanged();
    emit displayChanged();
}

void ViewerViewModel::setImageB(const ImagePtr& img)
{
    m_imageB = img;
    m_targetB.clear();
    emit imageBChanged();
    emit displayChanged();
}

void ViewerViewModel::setCheckerboardEnabled(bool enabled)
{
    m_checkerboard = enabled;
    emit checkerboardChanged(m_checkerboard);
    emit displayChanged();
}

void ViewerViewModel::toggleCheckerboard()
{
    setCheckerboardEnabled(!m_checkerboard);
}

// ---------------------------------------------------------------------------
// Flip
// ---------------------------------------------------------------------------

void ViewerViewModel::setFlipH(bool flip)
{
    if (m_flipH == flip)
        return;
    m_flipH = flip;
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::setFlipV(bool flip)
{
    if (m_flipV == flip)
        return;
    m_flipV = flip;
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::setRotation(int degrees)
{
    degrees = ((degrees % 360) + 360) % 360;
    if (degrees % 90 != 0) {
        degrees = (degrees / 90) * 90;
    }
    if (m_rotation == degrees)
        return;
    m_rotation = degrees;
    if (m_fit)
        fitInternal();
    emit rotationChanged(m_rotation);
    emit transformChanged();
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::pivot()
{
    setRotation((m_rotation + 90) % 360);
}

void ViewerViewModel::resetTransforms()
{
    const bool changed = (m_flipH || m_flipV || m_rotation != 0);
    m_flipH = false;
    m_flipV = false;
    m_rotation = 0;
    if (changed) {
        emit rotationChanged(0);
        emit displayChanged();
        emit overlayChanged();
    }
}

// ---------------------------------------------------------------------------
// 360° Panoramique & Caméra
// ---------------------------------------------------------------------------

void ViewerViewModel::set360Mode(bool enabled)
{
    if (m_mode360 == enabled)
        return;
    m_mode360 = enabled;
    emit mode360Changed(m_mode360);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::toggle360Mode()
{
    set360Mode(!m_mode360);
}

void ViewerViewModel::setYaw(float yaw)
{
    const float newYaw = normalizeYaw(yaw);
    if (std::abs(newYaw - m_yaw) < 1e-6f)
        return;
    m_yaw = newYaw;
    emit camera360Changed(m_yaw, m_pitch, m_fov);
    emit displayChanged();
}

void ViewerViewModel::setPitch(float pitch)
{
    const float newPitch = clampPitch(pitch);
    if (std::abs(newPitch - m_pitch) < 1e-6f)
        return;
    m_pitch = newPitch;
    emit camera360Changed(m_yaw, m_pitch, m_fov);
    emit displayChanged();
}

void ViewerViewModel::setFov(float fov)
{
    const float newFov = clampFov(fov);
    if (std::abs(newFov - m_fov) < 1e-6f)
        return;
    m_fov = newFov;
    emit camera360Changed(m_yaw, m_pitch, m_fov);
    emit displayChanged();
}

void ViewerViewModel::rotate360(float deltaYaw, float deltaPitch)
{
    m_yaw = normalizeYaw(m_yaw + deltaYaw);
    m_pitch = clampPitch(m_pitch + deltaPitch);
    emit camera360Changed(m_yaw, m_pitch, m_fov);
    emit displayChanged();
}

void ViewerViewModel::resetCamera360()
{
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_fov = kDefaultFov;
    emit camera360Changed(m_yaw, m_pitch, m_fov);
    emit displayChanged();
}

void ViewerViewModel::setShowPreviewSpheres(bool show)
{
    if (m_showSpheres == show)
        return;
    m_showSpheres = show;
    emit previewSpheresChanged(m_showSpheres);
    emit displayChanged();
    emit overlayChanged();
}

void ViewerViewModel::togglePreviewSpheres()
{
    setShowPreviewSpheres(!m_showSpheres);
}

void ViewerViewModel::setRigCorner(int corner)
{
    corner = std::clamp(corner, 0, 3);
    if (m_rigCorner == corner)
        return;
    m_rigCorner = corner;
    emit rigChanged();
}

void ViewerViewModel::setShowColorChart(bool show)
{
    if (m_showChart == show)
        return;
    m_showChart = show;
    emit rigChanged();
}

// ---------------------------------------------------------------------------
// OCIO GPU Processor
// ---------------------------------------------------------------------------

QString ViewerViewModel::gpuProcessorKey() const
{
    const SourceSpace space = m_image ? m_image->space : SourceSpace::SceneLinear;
    const QString hint = m_image ? m_image->sourceColorSpace : QString();
    return QStringLiteral("%1|%2|%3|%4")
        .arg(m_colors.display(), m_colors.view())
        .arg(int(space))
        .arg(hint);
}

OCIO::ConstGPUProcessorRcPtr ViewerViewModel::gpuProcessor()
{
    const QString key = gpuProcessorKey();
    if (key != m_gpuKey) {
        m_gpuKey = key;
        m_gpu = m_colors.gpuProcessor(m_image ? m_image->space : SourceSpace::SceneLinear,
                                      m_image ? m_image->sourceColorSpace : QString());
    }
    return m_gpu;
}

// ---------------------------------------------------------------------------
// Sonde pixel + texte d'overlay
// ---------------------------------------------------------------------------

void ViewerViewModel::probe(const QPointF& vp)
{
    if (!m_image) {
        m_probeText.clear();
        m_probeValues.clear();
        m_probeData = PixelProbeData{};
        emit probeUpdated(m_probeData);
        emit overlayChanged();
        return;
    }

    // Détermine si on sonde l'image A ou l'image B en mode Wipe
    bool sampleImageB = false;
    if (m_wipeEnabled && m_imageB) {
        if (m_wipeMode == 0) { // Split vertical
            const double splitX = double(m_wipePos) * m_viewport.width();
            sampleImageB = (vp.x() > splitX);
        } else if (m_wipeMode == 1) { // Split horizontal
            const double splitY = double(m_wipePos) * m_viewport.height();
            sampleImageB = (vp.y() > splitY);
        }
    }

    const ImagePtr targetImg = (sampleImageB && m_imageB) ? m_imageB : m_image;
    int x = -1;
    int y = -1;

    if (m_mode360) {
        const double w = m_viewport.width();
        const double h = m_viewport.height();
        if (w > 0.0 && h > 0.0 && vp.x() >= 0 && vp.y() >= 0 && vp.x() <= w && vp.y() <= h) {
            const double aspect = w / h;
            const double tanHalfFov = std::tan(double(m_fov) * 0.5);
            const double ndcX = (vp.x() / w - 0.5) * 2.0;
            const double ndcY = (vp.y() / h - 0.5) * 2.0;

            const double rx = ndcX * aspect * tanHalfFov;
            const double ry = -ndcY * tanHalfFov;
            const double rz = 1.0;
            const double invLen = 1.0 / std::sqrt(rx * rx + ry * ry + rz * rz);
            const double camX = rx * invLen;
            const double camY = ry * invLen;
            const double camZ = rz * invLen;

            // Rotation Pitch puis Yaw
            const double cp = std::cos(double(m_pitch)), sp = std::sin(double(m_pitch));
            const double v1x = camX;
            const double v1y = camY * cp + camZ * sp;
            const double v1z = -camY * sp + camZ * cp;

            const double cy = std::cos(double(m_yaw)), sy = std::sin(double(m_yaw));
            const double worldX = v1x * cy + v1z * sy;
            const double worldY = v1y;
            const double worldZ = -v1x * sy + v1z * cy;

            constexpr double kPiD = 3.14159265358979323846;
            double u = 0.5 + std::atan2(worldX, worldZ) / (2.0 * kPiD);
            u = u - std::floor(u);
            double v = 0.5 - std::asin(std::clamp(worldY, -1.0, 1.0)) / kPiD;
            v = std::clamp(v, 0.0, 1.0);

            x = std::clamp(int(std::floor(u * double(targetImg->width))), 0, targetImg->width - 1);
            y = std::clamp(int(std::floor(v * double(targetImg->height))), 0, targetImg->height - 1);
        }
    } else {
        const QPointF center(m_viewport.width() / 2.0, m_viewport.height() / 2.0);
        double vx = (vp.x() - center.x() - m_pan.x()) / m_zoom;
        double vy = (vp.y() - center.y() - m_pan.y()) / m_zoom;

        if (m_flipH) vx = -vx;
        if (m_flipV) vy = -vy;

        double dx = vx, dy = vy;
        if (m_rotation == 90) {
            dx = vy;
            dy = -vx;
        } else if (m_rotation == 180) {
            dx = -vx;
            dy = -vy;
        } else if (m_rotation == 270) {
            dx = -vy;
            dy = vx;
        }

        const double origX = dx + double(targetImg->width) * 0.5;
        const double origY = dy + double(targetImg->height) * 0.5;
        const int px = static_cast<int>(std::floor(origX));
        const int py = static_cast<int>(std::floor(origY));

        if (px < 0 || px >= targetImg->width || py < 0 || py >= targetImg->height) {
            x = -1;
            y = -1;
        } else {
            x = px;
            y = py;
        }
    }

    if (x < 0 || y < 0 || x >= targetImg->width || y >= targetImg->height) {
        m_probeText.clear();
        m_probeValues.clear();
        m_probeData = PixelProbeData{};
    } else {
        const size_t idx = (size_t(y) * size_t(targetImg->width) + size_t(x)) * 4;
        if (idx + 3 < targetImg->rgba.size()) {
            const float* p = &targetImg->rgba[idx];
            m_probeData.hasPixel = true;
            m_probeData.x = x;
            m_probeData.y = y;
            m_probeData.u = (static_cast<float>(x) + 0.5f) / static_cast<float>(targetImg->width);
            m_probeData.v = (static_cast<float>(y) + 0.5f) / static_cast<float>(targetImg->height);

            float r = p[0];
            float g = p[1];
            float b = p[2];
            float a = p[3];

            if (m_wipeEnabled && m_imageB && m_wipeMode == 2) {
                int xB = x, yB = y;
                if (m_imageB->width != targetImg->width || m_imageB->height != targetImg->height) {
                    xB = std::clamp(int(m_probeData.u * float(m_imageB->width)), 0, m_imageB->width - 1);
                    yB = std::clamp(int(m_probeData.v * float(m_imageB->height)), 0, m_imageB->height - 1);
                }
                const size_t idxB = (size_t(yB) * size_t(m_imageB->width) + size_t(xB)) * 4;
                if (idxB + 3 < m_imageB->rgba.size()) {
                    const float* pB = &m_imageB->rgba[idxB];
                    const float alpha = std::clamp(m_wipePos, 0.0f, 1.0f);
                    r = (1.0f - alpha) * r + alpha * pB[0];
                    g = (1.0f - alpha) * g + alpha * pB[1];
                    b = (1.0f - alpha) * b + alpha * pB[2];
                    a = (1.0f - alpha) * a + alpha * pB[3];
                }
            }

            m_probeData.r = r;
            m_probeData.g = g;
            m_probeData.b = b;
            m_probeData.a = a;
            m_probeData.luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

            const int ir = std::clamp(static_cast<int>(std::round(r * 255.0f)), 0, 255);
            const int ig = std::clamp(static_cast<int>(std::round(g * 255.0f)), 0, 255);
            const int ib = std::clamp(static_cast<int>(std::round(b * 255.0f)), 0, 255);
            m_probeData.hexCode = QString::asprintf("#%02X%02X%02X", ir, ig, ib);

            m_probeValues = QStringLiteral("%1 %2 %3 %4")
                                .arg(r, 0, 'g', 6).arg(g, 0, 'g', 6)
                                .arg(b, 0, 'g', 6).arg(a, 0, 'g', 6);
            m_probeText = QStringLiteral("x=%1 y=%2   R %3  G %4  B %5  A %6")
                              .arg(x).arg(y)
                              .arg(r, 0, 'f', 4).arg(g, 0, 'f', 4)
                              .arg(b, 0, 'f', 4).arg(a, 0, 'f', 4);
        } else {
            m_probeText.clear();
            m_probeValues.clear();
            m_probeData = PixelProbeData{};
        }
    }
    emit probeUpdated(m_probeData);
    emit overlayChanged();
}

QString ViewerViewModel::overlayText() const
{
    QStringList lines;

    if (!m_error.isEmpty())
        lines << m_error;
    else if (m_loading)
        lines << tr("Loading %1...").arg(QFileInfo(m_target).fileName());

    if (m_image && m_error.isEmpty() && !m_loading) {
        lines << QStringLiteral("%1   [%2/%3]")
                     .arg(QFileInfo(m_image->path).fileName())
                     .arg(m_folder.index() + 1)
                     .arg(m_folder.count());
        lines << QStringLiteral("%1 x %2   %3 ch   %4   %5%")
                     .arg(m_image->width).arg(m_image->height)
                     .arg(m_image->sourceChannels)
                     .arg(m_image->formatName)
                     .arg(m_zoom * 100.0, 0, 'f', 0);
        if (!m_image->sourceColorSpace.isEmpty()) {
            lines << QStringLiteral("Source color space: %1").arg(m_image->sourceColorSpace);
        }
        lines << QStringLiteral("Exposure %1 EV   Gamma %2   Channel %3   View %4")
                     .arg(m_exposure, 0, 'f', 2)
                     .arg(m_gamma, 0, 'f', 2)
                     .arg(QString::fromLatin1(kChannelNames[int(m_channel)]))
                     .arg(m_colors.isValid() ? m_colors.view() : tr("(no OCIO)"));
        if (!m_currentAov.isEmpty() && m_currentAov != QStringLiteral("RGBA")) {
            lines << QStringLiteral("AOV: %1").arg(m_currentAov);
        }
        if (m_wipeEnabled && m_imageB) {
            const QString modeStr = (m_wipeMode == 0) ? QStringLiteral("Vertical")
                                  : (m_wipeMode == 1) ? QStringLiteral("Horizontal")
                                                      : QStringLiteral("Onion Skin");
            lines << QStringLiteral("Wipe: %1 vs %2 (%3% %4)")
                         .arg(QFileInfo(m_image->path).fileName())
                         .arg(QFileInfo(m_imageB->path).fileName())
                         .arg(int(std::round(m_wipePos * 100.0f)))
                         .arg(modeStr);
        }
        if (m_mode360) {
            constexpr float kRadToDeg = 180.0f / 3.14159265f;
            lines << QStringLiteral("360° Mode: Yaw %1°  Pitch %2°  FOV %3°%4")
                         .arg(m_yaw * kRadToDeg, 0, 'f', 1)
                         .arg(m_pitch * kRadToDeg, 0, 'f', 1)
                         .arg(m_fov * kRadToDeg, 0, 'f', 1)
                         .arg(m_showSpheres ? QStringLiteral("  [Lookdev Spheres]") : QString());
        }
        if (!m_probeText.isEmpty())
            lines << m_probeText;
    }
    return lines.join('\n');
}

QString ViewerViewModel::windowTitle() const
{
    if (m_image)
        return QStringLiteral("%1 - SView").arg(QFileInfo(m_image->path).fileName());
    return QStringLiteral("SView");
}

}  // namespace sview
