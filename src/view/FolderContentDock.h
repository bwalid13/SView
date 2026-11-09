#pragma once

#include "../model/ImageData.h"

#include <QHash>
#include <QImage>
#include <QThreadPool>
#include <QWidget>

#include <atomic>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QShowEvent;

namespace sview {

class ViewerViewModel;

// Liste des images du dossier courant.
//
// Les vignettes sont décodées **hors du thread UI** (QThreadPool dédié, 2
// threads) et seulement quand le panneau est visible. Un compteur de génération
// invalide les tâches obsolètes dès que le dossier change, ce qui évite à la
// fois le gel à l'ouverture et le travail inutile.
class FolderContentDock : public QWidget {
    Q_OBJECT
public:
    explicit FolderContentDock(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~FolderContentDock() override;

    QListWidget* listWidget() const { return m_listWidget; }
    QLineEdit* searchEdit() const { return m_filterEdit; }
    QPushButton* refreshButton() const { return m_refreshBtn; }
    int count() const;
    QString currentDirectory() const { return m_currentDir; }

    // Conversion CPU d'une image déjà chargée (utilisée pour la vignette de
    // l'image courante, pas pour le décodage disque).
    static QImage generateThumbnail(const ImagePtr& img, int targetSize = 64);

public slots:
    void setCurrentFile(const QString& filePath, const ImagePtr& image = nullptr);
    void setDirectory(const QString& dirPath);
    void refresh();
    void clear();
    void filterFiles(const QString& filterText);

signals:
    void imageSelected(const QString& filePath);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onItemActivated(QListWidgetItem* item);

private:
    void setupUi();
    void populateFromFolder();
    void startThumbnailLoading();
    void onThumbnailReady(quint64 generation, const QString& path, const QImage& image);
    static QImage decodeThumbnail(const QString& path, int targetSize);

    ViewerViewModel* m_vm = nullptr;
    QString m_currentDir;
    QString m_currentFile;

    QLabel* m_folderLabel = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QListWidget* m_listWidget = nullptr;

    // Décodage asynchrone des vignettes
    QThreadPool m_thumbPool;
    QHash<QString, QListWidgetItem*> m_itemsByPath;
    QStringList m_pendingThumbs;
    std::atomic<quint64> m_thumbGeneration{0};
    bool m_thumbsDirty = false;

    static constexpr int kThumbSize = 64;
    // Garde-fou : au-delà, on n'essaie pas de vignetter un dossier de 10 000
    // images (les autres gardent l'icône générique).
    static constexpr int kMaxThumbnails = 500;
};

}  // namespace sview
