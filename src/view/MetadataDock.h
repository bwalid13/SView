#pragma once

#include "../viewmodel/ViewerViewModel.h"

#include <QMap>
#include <QString>
#include <QWidget>

class QLineEdit;
class QLabel;
class QTableWidget;

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace sview {

// -----------------------------------------------------------------------------
// MetadataDock: Panneau inspecteur de métadonnées EXIF, OIIO et spécifications
// techniques (résolution, canaux, sous-images/AOVs, espace couleur, tags).
// -----------------------------------------------------------------------------
class MetadataDock : public QWidget {
    Q_OBJECT
public:
    explicit MetadataDock(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~MetadataDock() override = default;

    // Getters publics pour les tests et l'introspection
    QTableWidget* tableWidget() const { return m_table; }
    QLineEdit* searchBox() const { return m_searchEdit; }
    QLabel* countLabel() const { return m_countLabel; }

    int metadataCount() const { return m_metadata.size(); }
    const QMap<QString, QString>& metadata() const { return m_metadata; }
    QString getMetadataValue(const QString& key) const { return m_metadata.value(key); }

    // Helper de création pour QtAdvancedDockingSystem
    static ads::CDockWidget* createDockWidget(ads::CDockManager* dockManager,
                                              ViewerViewModel* vm,
                                              QWidget* parent = nullptr);

public slots:
    void refreshMetadata();
    void setMetadata(const QMap<QString, QString>& meta);
    void clearMetadata();
    void filterMetadata(const QString& filterText);
    void copySelectedMetadata();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUi();
    void setupConnections();
    void populateTable();

    ViewerViewModel* m_vm = nullptr;
    QMap<QString, QString> m_metadata;

    QLineEdit* m_searchEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_countLabel = nullptr;
};

}  // namespace sview
