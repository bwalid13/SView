#pragma once

#include <QMainWindow>
#include <QPointer>

class QAction;
class QActionGroup;
class QCloseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QLabel;
class QMenu;

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace sview {

class AnalysisDock;
class CustomTitleBar;
class FloatingControlDock;
class FolderContentDock;
class MetadataDock;
class PixelProbeDock;
class PlaybackController;
class SequenceModel;
class TimelineDock;
class ViewerViewModel;
class ViewerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ViewerViewModel* vm, QWidget* parent = nullptr);
    ~MainWindow() override;

    ads::CDockManager* dockManager() const { return m_dockManager; }
    ViewerWidget* viewerWidget() const { return m_viewer; }
    TimelineDock* timelineDock() const { return m_timelineWidget; }
    AnalysisDock* analysisDock() const { return m_analysisWidget; }
    PixelProbeDock* pixelProbeDock() const { return m_probeWidget; }
    MetadataDock* metadataDock() const { return m_metadataWidget; }
    ads::CDockWidget* metadataDockWidget() const { return m_metadataDock; }
    FolderContentDock* folderContentDock() const { return m_folderContentWidget; }
    ads::CDockWidget* folderContentDockWidget() const { return m_folderContentDock; }
    CustomTitleBar* customTitleBar() const { return m_titleBar; }
    PlaybackController* playbackController() const { return m_playback; }
    SequenceModel* sequenceModel() const { return m_sequence; }
    FloatingControlDock* floatingControlDock() const { return m_floatingDock; }

    QAction* actionToggle360() const { return m_actionToggle360; }
    QAction* actionToggleSpheres() const { return m_actionToggleSpheres; }
    QAction* actionResetCamera360() const { return m_actionResetCamera360; }
    QAction* actionToggleFloatingDock() const { return m_actionToggleFloatingDock; }
    QAction* actionSettings() const { return m_actionSettings; }
    QAction* actionFit() const { return m_actionFit; }
    QAction* actionActualSize() const { return m_actionActualSize; }
    QAction* actionToggleMenuBar() const { return m_actionToggleMenuBar; }
    QAction* actionFlipH() const { return m_actionFlipH; }
    QAction* actionFlipV() const { return m_actionFlipV; }
    QAction* actionPivot() const { return m_actionPivot; }
    QAction* actionUndoTransform() const { return m_actionUndoTransform; }
    QAction* actionRedoTransform() const { return m_actionRedoTransform; }
    QAction* actionQuickExport() const { return m_actionQuickExport; }
    QMenu* recentFilesMenu() const { return m_recentMenu; }
    QMenuBar* menuBar() const { return m_menuBar; }

    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

public slots:
    void openFileOrSequence(const QString& path, bool autoPlay = true);
    void handleDroppedFiles(const QStringList& paths);
    void openDialog();
    void openFolderDialog();
    void saveAsDialog();
    void quickExportViewport();
    void showBatchConverter();
    void closeImage();
    void saveLayout();
    void restoreLayout();
    void resetLayout();
    void toggleFullscreen();
    void toggleMenuBar();
    void setMenuBarVisible(bool visible);
    void applyTheme(const QString& themeName);
    void updateRecentFilesMenu();
    void showSettingsDialog();
    void copyCurrentPixel();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void setupAdsDockManager();
    void setupDocks();
    void setupMenus();
    void setupShortcuts();
    void setupConnections();
    void refreshOverlay();
    void updateWindowTitle();

    // Model and ViewModel references
    ViewerViewModel* m_vm = nullptr;
    SequenceModel* m_sequence = nullptr;
    PlaybackController* m_playback = nullptr;

    // ADS Docking System
    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockWidget* m_viewerDock = nullptr;
    ads::CDockWidget* m_timelineDock = nullptr;
    ads::CDockWidget* m_analysisDock = nullptr;
    ads::CDockWidget* m_probeDock = nullptr;
    ads::CDockWidget* m_metadataDock = nullptr;
    ads::CDockWidget* m_folderContentDock = nullptr;

    // Content Widgets
    ViewerWidget* m_viewer = nullptr;
    FloatingControlDock* m_floatingDock = nullptr;
    TimelineDock* m_timelineWidget = nullptr;
    AnalysisDock* m_analysisWidget = nullptr;
    PixelProbeDock* m_probeWidget = nullptr;
    MetadataDock* m_metadataWidget = nullptr;
    FolderContentDock* m_folderContentWidget = nullptr;
    CustomTitleBar* m_titleBar = nullptr;
    QWidget* m_topContainer = nullptr;
    QMenuBar* m_menuBar = nullptr;

    // Menus
    QMenu* m_fileMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_channelsMenu = nullptr;
    QMenu* m_layoutMenu = nullptr;
    QMenu* m_panelsMenu = nullptr;
    QMenu* m_themeMenu = nullptr;
    QMenu* m_helpMenu = nullptr;

    // Action Groups
    QActionGroup* m_channelGroup = nullptr;
    QActionGroup* m_themeGroup = nullptr;

    // Actions
    QAction* m_actionOpen = nullptr;
    QAction* m_actionOpenFolder = nullptr;
    QAction* m_actionSaveAs = nullptr;
    QAction* m_actionQuickExport = nullptr;
    QAction* m_actionBatchConvert = nullptr;
    QAction* m_actionClose = nullptr;
    QAction* m_actionExit = nullptr;

    QAction* m_actionUndoTransform = nullptr;
    QAction* m_actionRedoTransform = nullptr;

    QAction* m_actionFit = nullptr;
    QAction* m_actionActualSize = nullptr;
    QAction* m_actionFullscreen = nullptr;
    QAction* m_actionToggleOverlay = nullptr;
    QAction* m_actionResetExposure = nullptr;

    QAction* m_actionStepForward = nullptr;
    QAction* m_actionStepBackward = nullptr;

    QAction* m_actionSaveLayout = nullptr;
    QAction* m_actionRestoreLayout = nullptr;
    QAction* m_actionResetLayout = nullptr;

    QAction* m_actionToggle360 = nullptr;
    QAction* m_actionToggleSpheres = nullptr;
    QAction* m_actionResetCamera360 = nullptr;
    QAction* m_actionToggleFloatingDock = nullptr;
    QAction* m_actionToggleMenuBar = nullptr;
    QAction* m_actionFlipH = nullptr;
    QAction* m_actionFlipV = nullptr;
    QAction* m_actionPivot = nullptr;
    QAction* m_actionSettings = nullptr;
    QMenu* m_recentMenu = nullptr;

    // Fullscreen and Layout state preservation
    bool m_isTrueFullscreen = false;
    bool m_preFullscreenWasMaximized = false;
    bool m_preFullscreenMenuBarVisible = false;
    QByteArray m_preFullscreenGeometry;
    QRect m_preFullscreenNormalGeometry;
    QByteArray m_preFullscreenDockState;
};

}  // namespace sview
