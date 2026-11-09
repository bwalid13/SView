#include "ImageCache.h"

#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QThread>

#include <algorithm>

namespace sview {

ImageCache::ImageCache(QObject* parent) : QObject(parent)
{
    // High-performance sequence playback: use up to 4 concurrent worker threads
    const int threadCount = std::clamp(QThread::idealThreadCount() / 2, 2, 8);
    m_pool.setMaxThreadCount(threadCount);
}

ImageCache::~ImageCache()
{
    m_pool.waitForDone();
}

void ImageCache::setBudgetBytes(std::size_t bytes)
{
    m_budget = bytes;
    trim();
}

std::size_t ImageCache::budgetBytes() const
{
    return m_budget;
}

void ImageCache::setBudgetMB(std::size_t mb)
{
    setBudgetBytes(static_cast<std::size_t>(mb) * 1024ULL * 1024ULL);
}

std::size_t ImageCache::budgetMB() const
{
    return m_budget / (1024ULL * 1024ULL);
}

void ImageCache::setBudgetGB(double gb)
{
    const auto bytes = static_cast<std::size_t>(gb * 1024.0 * 1024.0 * 1024.0);
    setBudgetBytes(bytes);
}

double ImageCache::budgetGB() const
{
    return static_cast<double>(m_budget) / (1024.0 * 1024.0 * 1024.0);
}

std::size_t ImageCache::usedBytes() const
{
    return m_bytes;
}

std::size_t ImageCache::usedMB() const
{
    return m_bytes / (1024ULL * 1024ULL);
}

int ImageCache::cachedCount() const
{
    return static_cast<int>(m_images.size());
}

void ImageCache::setOIIOCacheBudgetMB(double mb)
{
    // 1. Remove single-image 32GB safety limit (0 = unlimited)
    OIIO::attribute("limits:imagesize_MB", 0);

    // 2. Set memory limit on OIIO's shared ImageCache tile cache
    auto* oiioCache = OIIO::ImageCache::create(/*shared=*/true);
    if (oiioCache) {
        oiioCache->attribute("max_memory_MB", static_cast<float>(mb));
    }

    // 3. Set global attribute for subsystems reading global configuration
    OIIO::attribute("max_memory_MB", static_cast<float>(mb));
}

void ImageCache::prefetch(const QStringList& paths)
{
    for (const QString& path : paths) {
        request(path);
    }
}

void ImageCache::clear()
{
    m_images.clear();
    m_lru.clear();
    m_bytes = 0;
}

ImagePtr ImageCache::get(const QString& path)
{
    const auto it = m_images.constFind(path);
    if (it == m_images.constEnd())
        return {};
    m_lru.remove(path);
    m_lru.push_front(path);
    return *it;
}

bool ImageCache::isCached(const QString& path) const
{
    if (path.isEmpty())
        return false;
    return m_images.contains(path);
}

void ImageCache::put(const QString& path, ImagePtr img)
{
    if (path.isEmpty() || !img)
        return;

    if (auto existing = m_images.value(path)) {
        const std::size_t oldSz = existing->byteSize();
        m_bytes = (m_bytes > oldSz) ? (m_bytes - oldSz) : 0;
    }

    m_images.insert(path, img);
    m_bytes += img->byteSize();
    m_lru.remove(path);
    m_lru.push_front(path);
    trim();

    emit loaded(path);
}

void ImageCache::request(const QString& path)
{
    if (path.isEmpty() || m_images.contains(path) || m_inflight.contains(path))
        return;

    m_inflight.insert(path);

    auto* watcher = new QFutureWatcher<LoadResult>(this);
    connect(watcher, &QFutureWatcher<LoadResult>::finished, this, [this, watcher]() {
        const LoadResult result = watcher->result();
        watcher->deleteLater();
        onFinished(result);
    });
    watcher->setFuture(QtConcurrent::run(&m_pool, [path]() { return loadImage(path); }));
}

void ImageCache::setPinned(const QStringList& paths)
{
    m_pinned = QSet<QString>(paths.begin(), paths.end());
    trim();
}

void ImageCache::onFinished(const LoadResult& r)
{
    m_inflight.remove(r.path);

    if (!r.image) {
        emit failed(r.path, r.error);
        return;
    }

    // If already in cache (e.g. reload), subtract previous size first
    if (auto existing = m_images.value(r.path)) {
        const std::size_t oldSz = existing->byteSize();
        m_bytes = (m_bytes > oldSz) ? (m_bytes - oldSz) : 0;
    }

    m_images.insert(r.path, r.image);
    m_bytes += r.image->byteSize();
    m_lru.remove(r.path);
    m_lru.push_front(r.path);
    trim();

    emit loaded(r.path);
}

void ImageCache::trim()
{
    while (m_bytes > m_budget) {
        // Find the oldest unpinned entry from the back of the LRU
        auto it = std::find_if(m_lru.rbegin(), m_lru.rend(), [this](const QString& path) {
            return !m_pinned.contains(path);
        });
        if (it == m_lru.rend()) {
            break; // All cached images are pinned; cannot evict further
        }

        const QString path = *it;
        const ImagePtr img = m_images.take(path);
        if (img) {
            const std::size_t sz = img->byteSize();
            m_bytes = (m_bytes > sz) ? (m_bytes - sz) : 0;
        }
        m_lru.erase(std::prev(it.base()));
    }
}

}  // namespace sview
