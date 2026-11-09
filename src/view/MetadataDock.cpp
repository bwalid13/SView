#include "MetadataDock.h"
#include "../viewmodel/ViewerViewModel.h"

#include <DockManager.h>
#include <DockWidget.h>

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace sview {

MetadataDock::MetadataDock(ViewerViewModel* vm, QWidget* parent)
    : QWidget(parent)
    , m_vm(vm)
{
    setupUi();
    setupConnections();
    refreshMetadata();
}

void MetadataDock::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 1. Barre de recherche et filtrage
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("metadataSearchEdit"));
    m_searchEdit->setPlaceholderText(tr("Search metadata property or value..."));
    m_searchEdit->setClearButtonEnabled(true);
    mainLayout->addWidget(m_searchEdit);

    // 2. Table Clé - Valeur
    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("metadataTableWidget"));
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    mainLayout->addWidget(m_table);

    // 3. Label de statut / décompte
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("metadataCountLabel"));
    m_countLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    m_countLabel->setText(tr("No metadata loaded"));
    mainLayout->addWidget(m_countLabel);
}

void MetadataDock::setupConnections()
{
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MetadataDock::filterMetadata);

    if (m_vm) {
        connect(m_vm, &ViewerViewModel::imageChanged, this, &MetadataDock::refreshMetadata);
        connect(m_vm, &ViewerViewModel::displayChanged, this, &MetadataDock::refreshMetadata);
    }
}

ads::CDockWidget* MetadataDock::createDockWidget(ads::CDockManager* dockManager,
                                                 ViewerViewModel* vm,
                                                 QWidget* parent)
{
    auto* dockWidget = new ads::CDockWidget(dockManager, tr("Metadata"));
    dockWidget->setObjectName(QStringLiteral("MetadataDockWidget"));
    auto* content = new MetadataDock(vm, parent);
    dockWidget->setWidget(content);
    dockWidget->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);
    return dockWidget;
}

void MetadataDock::refreshMetadata()
{
    if (!m_vm) {
        clearMetadata();
        return;
    }

    const auto img = m_vm->image();
    if (!img) {
        clearMetadata();
        return;
    }

    QMap<QString, QString> meta = img->metadata;

    // Assurer la présence des métadonnées fondamentales si manquantes
    if (!meta.contains(QStringLiteral("File Name")) && !img->path.isEmpty()) {
        meta.insert(QStringLiteral("File Name"), QFileInfo(img->path).fileName());
    }
    if (!meta.contains(QStringLiteral("File Path")) && !img->path.isEmpty()) {
        meta.insert(QStringLiteral("File Path"), img->path);
    }
    if (!meta.contains(QStringLiteral("Dimensions")) && img->width > 0) {
        meta.insert(QStringLiteral("Dimensions"), QStringLiteral("%1 x %2").arg(img->width).arg(img->height));
    }
    if (!meta.contains(QStringLiteral("Channels")) && img->sourceChannels > 0) {
        meta.insert(QStringLiteral("Channels"),
                    QStringLiteral("%1 (%2)").arg(img->sourceChannels).arg(img->channelNames.join(QStringLiteral(", "))));
    }
    if (!meta.contains(QStringLiteral("File Format")) && !img->formatName.isEmpty()) {
        meta.insert(QStringLiteral("File Format"), img->formatName);
    }
    if (!meta.contains(QStringLiteral("Color Space"))) {
        meta.insert(QStringLiteral("Color Space"),
                    (img->space == SourceSpace::SceneLinear) ? QStringLiteral("Scene-Linear") : QStringLiteral("sRGB"));
    }
    if (!meta.contains(QStringLiteral("Active AOV")) && !img->activeAov.isEmpty()) {
        meta.insert(QStringLiteral("Active AOV"), img->activeAov);
    }

    setMetadata(meta);
}

void MetadataDock::setMetadata(const QMap<QString, QString>& meta)
{
    m_metadata = meta;
    populateTable();
}

void MetadataDock::clearMetadata()
{
    m_metadata.clear();
    m_table->setRowCount(0);
    m_countLabel->setText(tr("No metadata loaded"));
}

void MetadataDock::populateTable()
{
    m_table->setRowCount(0);
    m_table->setRowCount(m_metadata.size());

    int row = 0;
    for (auto it = m_metadata.constBegin(); it != m_metadata.constEnd(); ++it, ++row) {
        auto* keyItem = new QTableWidgetItem(it.key());
        keyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        keyItem->setToolTip(it.key());

        auto* valItem = new QTableWidgetItem(it.value());
        valItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        valItem->setToolTip(it.value());

        m_table->setItem(row, 0, keyItem);
        m_table->setItem(row, 1, valItem);
    }

    filterMetadata(m_searchEdit->text());
}

void MetadataDock::filterMetadata(const QString& filterText)
{
    const QString query = filterText.trimmed();
    int visibleCount = 0;

    for (int r = 0; r < m_table->rowCount(); ++r) {
        const auto* keyItem = m_table->item(r, 0);
        const auto* valItem = m_table->item(r, 1);
        const QString keyStr = keyItem ? keyItem->text() : QString();
        const QString valStr = valItem ? valItem->text() : QString();

        const bool match = query.isEmpty() ||
                           keyStr.contains(query, Qt::CaseInsensitive) ||
                           valStr.contains(query, Qt::CaseInsensitive);

        m_table->setRowHidden(r, !match);
        if (match) {
            ++visibleCount;
        }
    }

    if (m_metadata.isEmpty()) {
        m_countLabel->setText(tr("No metadata loaded"));
    } else if (query.isEmpty()) {
        m_countLabel->setText(tr("%1 attributes").arg(m_metadata.size()));
    } else {
        m_countLabel->setText(tr("Showing %1 of %2 attributes").arg(visibleCount).arg(m_metadata.size()));
    }
}

void MetadataDock::copySelectedMetadata()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount())
        return;

    const auto* keyItem = m_table->item(row, 0);
    const auto* valItem = m_table->item(row, 1);
    if (!keyItem || !valItem)
        return;

    const QString text = QStringLiteral("%1: %2").arg(keyItem->text(), valItem->text());
    QGuiApplication::clipboard()->setText(text);
}

void MetadataDock::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy)) {
        copySelectedMetadata();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace sview
