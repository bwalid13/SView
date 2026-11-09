#pragma once

#include "ImageData.h"
#include "ImageLoader.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThreadPool>

#include <cstddef>
#include <list>

namespace sview {

// Thread-safe LRU image cache for decoded float images with asynchronous loading.
// All cache state transitions occur on the main thread; loadImage() runs in workers.
class ImageCache : public QObject {
    Q_OBJECT
public:
    explicit ImageCache(QObject* parent = nullptr);
    ~ImageCache() override;

    // 64-bit byte budget (e.g. 64ULL * 1024ULL * 1024ULL * 1024ULL)
    void setBudgetBytes(std::size_t bytes);
    std::size_t budgetBytes() const;

    // Megabyte and Gigabyte conveniences
    void setBudgetMB(std::size_t mb);
    std::size_t budgetMB() const;

    void setBudgetGB(double gb);
    double budgetGB() const;

    std::size_t usedBytes() const;
    std::size_t usedMB() const;
    int cachedCount() const;

    // Configures OIIO shared tile cache and unlocks single-image limits
    static void setOIIOCacheBudgetMB(double mb);

    // Lookahead prefetching for timeline playback
    void prefetch(const QStringList& paths);

    // Returns image if present in cache and promotes it to MRU
    ImagePtr get(const QString& path);

    // Checks whether an image at the given path is currently resident in the RAM cache
    bool isCached(const QString& path) const;

    // Directly inserts an image into cache (for testing or synchronous loads)
    void put(const QString& path, ImagePtr img);

    // Initiates async load if not cached and not currently inflight
    void request(const QString& path);

    // Pinned paths are exempt from LRU eviction (current + immediate neighbors)
    void setPinned(const QStringList& paths);

    // Clears all unpinned and pinned images
    void clear();

signals:
    void loaded(const QString& path);
    void failed(const QString& path, const QString& error);

private:
    void onFinished(const LoadResult& result);
    void trim();

    QThreadPool m_pool;
    QHash<QString, ImagePtr> m_images;
    std::list<QString> m_lru;  // front = MRU, back = LRU
    QSet<QString> m_inflight;
    QSet<QString> m_pinned;
    std::size_t m_bytes = 0;
    std::size_t m_budget = 4096ULL * 1024ULL * 1024ULL; // Default 4 GB
};

}  // namespace sview
