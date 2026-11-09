#pragma once

#include <QTimer>
#include <QWidget>

namespace sview {

class LoadingSpinnerWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoadingSpinnerWidget(QWidget* parent = nullptr);
    ~LoadingSpinnerWidget() override = default;

    bool isSpinning() const { return m_animTimer.isActive(); }

public slots:
    void setLoading(bool loading);
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimTick();
    void onDebounceTimeout();

private:
    QTimer m_animTimer;
    QTimer m_debounceTimer;
    int m_angle = 0;
    bool m_requestedLoading = false;
};

}  // namespace sview
