#pragma once

#include "../model/ImageData.h"

#include <QImage>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace sview {

class ViewerViewModel;

class HistoryDock : public QWidget {
    Q_OBJECT
public:
    explicit HistoryDock(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~HistoryDock() override = default;

    QListWidget* listWidget() const { return m_listWidget; }
    QLineEdit* searchEdit() const { return m_filterEdit; }
    QPushButton* clearButton() const { return m_clearBtn; }
    int count() const;

    static QImage generateThumbnail(const ImagePtr& img, int targetSize = 64);

public slots:
    void addImage(const QString& filePath, ImagePtr image = nullptr);
    void removeImage(const QString& filePath);
    void clearHistory();
    void filterHistory(const QString& filterText);

signals:
    void imageSelected(const QString& filePath);
    void historyCleared();

private slots:
    void onItemActivated(QListWidgetItem* item);

private:
    void setupUi();

    ViewerViewModel* m_vm = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QListWidget* m_listWidget = nullptr;
};

}  // namespace sview
