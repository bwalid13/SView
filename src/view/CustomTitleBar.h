#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace sview {

class CustomTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit CustomTitleBar(QWidget* parent = nullptr);
    ~CustomTitleBar() override = default;

    void setTitle(const QString& title);
    QString title() const;

    QPushButton* minimizeButton() const { return m_minBtn; }
    QPushButton* maximizeButton() const { return m_maxBtn; }
    QPushButton* closeButton() const { return m_closeBtn; }
    QLabel* titleLabel() const { return m_titleLabel; }

signals:
    void closeRequested();
    void minimizeRequested();
    void maximizeRequested();

public slots:
    void updateThemeStyle();
    void updateMaximizeButtonState();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void setupUi();

    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
};

}  // namespace sview
