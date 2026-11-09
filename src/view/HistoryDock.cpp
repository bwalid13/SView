#include "HistoryDock.h"
#include "../viewmodel/ViewerViewModel.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace sview {

HistoryDock::HistoryDock(ViewerViewModel* vm, QWidget* parent)
    : QWidget(parent)
    , m_vm(vm)
{
    setObjectName(QStringLiteral("HistoryDock"));
    setupUi();
}

void HistoryDock::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top bar: Search filter + Clear button
    auto* topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(4);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setObjectName(QStringLiteral("historyFilterEdit"));
    m_filterEdit->setPlaceholderText(tr("Filter history..."));
    m_filterEdit->setClearButtonEnabled(true);
    topLayout->addWidget(m_filterEdit, 1);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_clearBtn->setObjectName(QStringLiteral("historyClearBtn"));
    m_clearBtn->setToolTip(tr("Clear all history entries"));
    topLayout->addWidget(m_clearBtn);

    mainLayout->addLayout(topLayout);

    // List widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setObjectName(QStringLiteral("historyListWidget"));
    m_listWidget->setIconSize(QSize(64, 64));
    m_listWidget->setSpacing(4);
    m_listWidget->setUniformItemSizes(true);
    mainLayout->addWidget(m_listWidget, 1);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &HistoryDock::filterHistory);
    connect(m_clearBtn, &QPushButton::clicked, this, &HistoryDock::clearHistory);
    connect(m_listWidget, &QListWidget::itemClicked, this, &HistoryDock::onItemActivated);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &HistoryDock::onItemActivated);
}

int HistoryDock::count() const
{
    return m_listWidget ? m_listWidget->count() : 0;
}

QImage HistoryDock::generateThumbnail(const ImagePtr& img, int targetSize)
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

void HistoryDock::addImage(const QString& filePath, ImagePtr image)
{
    if (filePath.isEmpty() || !m_listWidget)
        return;

    const QString clean = QDir::cleanPath(filePath);

    // Check if already in list
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* it = m_listWidget->item(i);
        if (it->data(Qt::UserRole).toString() == clean) {
            auto* taken = m_listWidget->takeItem(i);
            m_listWidget->insertItem(0, taken);
            m_listWidget->setCurrentRow(0);
            return;
        }
    }

    QIcon icon;
    if (image) {
        QImage thumb = generateThumbnail(image, 64);
        if (!thumb.isNull()) {
            icon = QIcon(QPixmap::fromImage(thumb));
        }
    }
    if (icon.isNull()) {
        QImageReader reader(clean);
        reader.setScaledSize(QSize(64, 64));
        QImage img = reader.read();
        if (!img.isNull()) {
            icon = QIcon(QPixmap::fromImage(img));
        } else {
            icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
        }
    }

    const QFileInfo fi(clean);
    auto* item = new QListWidgetItem(icon, fi.fileName());
    item->setToolTip(clean);
    item->setData(Qt::UserRole, clean);
    m_listWidget->insertItem(0, item);

    if (m_filterEdit && !m_filterEdit->text().isEmpty()) {
        filterHistory(m_filterEdit->text());
    }
}

void HistoryDock::removeImage(const QString& filePath)
{
    if (filePath.isEmpty() || !m_listWidget)
        return;

    const QString clean = QDir::cleanPath(filePath);
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* it = m_listWidget->item(i);
        if (it->data(Qt::UserRole).toString() == clean) {
            delete m_listWidget->takeItem(i);
            break;
        }
    }
}

void HistoryDock::clearHistory()
{
    if (m_listWidget) {
        m_listWidget->clear();
    }
    emit historyCleared();
}

void HistoryDock::filterHistory(const QString& filterText)
{
    if (!m_listWidget)
        return;

    const QString trimmed = filterText.trimmed();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        if (trimmed.isEmpty()) {
            item->setHidden(false);
        } else {
            const bool match = item->text().contains(trimmed, Qt::CaseInsensitive) ||
                               item->toolTip().contains(trimmed, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    }
}

void HistoryDock::onItemActivated(QListWidgetItem* item)
{
    if (!item)
        return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit imageSelected(path);
    }
}

}  // namespace sview
