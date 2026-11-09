#include "FolderContentDock.h"
#include "../model/FolderModel.h"
#include "../viewmodel/ViewerViewModel.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace sview {

FolderContentDock::FolderContentDock(ViewerViewModel* vm, QWidget* parent)
    : QWidget(parent)
    , m_vm(vm)
{
    setObjectName(QStringLiteral("FolderContentDock"));
    setupUi();

    // Décodage des vignettes hors du thread UI : 2 threads suffisent et
    // laissent le reste du pool global à l'image affichée.
    m_thumbPool.setMaxThreadCount(2);
}

FolderContentDock::~FolderContentDock()
{
    // Invalide les tâches en cours puis attend qu'elles se terminent : aucun
    // worker ne peut plus toucher `this` après la destruction.
    m_thumbGeneration.fetch_add(1);
    m_thumbPool.clear();
    m_thumbPool.waitForDone();
}

void FolderContentDock::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_thumbsDirty) {
        startThumbnailLoading();
    }
}

QImage FolderContentDock::decodeThumbnail(const QString& path, int targetSize)
{
    QImageReader reader(path);
    if (!reader.canRead()) {
        return QImage();
    }
    reader.setAutoTransform(true);
    // Les formats qui savent décoder à l'échelle (JPEG, WebP) évitent de
    // décoder l'image pleine résolution ; les autres sont rééchantillonnés
    // par Qt après décodage.
    reader.setScaledSize(QSize(targetSize, targetSize));
    QImage image = reader.read();
    if (image.isNull()) {
        return QImage();
    }
    if (image.width() > targetSize || image.height() > targetSize) {
        image = image.scaled(targetSize, targetSize, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    return image;
}

void FolderContentDock::startThumbnailLoading()
{
    m_thumbsDirty = false;
    if (!m_listWidget || m_pendingThumbs.isEmpty()) {
        return;
    }

    const quint64 generation = m_thumbGeneration.fetch_add(1) + 1;
    const QStringList paths = m_pendingThumbs;
    m_pendingThumbs.clear();

    for (const QString& path : paths) {
        m_thumbPool.start([this, generation, path]() {
            if (m_thumbGeneration.load(std::memory_order_relaxed) != generation) {
                return;  // dossier changé : on abandonne
            }
            const QImage thumb = decodeThumbnail(path, kThumbSize);
            if (thumb.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                this,
                [this, generation, path, thumb]() { onThumbnailReady(generation, path, thumb); },
                Qt::QueuedConnection);
        });
    }
}

void FolderContentDock::onThumbnailReady(quint64 generation, const QString& path, const QImage& image)
{
    if (generation != m_thumbGeneration.load(std::memory_order_relaxed)) {
        return;
    }
    QListWidgetItem* item = m_itemsByPath.value(path, nullptr);
    if (!item || image.isNull()) {
        return;
    }
    item->setIcon(QIcon(QPixmap::fromImage(image)));
}

void FolderContentDock::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Header: Folder name + Refresh button
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    m_folderLabel = new QLabel(tr("No folder"), this);
    m_folderLabel->setObjectName(QStringLiteral("folderContentLabel"));
    m_folderLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #b0b0b0;"));
    headerLayout->addWidget(m_folderLabel, 1);

    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("folderContentRefreshBtn"));
    m_refreshBtn->setToolTip(tr("Refresh current directory"));
    headerLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(headerLayout);

    // Search filter
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setObjectName(QStringLiteral("folderContentFilterEdit"));
    m_filterEdit->setPlaceholderText(tr("Filter folder images..."));
    m_filterEdit->setClearButtonEnabled(true);
    mainLayout->addWidget(m_filterEdit);

    // List widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setObjectName(QStringLiteral("folderContentListWidget"));
    m_listWidget->setIconSize(QSize(64, 64));
    m_listWidget->setSpacing(4);
    m_listWidget->setUniformItemSizes(true);
    mainLayout->addWidget(m_listWidget, 1);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &FolderContentDock::filterFiles);
    connect(m_refreshBtn, &QPushButton::clicked, this, &FolderContentDock::refresh);
    connect(m_listWidget, &QListWidget::itemClicked, this, &FolderContentDock::onItemActivated);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &FolderContentDock::onItemActivated);
}

int FolderContentDock::count() const
{
    return m_listWidget ? m_listWidget->count() : 0;
}

QImage FolderContentDock::generateThumbnail(const ImagePtr& img, int targetSize)
{
    if (!img || img->width <= 0 || img->height <= 0 || img->rgba.empty()) {
        return QImage();
    }

    const int srcW = img->width;
    const int srcH = img->height;

    int dstW = targetSize;
    int dstH = targetSize;
    if (srcW > srcH) {
        dstH = std::max(1, (targetSize * srcH) / srcW);
    } else {
        dstW = std::max(1, (targetSize * srcW) / srcH);
    }

    QImage thumb(dstW, dstH, QImage::Format_ARGB32_Premultiplied);
    const float stepX = float(srcW) / float(dstW);
    const float stepY = float(srcH) / float(dstH);

    auto toSrgb = [](float c) -> int {
        c = std::clamp(c, 0.0f, 1.0f);
        float s = (c <= 0.0031308f) ? (12.92f * c) : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
        return std::clamp(int(s * 255.0f + 0.5f), 0, 255);
    };

    for (int y = 0; y < dstH; ++y) {
        const int sy = std::clamp(int(y * stepY), 0, srcH - 1);
        QRgb* scanLine = reinterpret_cast<QRgb*>(thumb.scanLine(y));
        for (int x = 0; x < dstW; ++x) {
            const int sx = std::clamp(int(x * stepX), 0, srcW - 1);
            const size_t idx = (size_t(sy) * size_t(srcW) + size_t(sx)) * 4;
            if (idx + 3 < img->rgba.size()) {
                const float r = img->rgba[idx];
                const float g = img->rgba[idx + 1];
                const float b = img->rgba[idx + 2];
                const float a = std::clamp(img->rgba[idx + 3], 0.0f, 1.0f);
                scanLine[x] = qRgba(toSrgb(r), toSrgb(g), toSrgb(b), int(a * 255.0f + 0.5f));
            } else {
                scanLine[x] = qRgba(0, 0, 0, 255);
            }
        }
    }
    return thumb;
}

void FolderContentDock::setCurrentFile(const QString& filePath, const ImagePtr& image)
{
    if (filePath.isEmpty() || !m_listWidget)
        return;

    const QString clean = QDir::cleanPath(filePath);
    const QFileInfo fi(clean);
    const QString dir = fi.isDir() ? clean : fi.absolutePath();

    if (dir != m_currentDir || m_listWidget->count() == 0) {
        m_currentDir = dir;
        m_currentFile = clean;
        populateFromFolder();
    } else {
        m_currentFile = clean;
    }

    // Highlight and scroll to the current file
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == m_currentFile) {
            if (image) {
                QImage thumb = generateThumbnail(image, 64);
                if (!thumb.isNull()) {
                    item->setIcon(QIcon(QPixmap::fromImage(thumb)));
                }
            }
            m_listWidget->setCurrentItem(item);
            m_listWidget->scrollToItem(item);
            break;
        }
    }
}

void FolderContentDock::setDirectory(const QString& dirPath)
{
    if (dirPath.isEmpty())
        return;
    const QString clean = QDir::cleanPath(dirPath);
    if (clean == m_currentDir && m_listWidget->count() > 0)
        return;
    m_currentDir = clean;
    m_currentFile.clear();
    populateFromFolder();
}

void FolderContentDock::refresh()
{
    populateFromFolder();
}

void FolderContentDock::clear()
{
    m_currentDir.clear();
    m_currentFile.clear();
    m_thumbGeneration.fetch_add(1);
    m_itemsByPath.clear();
    m_pendingThumbs.clear();
    m_thumbsDirty = false;
    if (m_folderLabel) {
        m_folderLabel->setText(tr("No folder"));
        m_folderLabel->setToolTip(QString());
    }
    if (m_listWidget) {
        m_listWidget->clear();
    }
}

void FolderContentDock::populateFromFolder()
{
    if (!m_listWidget)
        return;

    m_listWidget->clear();

    // Invalide les décodages en vol : leurs résultats ne doivent pas atterrir
    // sur les items de la nouvelle liste.
    m_thumbGeneration.fetch_add(1);
    m_itemsByPath.clear();
    m_pendingThumbs.clear();
    m_thumbsDirty = false;

    if (m_currentDir.isEmpty()) {
        if (m_folderLabel) {
            m_folderLabel->setText(tr("No folder"));
            m_folderLabel->setToolTip(QString());
        }
        return;
    }

    const QDir dir(m_currentDir);
    if (m_folderLabel) {
        m_folderLabel->setText(dir.dirName().isEmpty() ? m_currentDir : dir.dirName());
        m_folderLabel->setToolTip(m_currentDir);
    }

    QStringList files;
    if (m_vm && !m_vm->folderFiles().isEmpty() &&
        QDir::cleanPath(QFileInfo(m_vm->currentFilePath()).absolutePath()) == m_currentDir) {
        files = m_vm->folderFiles();
    } else {
        QStringList nameFilters;
        for (const QString& ext : FolderModel::supportedExtensions()) {
            nameFilters << QStringLiteral("*.%1").arg(ext);
        }
        const QFileInfoList entries = dir.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const auto& fi : entries) {
            files << fi.absoluteFilePath();
        }
    }

    const QString filterText = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    const QIcon placeholder = QApplication::style()->standardIcon(QStyle::SP_FileIcon);

    QStringList pending;
    pending.reserve(files.size());

    for (const QString& file : files) {
        const QFileInfo fi(file);
        QIcon icon;
        bool hasRealIcon = false;

        // L'image courante est déjà en mémoire : vignette CPU immédiate.
        if (file == m_currentFile && m_vm && m_vm->image()) {
            const QImage thumb = generateThumbnail(m_vm->image(), kThumbSize);
            if (!thumb.isNull()) {
                icon = QIcon(QPixmap::fromImage(thumb));
                hasRealIcon = true;
            }
        }

        if (!hasRealIcon) {
            icon = placeholder;
        }

        auto* item = new QListWidgetItem(icon, fi.fileName());
        item->setToolTip(file);
        item->setData(Qt::UserRole, file);

        if (!filterText.isEmpty() && !fi.fileName().contains(filterText, Qt::CaseInsensitive)) {
            item->setHidden(true);
        }

        m_listWidget->addItem(item);
        m_itemsByPath.insert(file, item);

        // Tout le reste est décodé en tâche de fond, sans bloquer l'ouverture.
        if (!hasRealIcon && pending.size() < kMaxThumbnails) {
            pending << file;
        }

        if (file == m_currentFile) {
            m_listWidget->setCurrentItem(item);
            m_listWidget->scrollToItem(item);
        }
    }

    m_pendingThumbs = pending;
    m_thumbsDirty = true;
    if (isVisible()) {
        startThumbnailLoading();
    }
}

void FolderContentDock::filterFiles(const QString& filterText)
{
    if (!m_listWidget)
        return;

    const QString needle = filterText.trimmed();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        if (needle.isEmpty()) {
            item->setHidden(false);
        } else {
            item->setHidden(!item->text().contains(needle, Qt::CaseInsensitive));
        }
    }
}

void FolderContentDock::onItemActivated(QListWidgetItem* item)
{
    if (!item)
        return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit imageSelected(path);
    }
}

}  // namespace sview
