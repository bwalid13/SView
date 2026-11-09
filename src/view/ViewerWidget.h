#pragma once

#include "../model/ColorManager.h"   // alias OCIO::
#include "../model/ImageData.h"

#include <QByteArray>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPoint>

#include <vector>

class QLabel;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

namespace sview {

class FloatingControlDock;
class LoadingSpinnerWidget;
class PlaybackController;
class ViewerViewModel;

// Vue : affiche l'image en texture RGBA16F, applique exposition / gamma / A/B wipe / damier / OCIO
// dans un fragment shader.
class ViewerWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    struct ViewportTransform {
        bool flipH = false;
        bool flipV = false;
        int rotation = 0;

        bool operator==(const ViewportTransform& o) const {
            return flipH == o.flipH && flipV == o.flipV && rotation == o.rotation;
        }
        bool operator!=(const ViewportTransform& o) const {
            return !(*this == o);
        }
    };

    explicit ViewerWidget(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~ViewerWidget() override;

    FloatingControlDock* floatingControlDock() const { return m_floatingDock; }
    void setFloatingControlDock(FloatingControlDock* dock);
    void setFloatingDockEnabled(bool enabled);

    ViewerViewModel* viewModel() const;
    double zoomFactor() const;
    void setZoom(double z);

    // Feature 3: Minimalist Viewport OSD ('I')
    void setOsdVisible(bool visible);
    bool isOsdVisible() const { return m_osdVisible; }
    void toggleOsd();
    QString osdText() const;
    void setPlaybackController(PlaybackController* controller);
    PlaybackController* playbackController() const { return m_playback; }
    QLabel* osdLabel() const { return m_osdLabel; }
    LoadingSpinnerWidget* loadingSpinner() const { return m_spinner; }

    bool canUndoTransform() const { return !m_undoStack.empty(); }
    bool canRedoTransform() const { return !m_redoStack.empty(); }
    QImage captureViewport();

public slots:
    void fitToScreen();
    bool undoTransform();
    bool redoTransform();
    void resetTransformHistory();

signals:
    void fileDropped(const QString& path);
    void filesDropped(const QStringList& paths);
    void copyPixelRequested();
    void fullscreenToggleRequested();
    void transformHistoryChanged(bool canUndo, bool canRedo);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void resizeEvent(QResizeEvent* e) override;

    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;
    bool focusNextPrevChild(bool next) override { Q_UNUSED(next); return false; }

private slots:
    void onImageChanged();
    void onImageBChanged();
    void updateOsd();
    void onTransformStateMaybeChanged();

private:
    struct Lut {
        GLuint tex = 0;
        GLenum target = GL_TEXTURE_2D;
        int unit = 0;
        QByteArray sampler;
    };

    void uploadImage();
    void uploadImageB();
    // Prépare les pixels pour l'upload : réduit l'image (box filter) si elle
    // dépasse GL_MAX_TEXTURE_SIZE, sinon l'upload échoue silencieusement.
    const float* textureSource(const ImagePtr& img, std::vector<float>& scratch,
                               int& width, int& height, float& scale);
    void rebuildProgram();
    bool buildProgram(const QString& ocioFunctionSource);
    void uploadLuts(const OCIO::GpuShaderDescRcPtr& desc);
    void destroyLuts();

    ViewerViewModel* m_vm;

    bool m_glReady = false;
    bool m_hasTexture = false;
    bool m_hasTextureB = false;
    ImagePtr m_uploaded;
    ImagePtr m_uploadedB;
    float m_textureScale = 1.0f;   // < 1 quand l'image a été réduite pour le GPU

    GLuint m_vao = 0;
    GLuint m_tex = 0;
    GLuint m_texB = 0;
    QOpenGLShaderProgram m_prog;
    std::vector<Lut> m_luts;
    QString m_key;

    bool m_dragging = false;
    bool m_draggingWipe = false;
    QPoint m_lastPos;

    FloatingControlDock* m_floatingDock = nullptr;
    PlaybackController* m_playback = nullptr;

    bool m_osdVisible = true;
    QLabel* m_osdLabel = nullptr;
    LoadingSpinnerWidget* m_spinner = nullptr;

    std::vector<ViewportTransform> m_undoStack;
    std::vector<ViewportTransform> m_redoStack;
    ViewportTransform m_currentTransform;
    bool m_isApplyingUndoRedo = false;
};

}  // namespace sview
