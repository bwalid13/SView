#pragma once

#include "../model/ColorManager.h"
#include "../model/FolderModel.h"
#include "../model/ImageCache.h"
#include "../model/ImageData.h"

#include <QList>
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

namespace sview {

enum class ChannelMode { RGB = 0, Red = 1, Green = 2, Blue = 3, Alpha = 4, Luma = 5 };

// Données complètes de sonde pixel sous le curseur
struct PixelProbeData {
    bool hasPixel = false;
    int x = 0;              // Coordonnée entière X dans l'espace image (0..width-1)
    int y = 0;              // Coordonnée entière Y dans l'espace image (0..height-1)
    float u = 0.0f;         // Coordonnée normalisée U (0.0..1.0)
    float v = 0.0f;         // Coordonnée normalisée V (0.0..1.0)
    float r = 0.0f;         // Valeur brute float32 Red (non écrêtée)
    float g = 0.0f;         // Valeur brute float32 Green (non écrêtée)
    float b = 0.0f;         // Valeur brute float32 Blue (non écrêtée)
    float a = 1.0f;         // Valeur brute float32 Alpha
    float luminance = 0.0f; // ITU-R BT.709: 0.2126*R + 0.7152*G + 0.0722*B
    QString hexCode;        // Code couleur hexadécimal 8 bits ("#RRGGBB")
};

// ViewModel : tout l'état et toutes les commandes du viewer.
// Ne connaît aucun widget ; la vue se contente de s'abonner aux signaux.
class ViewerViewModel : public QObject {
    Q_OBJECT
public:
    explicit ViewerViewModel(QObject* parent = nullptr);

    // --- État lisible par la vue ---
    ImagePtr image() const { return m_image; }
    double zoom() const { return m_zoom; }                 // pixels image -> pixels logiques
    QPointF pan() const { return m_pan; }                  // décalage du centre, en pixels logiques
    double exposureStops() const { return m_exposure; }
    float exposureMultiplier() const;
    double gamma() const { return m_gamma; }
    ChannelMode channel() const { return m_channel; }
    bool checkerboardEnabled() const { return m_checkerboard; }

    // --- A/B Wipe & Image B ---
    ImagePtr imageB() const { return m_imageB; }
    bool wipeEnabled() const { return m_wipeEnabled; }
    int wipeMode() const { return m_wipeMode; }            // 0 = Vertical, 1 = Horizontal, 2 = Onion Skin
    float wipePos() const { return m_wipePos; }            // 0.0 à 1.0

    // --- AOV / Couches ---
    QString currentAov() const { return m_currentAov; }
    QStringList availableAovs() const;
    QList<AovLayer> aovLayers() const;

    // Processeur OCIO pour l'image courante (mis en cache, nul si OCIO indisponible)
    OCIO::ConstGPUProcessorRcPtr gpuProcessor();
    QString gpuProcessorKey() const;  // change quand le shader doit être reconstruit
    QStringList availableColorSpaces() const { return m_colors.colorSpaces(); }
    QString currentColorSpace() const { return m_colors.currentColorSpace(); }

    bool isLoading() const { return m_loading; }
    bool overlayVisible() const { return m_infoVisible || !m_error.isEmpty(); }
    QString error() const { return m_error; }
    QString overlayText() const;
    QString windowTitle() const;
    QString probeValues() const { return m_probeValues; }
    QString probeText() const { return m_probeText; }
    const PixelProbeData& currentProbeData() const { return m_probeData; }

    ImageCache& cache() { return m_cache; }
    const ImageCache& cache() const { return m_cache; }

    // --- Commandes ---
    void openPath(const QString& path);
    void closeImage();
    void showFrame(const QString& path);
    void next() { step(+1); }
    void prev() { step(-1); }
    void first();
    void last();

    int folderCount() const { return m_folder.count(); }
    int folderIndex() const { return m_folder.index(); }
    QString currentFilePath() const { return m_folder.currentPath(); }
    const QStringList& folderFiles() const { return m_folder.files(); }
    bool hasMultipleFolderImages() const { return m_folder.count() > 1; }

    void setViewportSize(const QSizeF& size);
    void fitToWindow();
    void resetView();
    void actualSize();
    void toggleFitActual();
    void zoomBy(double factor, const QPointF& anchor);
    void setZoom(double z);
    void setImage(const ImagePtr& img);
    void panBy(const QPointF& delta);

    // Exposition & Gamma
    void setExposure(double stops);
    void adjustExposure(double stops);
    void resetExposure();

    void setGamma(double gamma);
    void resetGamma();

    // Canaux & AOVs
    void setChannel(ChannelMode mode);
    void setAov(const QString& aovName);
    void cycleView(int direction);
    void setColorSpace(const QString& name);
    void toggleInfo();

    // A/B Wipe
    void setWipeEnabled(bool enabled);
    void setWipeMode(int mode);
    void setWipePos(float pos);

    // Image B
    void openImageB(const QString& path);
    void clearImageB();
    void setImageB(const ImagePtr& img);

    // Checkerboard
    void setCheckerboardEnabled(bool enabled);
    void toggleCheckerboard();

    // Flip & Rotation (Pivot)
    bool flipH() const { return m_flipH; }
    void setFlipH(bool flip);
    bool flipV() const { return m_flipV; }
    void setFlipV(bool flip);
    int rotation() const { return m_rotation; }
    void setRotation(int degrees);
    void pivot();
    void resetTransforms();

    // --- Mode 360° & Caméra Panoramique ---
    bool is360Mode() const { return m_mode360; }
    void set360Mode(bool enabled);
    void toggle360Mode();

    float yaw() const { return m_yaw; }       // radians [-pi, pi]
    void setYaw(float yaw);
    float pitch() const { return m_pitch; }   // radians [-pi/2+eps, pi/2-eps]
    void setPitch(float pitch);
    float fov() const { return m_fov; }       // radians [15 deg, 120 deg]
    void setFov(float fov);
    void rotate360(float deltaYaw, float deltaPitch);
    void resetCamera360();

    // Sphères Lookdev (Chrome & Gris 18%)
    bool showPreviewSpheres() const { return m_showSpheres; }
    void setShowPreviewSpheres(bool show);
    void togglePreviewSpheres();

    // Rig lookdev : coin d'ancrage et color checker (taille fixe)
    int rigCorner() const { return m_rigCorner; }        // 0=bas-gauche 1=bas-droite 2=haut-gauche 3=haut-droite
    void setRigCorner(int corner);
    bool showColorChart() const { return m_showChart; }
    void setShowColorChart(bool show);

    // Sonde pixel
    void probe(const QPointF& viewportPos);

signals:
    void imageChanged();       // nouvelle image principale (ou aucune)
    void imageBChanged();      // nouvelle image B ou fermeture
    void transformChanged();   // zoom / pan
    void displayChanged();     // exposition, gamma, canal, vue OCIO, wipe, damier, flip
    void overlayChanged();     // texte d'info / titre
    void wipeChanged();        // paramètres wipe modifiés
    void aovChanged(const QString& aovName); // AOV sélectionné modifié
    void aovsChanged();        // ensemble des AOVs modifié
    void gammaChanged(double gamma);
    void checkerboardChanged(bool enabled);
    void probeUpdated(const PixelProbeData& data);
    void mode360Changed(bool enabled);
    void camera360Changed(float yaw, float pitch, float fov);
    void previewSpheresChanged(bool enabled);
    void rigChanged();        // taille / coin / espacement / sheen / chart du rig lookdev
    void rotationChanged(int degrees);
    void loadingChanged(bool loading);

private:
    void step(int delta);
    void showCurrent();
    void prefetchNeighbors();
    QStringList neighborhood() const;
    void applyImage(const ImagePtr& img);
    void fitInternal();
    void onLoaded(const QString& path);
    void onFailed(const QString& path, const QString& error);

    ImageCache m_cache;
    FolderModel m_folder;
    ColorManager m_colors;

    ImagePtr m_image;
    ImagePtr m_imageB;
    QString m_target;
    QString m_targetB;
    bool m_loading = false;
    QString m_error;

    QSizeF m_viewport{1, 1};
    double m_zoom = 1.0;
    QPointF m_pan{0, 0};
    bool m_fit = true;

    double m_exposure = 0.0;
    double m_gamma = 1.0;
    ChannelMode m_channel = ChannelMode::RGB;
    bool m_infoVisible = true;

    // A/B Wipe
    bool m_wipeEnabled = false;
    int m_wipeMode = 0;        // 0 = Vertical, 1 = Horizontal
    float m_wipePos = 0.5f;

    // AOV
    QString m_currentAov = QStringLiteral("RGBA");

    // Damier Alpha
    bool m_checkerboard = true;

    // Sonde pixel
    PixelProbeData m_probeData;
    QString m_probeText;
    QString m_probeValues;

    OCIO::ConstGPUProcessorRcPtr m_gpu;
    QString m_gpuKey;

    // 360° & Lookdev
    bool m_mode360 = false;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_fov = 0.610865238f; // 35 deg en radians (défaut du mode 360°)
    bool m_showSpheres = false;
    int m_rigCorner = 0;          // 0=bas-gauche
    bool m_showChart = true;      // color checker Macbeth

    // Flip & Rotation
    bool m_flipH = false;
    bool m_flipV = false;
    int m_rotation = 0;
};

}  // namespace sview
