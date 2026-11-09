#pragma once

#include "../viewmodel/ViewerViewModel.h"

#include <QColor>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace sview {

// -----------------------------------------------------------------------------
// ColorSwatchWidget: Échantillon couleur avec damier alpha d'arrière-plan
// -----------------------------------------------------------------------------
class ColorSwatchWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorSwatchWidget(QWidget* parent = nullptr);
    ~ColorSwatchWidget() override = default;

    void setColor(const QColor& color, float alphaFloat = 1.0f);
    void clearColor();

    QColor color() const { return m_color; }
    float alpha() const { return m_alpha; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    bool m_hasColor = false;
    QColor m_color;
    float m_alpha = 1.0f;
};

// -----------------------------------------------------------------------------
// PixelProbeDock: Inspecteur de pixel sous le curseur affichant coordonnées,
// valeurs brutes 32-bit float RGBA (format standard et scientifique), swatch,
// luminance et code hexadécimal.
// -----------------------------------------------------------------------------
class PixelProbeDock : public QWidget {
    Q_OBJECT
public:
    explicit PixelProbeDock(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~PixelProbeDock() override = default;

    // Getters publics pour les tests automatisés et l'introspection
    QLabel* coordLabel() const { return m_coordLabel; }
    QLabel* normCoordLabel() const { return m_normCoordLabel; }

    QLabel* rValLabel() const { return m_rValLabel; }
    QLabel* gValLabel() const { return m_gValLabel; }
    QLabel* bValLabel() const { return m_bValLabel; }
    QLabel* aValLabel() const { return m_aValLabel; }

    QLabel* rSciLabel() const { return m_rSciLabel; }
    QLabel* gSciLabel() const { return m_gSciLabel; }
    QLabel* bSciLabel() const { return m_bSciLabel; }
    QLabel* aSciLabel() const { return m_aSciLabel; }

    ColorSwatchWidget* swatchWidget() const { return m_swatch; }
    QLabel* luminanceLabel() const { return m_luminanceLabel; }
    QLabel* hexLabel() const { return m_hexLabel; }
    QPushButton* copyButton() const { return m_copyButton; }

    const PixelProbeData& currentData() const { return m_currentData; }
    ViewerViewModel* viewModel() const { return m_vm; }

    // Helper de création pour QtAdvancedDockingSystem
    static ads::CDockWidget* createDockWidget(ads::CDockManager* dockManager,
                                              ViewerViewModel* vm,
                                              QWidget* parent = nullptr);

public slots:
    void setProbeData(const PixelProbeData& data);
    void clearProbe();
    void copyToClipboard();
    QString copyCurrentPixel();
    QString formattedRgbaFloat() const;
    void copyRgbaFloatToClipboard();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onProbeUpdated(const PixelProbeData& data);

private:
    void setupUi();
    void setupShortcuts();
    void setupConnections();

    ViewerViewModel* m_vm = nullptr;
    PixelProbeData m_currentData;

    // UI Widgets
    QLabel* m_coordLabel = nullptr;
    QLabel* m_normCoordLabel = nullptr;

    QLabel* m_rValLabel = nullptr;
    QLabel* m_gValLabel = nullptr;
    QLabel* m_bValLabel = nullptr;
    QLabel* m_aValLabel = nullptr;

    QLabel* m_rSciLabel = nullptr;
    QLabel* m_gSciLabel = nullptr;
    QLabel* m_bSciLabel = nullptr;
    QLabel* m_aSciLabel = nullptr;

    ColorSwatchWidget* m_swatch = nullptr;
    QLabel* m_luminanceLabel = nullptr;
    QLabel* m_hexLabel = nullptr;
    QPushButton* m_copyButton = nullptr;
};

}  // namespace sview
