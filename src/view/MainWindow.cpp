#include "MainWindow.h"
#include "BatchConverterDialog.h"
#include "CustomTitleBar.h"
#include "FolderContentDock.h"
#include "SettingsDialog.h"

#include "../model/FolderModel.h"
#include "../model/ImageLoader.h"
#include "../model/PlaybackController.h"
#include "../model/SequenceModel.h"
#include "../model/SettingsModel.h"
#include "../viewmodel/ViewerViewModel.h"
#include "AnalysisDock.h"
#include "FloatingControlDock.h"
#include "MetadataDock.h"
#include "PixelProbeDock.h"
#include "ThemeManager.h"
#include "TimelineDock.h"
#include "ViewerWidget.h"

#include <DockAreaTitleBar.h>
#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

#include <QAbstractSpinBox>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QLineEdit>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#endif

namespace sview {

MainWindow::MainWindow(ViewerViewModel* vm, QWidget* parent)
    : QMainWindow(parent)
    , m_vm(vm)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    setAcceptDrops(true);
    setWindowTitle(QStringLiteral("SView"));
    setWindowIcon(QApplication::windowIcon());

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_CAPTION);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif

    // 1. Instantiate Sequence and Playback engines
    m_sequence = new SequenceModel(this);
    m_playback = new PlaybackController(this);
    m_playback->setSequence(m_sequence);
    m_playback->setImageCache(&m_vm->cache());
    m_playback->setFps(SettingsModel::instance().defaultFps());

    // 2. Initialize ADS Dock Manager & Dock Widgets
    setupAdsDockManager();
    setupDocks();

    // 3. Menus, Custom Title Bar & Shortcuts
    m_topContainer = new QWidget(this);
    m_topContainer->setObjectName(QStringLiteral("topContainer"));
    auto* topLayout = new QVBoxLayout(m_topContainer);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    m_titleBar = new CustomTitleBar(this);
    m_menuBar = new QMenuBar(m_topContainer);
    topLayout->addWidget(m_titleBar);
    topLayout->addWidget(m_menuBar);
    setMenuWidget(m_topContainer);
    m_topContainer->show();
    m_titleBar->show();
    m_menuBar->setVisible(false);

    setupMenus();
    setupShortcuts();

    setupConnections();

    // 4. Restore Saved State (Geometry & Dock Layout)
    restoreLayout();
    if (SettingsModel::instance().dockLayoutState().isEmpty()) {
        m_dockManager->openPerspective(QStringLiteral("Default"));
    }

    // 6. Apply Persistent UI Theme
    const QString savedTheme = SettingsModel::instance().currentTheme();
    applyTheme(savedTheme.isEmpty() ? QStringLiteral("Classic Blue") : savedTheme);

    refreshOverlay();
    updateWindowTitle();
    m_viewer->setFocus();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupAdsDockManager()
{
    // Global ADS Configuration Flags (must be set prior to CDockManager instantiation)
    ads::CDockManager::setConfigFlag(ads::CDockManager::ActiveTabHasCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaCloseButtonClosesTab, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FloatingContainerHasWidgetTitle, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DoubleClickUndocksWidget, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DragPreviewShowsContentPixmap, true);

    m_dockManager = new ads::CDockManager(this);
    m_dockManager->setStyleSheet(""); // Disable internal stylesheet to allow ThemeManager styling
    setCentralWidget(m_dockManager);
}

void MainWindow::setupDocks()
{
    // A. Viewport Dock Widget
    m_viewer = new ViewerWidget(m_vm, this);
    m_viewer->setPlaybackController(m_playback);
    m_floatingDock = new FloatingControlDock(m_viewer, m_playback, m_vm);
    m_viewer->setFloatingControlDock(m_floatingDock);

    m_viewerDock = new ads::CDockWidget(m_dockManager, tr("Viewport"));
    m_viewerDock->setObjectName(QStringLiteral("ViewportDockWidget"));
    m_viewerDock->setWidget(m_viewer);
    m_viewerDock->setFeatures(ads::CDockWidget::DockWidgetFocusable);
    m_viewerDock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);

    // Add to Center Dock Area (tear-off capable)
    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, m_viewerDock);

    // B. Timeline Dock Widget - Neutralized / removed per user directive
    m_timelineWidget = nullptr;
    m_timelineDock = nullptr;

    // C. Control / Analysis Dock Widget (Milestone 3)
    m_analysisWidget = new AnalysisDock(m_vm, this);
    m_analysisDock = new ads::CDockWidget(m_dockManager, tr("Image Controls"));
    m_analysisDock->setObjectName(QStringLiteral("AnalysisDockWidget"));
    m_analysisDock->setWidget(m_analysisWidget);
    m_analysisDock->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);

    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_analysisDock);
    if (auto* area = m_analysisDock->dockAreaWidget()) {
        m_dockManager->setSplitterSizes(area, {950, 320});
    }

    // D. Pixel Probe Dock Widget (Milestone 3)
    m_probeWidget = new PixelProbeDock(m_vm, this);
    m_probeDock = new ads::CDockWidget(m_dockManager, tr("Pixel Probe"));
    m_probeDock->setObjectName(QStringLiteral("PixelProbeDockWidget"));
    m_probeDock->setWidget(m_probeWidget);
    m_probeDock->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);

    // Tabify Pixel Probe with Image Controls on the right dock area
    m_dockManager->addDockWidgetTabToArea(m_probeDock, m_analysisDock->dockAreaWidget());

    // E. Metadata Dock Widget (Milestone 4)
    m_metadataWidget = new MetadataDock(m_vm, this);
    m_metadataDock = new ads::CDockWidget(m_dockManager, tr("Metadata"));
    m_metadataDock->setObjectName(QStringLiteral("MetadataDockWidget"));
    m_metadataDock->setWidget(m_metadataWidget);
    m_metadataDock->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);

    // Tabify Metadata with Image Controls & Pixel Probe on the right dock area
    m_dockManager->addDockWidgetTabToArea(m_metadataDock, m_analysisDock->dockAreaWidget());

    // F. Folder Content Dock Widget
    m_folderContentWidget = new FolderContentDock(m_vm, this);
    m_folderContentDock = new ads::CDockWidget(m_dockManager, tr("Folder Content"));
    m_folderContentDock->setObjectName(QStringLiteral("FolderContentDockWidget"));
    m_folderContentDock->setWidget(m_folderContentWidget);
    m_folderContentDock->setFeatures(ads::CDockWidget::DefaultDockWidgetFeatures);
    m_dockManager->addDockWidget(ads::LeftDockWidgetArea, m_folderContentDock);
    if (auto* area = m_folderContentDock->dockAreaWidget()) {
        m_dockManager->setSplitterSizes(area, {260, 1010});
    }

    connect(m_folderContentWidget, &FolderContentDock::imageSelected, this, [this](const QString& path) {
        openFileOrSequence(path, true);
    });

    // Register this complete state as "Expert" perspective
    m_dockManager->addPerspective(QStringLiteral("Expert"));

    // Hide docks for an empty UI at launch
    if (m_timelineDock) m_timelineDock->toggleView(false);
    m_analysisDock->toggleView(false);
    m_probeDock->toggleView(false);
    m_metadataDock->toggleView(false);
    m_folderContentDock->toggleView(false);

    // Register this empty state as "Default" perspective
    m_dockManager->addPerspective(QStringLiteral("Default"));
}

void MainWindow::setupMenus()
{
    // ==========================================
    // 1. File Menu
    // ==========================================
    m_fileMenu = menuBar()->addMenu(tr("&File"));

    m_actionOpen = m_fileMenu->addAction(tr("&Open Image..."));
    m_actionOpen->setShortcut(QKeySequence::Open);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::openDialog);

    m_actionOpenFolder = m_fileMenu->addAction(tr("Open &Folder..."));
    m_actionOpenFolder->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_actionOpenFolder, &QAction::triggered, this, &MainWindow::openFolderDialog);

    // Feature 6: Recent Files Submenu
    m_recentMenu = m_fileMenu->addMenu(tr("&Recent Files"));
    updateRecentFilesMenu();

    m_fileMenu->addSeparator();

    m_actionSaveAs = m_fileMenu->addAction(tr("Save &As..."));
    m_actionSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(m_actionSaveAs, &QAction::triggered, this, &MainWindow::saveAsDialog);

    m_actionQuickExport = m_fileMenu->addAction(tr("Quick &Export Viewport..."));
    m_actionQuickExport->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    m_actionQuickExport->setShortcutContext(Qt::WindowShortcut);
    addAction(m_actionQuickExport);
    connect(m_actionQuickExport, &QAction::triggered, this, &MainWindow::quickExportViewport);

    m_actionBatchConvert = m_fileMenu->addAction(tr("&Batch Converter..."));
    connect(m_actionBatchConvert, &QAction::triggered, this, &MainWindow::showBatchConverter);

    m_fileMenu->addSeparator();

    // Feature 8: Settings Dialog
    m_actionSettings = m_fileMenu->addAction(tr("&Settings..."));
    m_actionSettings->setShortcut(QKeySequence::Preferences);
    connect(m_actionSettings, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    m_fileMenu->addSeparator();

    m_actionClose = m_fileMenu->addAction(tr("&Close Image"));
    m_actionClose->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actionClose->setShortcutContext(Qt::WindowShortcut);
    addAction(m_actionClose);
    connect(m_actionClose, &QAction::triggered, this, [this]() {
        if (m_vm && m_vm->image()) {
            closeImage();
        } else {
            close();
        }
    });

    m_fileMenu->addSeparator();

    m_actionExit = m_fileMenu->addAction(tr("E&xit"));
    m_actionExit->setShortcut(QKeySequence::Quit);
    connect(m_actionExit, &QAction::triggered, this, &MainWindow::close);

    // ==========================================
    // 2. View Menu
    // ==========================================
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    // Feature 5: Framing Shortcuts 'F' and '1' with active text-input immunity
    m_actionFit = m_viewMenu->addAction(tr("&Fit to Window"));
    m_actionFit->setShortcut(QKeySequence(Qt::Key_F));
    m_actionFit->setShortcutContext(Qt::WindowShortcut);
    connect(m_actionFit, &QAction::triggered, this, [this]() {
        QWidget* fw = QApplication::focusWidget();
        if (!fw)
            fw = focusWidget();
        if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QAbstractSpinBox*>(fw)))
            return;
        m_vm->fitToWindow();
    });

    m_actionActualSize = m_viewMenu->addAction(tr("&Actual Size (1:1)"));
    m_actionActualSize->setShortcut(QKeySequence(Qt::Key_1));
    m_actionActualSize->setShortcutContext(Qt::WindowShortcut);
    connect(m_actionActualSize, &QAction::triggered, this, [this]() {
        QWidget* fw = QApplication::focusWidget();
        if (!fw)
            fw = focusWidget();
        if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QAbstractSpinBox*>(fw)))
            return;
        m_vm->actualSize();
    });

    m_viewMenu->addSeparator();

    m_actionToggleMenuBar = m_viewMenu->addAction(tr("Toggle &Menu Bar"));
    m_actionToggleMenuBar->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    m_actionToggleMenuBar->setShortcutContext(Qt::WindowShortcut);
    m_actionToggleMenuBar->setCheckable(true);
    m_actionToggleMenuBar->setChecked(false);
    // La barre de menus est masquée au démarrage : sans cet enregistrement au
    // niveau fenêtre, le raccourci de l'action ne se déclenche jamais.
    addAction(m_actionToggleMenuBar);
    connect(m_actionToggleMenuBar, &QAction::triggered, this, &MainWindow::toggleMenuBar);

    m_actionFullscreen = m_viewMenu->addAction(tr("Full &Screen"));
    m_actionFullscreen->setShortcuts({QKeySequence(Qt::Key_F11), QKeySequence(Qt::CTRL | Qt::Key_F)});
    m_actionFullscreen->setShortcutContext(Qt::ApplicationShortcut);
    m_actionFullscreen->setCheckable(true);
    addAction(m_actionFullscreen);
    connect(m_actionFullscreen, &QAction::triggered, this, &MainWindow::toggleFullscreen);

    m_actionToggleOverlay = m_viewMenu->addAction(tr("Toggle &Info Overlay (OSD)"));
    m_actionToggleOverlay->setShortcut(QKeySequence(Qt::Key_Tab));
    connect(m_actionToggleOverlay, &QAction::triggered, this, [this]() {
        if (m_viewer) {
            m_viewer->toggleOsd();
        }
    });

    m_viewMenu->addSeparator();

    m_actionResetExposure = m_viewMenu->addAction(tr("Reset &Exposure"));
    m_actionResetExposure->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_actionResetExposure, &QAction::triggered, m_vm, &ViewerViewModel::resetExposure);

    // Channels Submenu
    m_channelsMenu = m_viewMenu->addMenu(tr("&Channels"));
    m_channelGroup = new QActionGroup(this);
    m_channelGroup->setExclusive(true);

    const struct {
        const char* label;
        ChannelMode mode;
        QKeySequence shortcut;
    } channelDefs[] = {
        {"RGB", ChannelMode::RGB, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_0)},
        {"Red", ChannelMode::Red, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1)},
        {"Green", ChannelMode::Green, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2)},
        {"Blue", ChannelMode::Blue, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3)},
        {"Alpha", ChannelMode::Alpha, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_4)},
        {"Luminance", ChannelMode::Luma, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_5)}
    };

    for (const auto& def : channelDefs) {
        auto* act = m_channelsMenu->addAction(tr(def.label));
        act->setCheckable(true);
        act->setShortcut(def.shortcut);
        m_channelGroup->addAction(act);
        if (def.mode == ChannelMode::RGB)
            act->setChecked(true);

        connect(act, &QAction::triggered, this, [this, m = def.mode]() {
            m_vm->setChannel(m);
        });
    }

    m_viewMenu->addSeparator();

    m_actionStepForward = m_viewMenu->addAction(tr("Step &Forward"));
    m_actionStepForward->setShortcut(QKeySequence(Qt::Key_Right));
    m_actionStepForward->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_actionStepForward, &QAction::triggered, this, [this]() {
        m_vm->next();
    });

    m_actionStepBackward = m_viewMenu->addAction(tr("Step &Backward"));
    m_actionStepBackward->setShortcut(QKeySequence(Qt::Key_Left));
    m_actionStepBackward->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_actionStepBackward, &QAction::triggered, this, [this]() {
        m_vm->prev();
    });

    m_actionUndoTransform = m_viewMenu->addAction(tr("&Undo Transform"));
    m_actionUndoTransform->setShortcut(QKeySequence::Undo);
    m_actionUndoTransform->setShortcutContext(Qt::WindowShortcut);
    addAction(m_actionUndoTransform);
    connect(m_actionUndoTransform, &QAction::triggered, this, [this]() {
        QWidget* fw = focusWidget();
        if (!fw) fw = QApplication::focusWidget();
        if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QAbstractSpinBox*>(fw)))
            return;
        if (m_viewer) m_viewer->undoTransform();
    });

    m_actionRedoTransform = m_viewMenu->addAction(tr("&Redo Transform"));
    m_actionRedoTransform->setShortcut(QKeySequence::Redo);
    m_actionRedoTransform->setShortcutContext(Qt::WindowShortcut);
    addAction(m_actionRedoTransform);
    connect(m_actionRedoTransform, &QAction::triggered, this, [this]() {
        QWidget* fw = focusWidget();
        if (!fw) fw = QApplication::focusWidget();
        if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QAbstractSpinBox*>(fw)))
            return;
        if (m_viewer) m_viewer->redoTransform();
    });

    m_viewMenu->addSeparator();

    m_actionFlipH = m_viewMenu->addAction(tr("Flip &Horizontal (⇄)"));
    m_actionFlipH->setCheckable(true);
    m_actionFlipH->setChecked(m_vm->flipH());
    connect(m_actionFlipH, &QAction::toggled, m_vm, &ViewerViewModel::setFlipH);

    m_actionFlipV = m_viewMenu->addAction(tr("Flip &Vertical (⇅)"));
    m_actionFlipV->setCheckable(true);
    m_actionFlipV->setChecked(m_vm->flipV());
    connect(m_actionFlipV, &QAction::toggled, m_vm, &ViewerViewModel::setFlipV);

    m_actionPivot = m_viewMenu->addAction(tr("&Rotate 90° CW (Pivot)"));
    m_actionPivot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_actionPivot->setShortcutContext(Qt::WindowShortcut);
    connect(m_actionPivot, &QAction::triggered, m_vm, &ViewerViewModel::pivot);

    m_viewMenu->addSeparator();

    m_actionToggle360 = m_viewMenu->addAction(tr("360° &Equirectangular View"));
    m_actionToggle360->setCheckable(true);
    m_actionToggle360->setChecked(m_vm->is360Mode());
    m_actionToggle360->setShortcut(QKeySequence(Qt::Key_3));
    connect(m_actionToggle360, &QAction::toggled, m_vm, &ViewerViewModel::set360Mode);

    m_actionToggleSpheres = m_viewMenu->addAction(tr("&Lookdev Spheres (Chrome & Grey)"));
    m_actionToggleSpheres->setCheckable(true);
    m_actionToggleSpheres->setChecked(m_vm->showPreviewSpheres());
    m_actionToggleSpheres->setShortcut(QKeySequence(Qt::Key_L));
    connect(m_actionToggleSpheres, &QAction::toggled, m_vm, &ViewerViewModel::setShowPreviewSpheres);

    m_actionResetCamera360 = m_viewMenu->addAction(tr("&Reset 360° Camera"));
    m_actionResetCamera360->setShortcut(QKeySequence(Qt::Key_R));
    connect(m_actionResetCamera360, &QAction::triggered, m_vm, &ViewerViewModel::resetCamera360);

    m_viewMenu->addSeparator();

    m_actionToggleFloatingDock = m_viewMenu->addAction(tr("Floating &Controls HUD"));
    m_actionToggleFloatingDock->setCheckable(true);
    m_actionToggleFloatingDock->setChecked(SettingsModel::instance().isFloatingDockEnabled());
    connect(m_actionToggleFloatingDock, &QAction::toggled, this, [](bool enabled) {
        if (SettingsModel::instance().isFloatingDockEnabled() != enabled) {
            SettingsModel::instance().setFloatingDockEnabled(enabled);
        }
    });

    // ==========================================
    // 3. Layout Menu
    // ==========================================
    m_layoutMenu = menuBar()->addMenu(tr("&Layout"));

    m_actionSaveLayout = m_layoutMenu->addAction(tr("&Save Layout"));
    m_actionSaveLayout->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_actionSaveLayout, &QAction::triggered, this, &MainWindow::saveLayout);

    m_actionRestoreLayout = m_layoutMenu->addAction(tr("&Load Saved Layout"));
    connect(m_actionRestoreLayout, &QAction::triggered, this, &MainWindow::restoreLayout);

    m_actionResetLayout = m_layoutMenu->addAction(tr("&Reset to Default Layout"));
    m_actionResetLayout->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    connect(m_actionResetLayout, &QAction::triggered, this, &MainWindow::resetLayout);

    auto* actionExpertLayout = m_layoutMenu->addAction(tr("E&xpert Layout"));
    connect(actionExpertLayout, &QAction::triggered, this, [this]() {
        m_dockManager->openPerspective(QStringLiteral("Expert"));
        if (auto* area = m_analysisDock->dockAreaWidget()) {
            m_dockManager->setSplitterSizes(area, {950, 320});
        }
        if (m_timelineDock) {
            if (auto* area = m_timelineDock->dockAreaWidget()) {
                m_dockManager->setSplitterSizes(area, {1000, 74});
            }
        }
        if (m_viewer) {
            m_viewer->fitToScreen();
        }
    });

    m_layoutMenu->addSeparator();

    // Panels Submenu (ADS toggleViewAction checkable actions)
    m_panelsMenu = m_layoutMenu->addMenu(tr("&Panels"));
    m_panelsMenu->addAction(m_viewerDock->toggleViewAction());
    if (m_timelineDock) {
        m_panelsMenu->addAction(m_timelineDock->toggleViewAction());
    }
    m_panelsMenu->addAction(m_analysisDock->toggleViewAction());
    m_panelsMenu->addAction(m_probeDock->toggleViewAction());
    m_panelsMenu->addAction(m_metadataDock->toggleViewAction());
    if (m_folderContentDock) {
        m_panelsMenu->addAction(m_folderContentDock->toggleViewAction());
    }

    // ==========================================
    // 4. Theme Menu
    // ==========================================
    m_themeMenu = menuBar()->addMenu(tr("&Theme"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);

    const QString currentThemeId = SettingsModel::instance().currentTheme();
    const ThemeManager::Theme activeTheme = ThemeManager::themeFromId(currentThemeId);

    for (ThemeManager::Theme theme : ThemeManager::availableThemes()) {
        const QString tName = ThemeManager::themeName(theme);
        const QString tId = ThemeManager::themeId(theme);

        auto* act = m_themeMenu->addAction(tName);
        act->setCheckable(true);
        act->setData(tId);
        m_themeGroup->addAction(act);

        if (theme == activeTheme) {
            act->setChecked(true);
        }

        connect(act, &QAction::triggered, this, [this, tName]() {
            applyTheme(tName);
        });
    }

    // ==========================================
    // 5. Help Menu
    // ==========================================
    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* actionShortcuts = m_helpMenu->addAction(tr("&Keyboard Shortcuts..."));
    connect(actionShortcuts, &QAction::triggered, this, [this]() {
        QMessageBox::information(
            this, tr("Keyboard Shortcuts"),
            tr("<b>File</b><br>"
               "Ctrl+O : Open Image<br>"
               "Ctrl+Shift+O : Open Folder<br>"
               "Ctrl+W : Close Image<br>"
               "Ctrl+Q : Exit<br><br>"
               "<b>Navigation &amp; View</b><br>"
               "Right / Space : Next image<br>"
               "Left : Previous image<br>"
               "Home / End : First / last image<br>"
               "F : Fit to window<br>"
               "1 : Actual size (1:1)<br>"
               "3 : 360° Equirectangular view<br>"
               "L : Lookdev preview spheres<br>"
               "R : Reset 360° camera / Red channel<br>"
               "F11 : Toggle full screen<br>"
               "Ctrl+R : Rotate 90° CW (Pivot)<br>"
               "Tab : Toggle info overlay<br>"
               "Ctrl+E : Reset exposure<br><br>"
               "<b>Layout &amp; Interface</b><br>"
               "Ctrl+M : Toggle menu bar<br>"
               "Ctrl+S : Save layout<br>"
               "Ctrl+Shift+R : Reset default layout"));
    });

    auto* actionAbout = m_helpMenu->addAction(tr("&About SView..."));
    connect(actionAbout, &QAction::triggered, this, [this]() {
        QStringList exts;
        for (const QString& e : FolderModel::supportedExtensions()) {
            exts << e.toUpper();
        }
        const QString formats = exts.join(QStringLiteral(" · "));

        QMessageBox box(this);
        box.setWindowTitle(tr("About SView"));
        box.setWindowIcon(QApplication::windowIcon());
        if (!QApplication::windowIcon().isNull()) {
            box.setIconPixmap(QApplication::windowIcon().pixmap(72, 72));
        }
        box.setTextFormat(Qt::RichText);
        box.setText(
            QStringLiteral(
                "<h3 style='margin-bottom:2px'>SView VFX %1</h3>"
                "<p style='margin-top:0px'>A fast, focused image viewer for visual effects.</p>"
                "<p><b>Supported formats</b><br>%3</p>"
                "<p><b>Walid B.</b><br>"
                "<a href='mailto:bwalid13@gmail.com'>bwalid13@gmail.com</a><br>"
                "<a href='https://github.com/bwalid13'>github.com/bwalid13</a></p>"
                "<p style='color:#9a9a9a'>© 2026 Walid B. — MIT License<br>"
                "Built with Qt %2 (LGPLv3), OpenImageIO, OpenColorIO, OpenEXR and "
                "Qt Advanced Docking System. License texts: LICENSES.txt</p>")
                .arg(QApplication::applicationVersion(),
                     QString::fromLatin1(qVersion()),
                     formats));

        QPushButton* licensesButton =
            box.addButton(tr("Third-party licenses"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Ok);
        box.setDefaultButton(QMessageBox::Ok);
        for (QLabel* label : box.findChildren<QLabel*>()) {
            label->setOpenExternalLinks(true);
            label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        }
        box.exec();

        if (box.clickedButton() == licensesButton) {
            const QString path =
                QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("LICENSES.txt"));
            if (QFile::exists(path)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            } else {
                QMessageBox::information(
                    this, tr("Licenses"),
                    tr("LICENSES.txt was not found next to the executable.\n\n"
                       "Qt 6 — LGPLv3 · Qt Advanced Docking System — LGPL-2.1\n"
                       "OpenImageIO — Apache-2.0 · OpenColorIO — BSD-3\n"
                       "OpenEXR / Imath — BSD-3 · TBB — Apache-2.0"));
            }
        }
    });
}

void MainWindow::setupShortcuts()
{
    // Fullscreen Escape hotkey
    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this]() {
        if (isFullScreen())
            toggleFullscreen();
    });

    new QShortcut(QKeySequence(Qt::Key_Home), this, [this]() {
        if (m_vm) m_vm->first();
    });

    new QShortcut(QKeySequence(Qt::Key_End), this, [this]() {
        if (m_vm) m_vm->last();
    });

    new QShortcut(QKeySequence(Qt::Key_PageUp), this, [this]() {
        if (m_vm) m_vm->prev();
    });

    new QShortcut(QKeySequence(Qt::Key_PageDown), this, [this]() {
        if (m_vm) m_vm->next();
    });

    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this);
    copyShortcut->setContext(Qt::WindowShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &MainWindow::copyCurrentPixel);
}

void MainWindow::setupConnections()
{
    // Viewport drag-and-drop forwarding
    if (m_viewer) {
        connect(m_viewer, &ViewerWidget::fileDropped, this, [this](const QString& path) {
            openFileOrSequence(path, true);
        });
        if (m_probeWidget) {
            connect(m_viewer, &ViewerWidget::copyPixelRequested, m_probeWidget, &PixelProbeDock::copyRgbaFloatToClipboard);
        }
    }

    // ViewModel connections
    connect(m_vm, &ViewerViewModel::overlayChanged, this, &MainWindow::refreshOverlay);
    connect(m_vm, &ViewerViewModel::imageChanged, this, [this]() {
        updateWindowTitle();
        if (m_folderContentWidget && m_vm->image() && !m_vm->image()->path.isEmpty()) {
            m_folderContentWidget->setCurrentFile(m_vm->image()->path, m_vm->image());
        }
    });
    connect(m_vm, &ViewerViewModel::displayChanged, this, [this]() {
        if (m_actionFlipH && m_actionFlipH->isChecked() != m_vm->flipH()) {
            QSignalBlocker blocker(m_actionFlipH);
            m_actionFlipH->setChecked(m_vm->flipH());
        }
        if (m_actionFlipV && m_actionFlipV->isChecked() != m_vm->flipV()) {
            QSignalBlocker blocker(m_actionFlipV);
            m_actionFlipV->setChecked(m_vm->flipV());
        }
    });

    // 360° connections
    connect(m_vm, &ViewerViewModel::mode360Changed, this, [this](bool enabled) {
        if (m_actionToggle360 && m_actionToggle360->isChecked() != enabled) {
            QSignalBlocker blocker(m_actionToggle360);
            m_actionToggle360->setChecked(enabled);
        }
    });
    connect(m_vm, &ViewerViewModel::previewSpheresChanged, this, [this](bool enabled) {
        if (m_actionToggleSpheres && m_actionToggleSpheres->isChecked() != enabled) {
            QSignalBlocker blocker(m_actionToggleSpheres);
            m_actionToggleSpheres->setChecked(enabled);
        }
    });

    // PlaybackController lookahead cache prefetch connection
    connect(m_playback, &PlaybackController::prefetchRequested, this, [this](const QStringList& paths) {
        m_vm->cache().prefetch(paths);
    });

    // AnalysisDock Load Image B connection
    if (m_analysisWidget) {
        connect(m_analysisWidget, &AnalysisDock::requestLoadImageB, this, [this]() {
            QStringList patterns;
            for (const QString& e : FolderModel::supportedExtensions())
                patterns << QStringLiteral("*.") + e;
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Open Secondary Image (Image B)"), QString(), tr("VFX Images (%1)").arg(patterns.join(' ')));
            if (!path.isEmpty()) {
                m_vm->openImageB(path);
            }
        });
    }

    // Floating Control Dock connections
    if (m_floatingDock) {
        connect(m_floatingDock, &FloatingControlDock::menuButtonClicked, this, &MainWindow::toggleMenuBar);
        connect(m_floatingDock, &FloatingControlDock::prevFileClicked, m_vm, &ViewerViewModel::prev);
        connect(m_floatingDock, &FloatingControlDock::nextFileClicked, m_vm, &ViewerViewModel::next);
        connect(m_floatingDock, &FloatingControlDock::pivotClicked, m_vm, &ViewerViewModel::pivot);
        connect(m_floatingDock, &FloatingControlDock::mode360Clicked, m_vm, &ViewerViewModel::set360Mode);
    }

    connect(&SettingsModel::instance(), &SettingsModel::floatingDockEnabledChanged,
            this, [this](bool enabled) {
        if (m_actionToggleFloatingDock && m_actionToggleFloatingDock->isChecked() != enabled) {
            const QSignalBlocker blocker(m_actionToggleFloatingDock);
            m_actionToggleFloatingDock->setChecked(enabled);
        }
        if (m_viewer) {
            m_viewer->setFloatingDockEnabled(enabled);
        }
    });

    if (m_actionStepForward) {
        connect(m_actionStepForward, &QAction::triggered, this, [this]() {
            if (m_floatingDock) m_floatingDock->notifyKeyboardNavigation();
        });
    }
    if (m_actionStepBackward) {
        connect(m_actionStepBackward, &QAction::triggered, this, [this]() {
            if (m_floatingDock) m_floatingDock->notifyKeyboardNavigation();
        });
    }

    // Recent Files updates
    connect(&SettingsModel::instance(), &SettingsModel::recentFilesChanged,
            this, &MainWindow::updateRecentFilesMenu);

    // Dynamic Icon live updates
    connect(&SettingsModel::instance(), &SettingsModel::appIconChanged, this, [this](const QString& path) {
        const QIcon newIcon(path.isEmpty() ? QStringLiteral(":/icons/sview.ico") : path);
        if (!newIcon.isNull()) {
            QApplication::setWindowIcon(newIcon);
            setWindowIcon(newIcon);
        }
    });

    if (m_viewer) {
        connect(m_viewer, &ViewerWidget::fullscreenToggleRequested, this, &MainWindow::toggleFullscreen);
        connect(m_viewer, &ViewerWidget::filesDropped, this, &MainWindow::handleDroppedFiles);
    }

    if (m_titleBar) {
        connect(m_titleBar, &CustomTitleBar::closeRequested, this, &MainWindow::close);
        connect(m_titleBar, &CustomTitleBar::minimizeRequested, this, &MainWindow::showMinimized);
        connect(m_titleBar, &CustomTitleBar::maximizeRequested, this, [this]() {
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
            if (m_titleBar) {
                m_titleBar->updateMaximizeButtonState();
            }
        });
    }
}

void MainWindow::refreshOverlay()
{
    updateWindowTitle();
}

void MainWindow::copyCurrentPixel()
{
    QWidget* fw = QApplication::focusWidget();
    if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QAbstractSpinBox*>(fw))) {
        return;
    }
    if (m_probeWidget) {
        m_probeWidget->copyCurrentPixel();
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = m_vm ? m_vm->windowTitle() : QStringLiteral("SView");
    if (m_vm && m_vm->image()) {
        const auto& img = m_vm->image();
        title += QStringLiteral(" — %1×%2").arg(img->width).arg(img->height);
        if (!img->formatName.isEmpty()) {
            title += QStringLiteral(" (%1)").arg(img->formatName);
        }
        if (m_vm->hasMultipleFolderImages() && m_vm->folderIndex() >= 0) {
            title += QStringLiteral(" [%1/%2]")
                         .arg(m_vm->folderIndex() + 1)
                         .arg(m_vm->folderCount());
        }
    }
    setWindowTitle(title);
    if (m_titleBar) {
        m_titleBar->setTitle(title);
    }
}

void MainWindow::openDialog()
{
    QStringList patterns;
    for (const QString& e : FolderModel::supportedExtensions())
        patterns << QStringLiteral("*.") + e;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Image"), QString(), tr("VFX Images (%1)").arg(patterns.join(' ')));
    if (path.isEmpty())
        return;

    openFileOrSequence(path, true);
    m_viewer->setFocus();
}

void MainWindow::openFolderDialog()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Open Folder"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty())
        return;

    openFileOrSequence(dir, true);
    m_viewer->setFocus();
}

void MainWindow::saveAsDialog()
{
    auto img = m_vm->image();
    if (!img || img->path.isEmpty())
        return;
    const QString filter = tr("Images (*.exr *.hdr *.png *.jpg *.jpeg *.tif *.tiff *.tga)");
    const QString outPath = QFileDialog::getSaveFileName(this, tr("Save As"), img->path, filter);
    if (!outPath.isEmpty()) {
        if (!saveImage(img->path, outPath)) {
            QMessageBox::warning(this, tr("Save Error"), tr("Could not save to %1").arg(outPath));
        }
    }
}

void MainWindow::showBatchConverter()
{
    BatchConverterDialog dlg(this);
    dlg.exec();
}

void MainWindow::closeImage()
{
    if (m_sequence) m_sequence->clear();
    if (m_playback) m_playback->stop();
    if (m_vm) m_vm->closeImage();
    if (m_folderContentWidget) m_folderContentWidget->clear();
    refreshOverlay();
    updateWindowTitle();
    if (m_viewer) m_viewer->update();
}

void MainWindow::quickExportViewport()
{
    if (!m_viewer)
        return;
    QImage frame = m_viewer->captureViewport();
    if (frame.isNull())
        return;

    if (QApplication::platformName() == QStringLiteral("offscreen")) {
        return;
    }

    QString defaultName;
    if (m_vm && m_vm->image() && !m_vm->image()->path.isEmpty()) {
        QFileInfo fi(m_vm->image()->path);
        defaultName = fi.dir().filePath(fi.baseName() + QStringLiteral("_export.png"));
    } else {
        defaultName = QStringLiteral("viewport_export.png");
    }

    const QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;TIFF Image (*.tif *.tiff);;All Files (*.*)");
    const QString outPath = QFileDialog::getSaveFileName(this, tr("Quick Export Viewport"), defaultName, filter);
    if (!outPath.isEmpty()) {
        if (!frame.save(outPath)) {
            QMessageBox::warning(this, tr("Export Error"), tr("Could not export viewport to %1").arg(outPath));
        }
    }
}

void MainWindow::saveLayout()
{
    SettingsModel::instance().setWindowGeometry(saveGeometry());
    SettingsModel::instance().setDockLayoutState(m_dockManager->saveState());
    SettingsModel::instance().sync();
}

void MainWindow::restoreLayout()
{
    const QByteArray geom = SettingsModel::instance().windowGeometry();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    } else {
        resize(1280, 720);
    }

    const QByteArray dockState = SettingsModel::instance().dockLayoutState();
    if (!dockState.isEmpty()) {
        m_dockManager->restoreState(dockState);
        if (m_viewerDock && m_viewerDock->dockAreaWidget()) {
            if (m_viewerDock->dockAreaWidget()->width() < 100) {
                if (auto* area = m_analysisDock->dockAreaWidget()) {
                    m_dockManager->setSplitterSizes(area, {950, 320});
                }
            }
        }
    }
}

void MainWindow::resetLayout()
{
    m_dockManager->openPerspective(QStringLiteral("Default"));
    saveLayout();
}

void MainWindow::toggleMenuBar()
{
    setMenuBarVisible(!menuBar()->isVisible());
}

void MainWindow::setMenuBarVisible(bool visible)
{
    menuBar()->setVisible(visible);
    if (m_actionToggleMenuBar) {
        m_actionToggleMenuBar->setChecked(visible);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt) {
        toggleMenuBar();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && (isFullScreen() || m_isTrueFullscreen)) {
        toggleFullscreen();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen() || m_isTrueFullscreen) {
        // Exit True Fullscreen
        m_isTrueFullscreen = false;

        // Restore window state and geometry accurately
        if (!m_preFullscreenGeometry.isEmpty()) {
            restoreGeometry(m_preFullscreenGeometry);
        } else if (m_preFullscreenNormalGeometry.isValid()) {
            setGeometry(m_preFullscreenNormalGeometry);
        }

        if (m_preFullscreenWasMaximized) {
            showMaximized();
        } else {
            showNormal();
        }

#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(winId());
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
        if (m_topContainer) {
            m_topContainer->setVisible(true);
        }
        if (m_titleBar) {
            m_titleBar->setVisible(true);
            m_titleBar->updateMaximizeButtonState();
        }
        if (statusBar()) {
            statusBar()->setVisible(true);
        }
        setMenuBarVisible(m_preFullscreenMenuBarVisible);
        if (!m_preFullscreenDockState.isEmpty()) {
            m_dockManager->restoreState(m_preFullscreenDockState);
            m_preFullscreenDockState.clear();
        }

        // Restore Viewport dock title bar / tab
        if (m_viewerDock) {
            m_viewerDock->setFeature(ads::CDockWidget::NoTab, false);
            if (auto* area = m_viewerDock->dockAreaWidget()) {
                area->setDockAreaFlag(ads::CDockAreaWidget::HideSingleWidgetTitleBar, false);
                if (auto* tb = area->titleBar()) {
                    tb->setVisible(true);
                    tb->show();
                }
            }
            if (auto* tab = m_viewerDock->tabWidget()) {
                tab->setVisible(true);
                tab->show();
            }
        }

        if (m_actionFullscreen)
            m_actionFullscreen->setChecked(false);
        if (m_viewer) {
            m_viewer->fitToScreen();
        }
    } else {
        // Enter True Fullscreen
        m_isTrueFullscreen = true;
        m_preFullscreenWasMaximized = isMaximized();
        m_preFullscreenGeometry = saveGeometry();
        m_preFullscreenNormalGeometry = normalGeometry();
        m_preFullscreenDockState = m_dockManager->saveState();
        m_preFullscreenMenuBarVisible = menuBar()->isVisible();

        if (m_topContainer) {
            m_topContainer->setVisible(false);
        }
        if (m_titleBar) {
            m_titleBar->setVisible(false);
        }
        menuBar()->setVisible(false);
        if (m_actionToggleMenuBar) {
            m_actionToggleMenuBar->setChecked(false);
        }
        if (statusBar()) {
            statusBar()->setVisible(false);
        }

        if (m_timelineDock) m_timelineDock->toggleView(false);
        if (m_analysisDock) m_analysisDock->toggleView(false);
        if (m_probeDock) m_probeDock->toggleView(false);
        if (m_metadataDock) m_metadataDock->toggleView(false);
        if (m_folderContentDock) m_folderContentDock->toggleView(false);

        // Hide Viewport dock title bar and tab header completely
        if (m_viewerDock) {
            m_viewerDock->setFeature(ads::CDockWidget::NoTab, true);
            if (auto* area = m_viewerDock->dockAreaWidget()) {
                area->setDockAreaFlag(ads::CDockAreaWidget::HideSingleWidgetTitleBar, true);
                if (auto* tb = area->titleBar()) {
                    tb->setVisible(false);
                    tb->hide();
                }
            }
            if (auto* tab = m_viewerDock->tabWidget()) {
                tab->setVisible(false);
                tab->hide();
            }
        }

        showFullScreen();
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(winId());
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (hMon) {
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfo(hMon, &mi)) {
                SetWindowPos(hwnd, HWND_TOP,
                             mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            }
        }
#endif
        if (m_actionFullscreen)
            m_actionFullscreen->setChecked(true);
        if (m_floatingDock) {
            m_floatingDock->startFadeOut();
        }
        if (m_viewer) {
            m_viewer->fitToScreen();
        }
    }
}

void MainWindow::applyTheme(const QString& themeName)
{
    const ThemeManager::Theme targetTheme = ThemeManager::themeFromId(themeName);
    ThemeManager::applyTheme(targetTheme, this, m_dockManager);

    if (m_titleBar) {
        m_titleBar->updateThemeStyle();
    }
    if (m_floatingDock) {
        m_floatingDock->updateThemeStyle(targetTheme);
    }

    const QString targetName = ThemeManager::themeName(targetTheme);
    const QString targetId = ThemeManager::themeId(targetTheme);

    for (QAction* act : m_themeGroup->actions()) {
        if (act->text().compare(targetName, Qt::CaseInsensitive) == 0 ||
            act->data().toString().compare(targetId, Qt::CaseInsensitive) == 0 ||
            act->text().compare(themeName, Qt::CaseInsensitive) == 0) {
            act->setChecked(true);
            break;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveLayout();
    QMainWindow::closeEvent(event);
    if (event->isAccepted()) {
        QApplication::closeAllWindows();
    }
}

// -----------------------------------------------------------------------------
// Drag & Drop Event Handling (Strict test-compatibility preserved)
// -----------------------------------------------------------------------------
void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (!FolderModel::allSupportedPaths(e->mimeData()).isEmpty()) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* e)
{
    if (!FolderModel::allSupportedPaths(e->mimeData()).isEmpty()) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void MainWindow::openFileOrSequence(const QString& path, bool /*autoPlay*/)
{
    SettingsModel::instance().addRecentFile(path);

    m_vm->openPath(path);

    QString actualFilePath = path;
    if (QFileInfo(actualFilePath).isDir()) {
        const QString firstFile = FolderModel::firstSupportedPathInDir(actualFilePath);
        if (!firstFile.isEmpty()) {
            actualFilePath = firstFile;
        } else if (m_vm->image() && !m_vm->image()->path.isEmpty()) {
            actualFilePath = m_vm->image()->path;
        }
    }

    if (m_folderContentWidget) {
        m_folderContentWidget->setCurrentFile(actualFilePath, m_vm->image());
    }

    if (m_sequence) {
        m_sequence->detectSequence(actualFilePath);
    }
    if (m_playback) {
        m_playback->stop();
    }
}

void MainWindow::handleDroppedFiles(const QStringList& paths)
{
    if (paths.isEmpty())
        return;

    // First file is opened as primary
    openFileOrSequence(paths.first());

    // If a second file is provided, open as comparison Image B and enable A/B Wipe
    if (paths.size() > 1) {
        m_vm->openImageB(paths[1]);
        if (m_vm->imageB()) {
            m_vm->setWipeEnabled(true);
        }
    }

    // Register all dropped files in recent files
    for (int i = 0; i < paths.size(); ++i) {
        const QString& p = paths[i];
        SettingsModel::instance().addRecentFile(p);
    }
    updateRecentFilesMenu();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    const QStringList paths = FolderModel::allSupportedPaths(e->mimeData());
    if (!paths.isEmpty()) {
        handleDroppedFiles(paths);
        if (e->possibleActions() & Qt::CopyAction)
            e->setDropAction(Qt::CopyAction);
        e->accept();
        m_viewer->setFocus();
        return;
    }
    e->ignore();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        if (m_titleBar) {
            m_titleBar->updateMaximizeButtonState();
        }
    }
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCCALCSIZE) {
            if (msg->wParam == TRUE) {
                *result = 0;
                return true;
            }
        }
        if (msg->message == WM_GETMINMAXINFO) {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            HMONITOR hMonitor = MonitorFromWindow(reinterpret_cast<HWND>(winId()), MONITOR_DEFAULTTONEAREST);
            if (hMonitor) {
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfo(hMonitor, &mi)) {
                    if (isFullScreen() || m_isTrueFullscreen) {
                        mmi->ptMaxPosition.x = 0;
                        mmi->ptMaxPosition.y = 0;
                        mmi->ptMaxSize.x = std::abs(mi.rcMonitor.right - mi.rcMonitor.left);
                        mmi->ptMaxSize.y = std::abs(mi.rcMonitor.bottom - mi.rcMonitor.top);
                        mmi->ptMaxTrackSize.x = mmi->ptMaxSize.x;
                        mmi->ptMaxTrackSize.y = mmi->ptMaxSize.y;
                    } else {
                        mmi->ptMaxPosition.x = std::abs(mi.rcWork.left - mi.rcMonitor.left);
                        mmi->ptMaxPosition.y = std::abs(mi.rcWork.top - mi.rcMonitor.top);
                        mmi->ptMaxSize.x = std::abs(mi.rcWork.right - mi.rcWork.left);
                        mmi->ptMaxSize.y = std::abs(mi.rcWork.bottom - mi.rcWork.top);
                    }
                    *result = 0;
                    return true;
                }
            }
        }
        if (msg->message == WM_NCHITTEST) {
            if (!isMaximized() && !isFullScreen() && !m_isTrueFullscreen) {
                const int border = 8;
                const qreal dpr = devicePixelRatioF();
                const int screenX = GET_X_LPARAM(msg->lParam);
                const int screenY = GET_Y_LPARAM(msg->lParam);

                QPoint localPos = mapFromGlobal(QPoint(std::round(screenX / dpr), std::round(screenY / dpr)));
                if (!rect().contains(localPos)) {
                    localPos = mapFromGlobal(QPoint(screenX, screenY));
                }
                if (!rect().contains(localPos)) {
                    return false;
                }

                const int x = localPos.x();
                const int y = localPos.y();
                const int w = width();
                const int h = height();

                const bool left = (x < border);
                const bool right = (x >= w - border);
                const bool top = (y < border);
                const bool bottom = (y >= h - border);

                if (top && left) {
                    *result = HTTOPLEFT;
                    return true;
                } else if (top && right) {
                    *result = HTTOPRIGHT;
                    return true;
                } else if (bottom && left) {
                    *result = HTBOTTOMLEFT;
                    return true;
                } else if (bottom && right) {
                    *result = HTBOTTOMRIGHT;
                    return true;
                }

                // Title Bar Buttons Exemption:
                // If cursor is within CustomTitleBar and over m_closeBtn, m_maxBtn, or m_minBtn,
                // return HTCLIENT so mouse clicks are delivered directly to the buttons rather than
                // consumed as resize borders or non-client caption clicks.
                if (m_titleBar && m_titleBar->isVisible()) {
                    const QPoint tbPos = m_titleBar->mapFrom(this, localPos);
                    if (m_titleBar->rect().contains(tbPos)) {
                        auto isOverBtn = [&](QPushButton* btn) {
                            return btn && btn->isVisible() && btn->geometry().contains(tbPos);
                        };
                        if (isOverBtn(m_titleBar->closeButton()) ||
                            isOverBtn(m_titleBar->maximizeButton()) ||
                            isOverBtn(m_titleBar->minimizeButton())) {
                            *result = HTCLIENT;
                            return true;
                        }
                    }
                }

                if (left) {
                    *result = HTLEFT;
                    return true;
                } else if (right) {
                    *result = HTRIGHT;
                    return true;
                } else if (top) {
                    *result = HTTOP;
                    return true;
                } else if (bottom) {
                    *result = HTBOTTOM;
                    return true;
                }
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu)
        return;

    m_recentMenu->clear();
    const QStringList files = SettingsModel::instance().recentFiles();

    if (files.isEmpty()) {
        auto* emptyAct = m_recentMenu->addAction(tr("(No Recent Files)"));
        emptyAct->setEnabled(false);
        return;
    }

    for (int i = 0; i < files.size(); ++i) {
        const QString filePath = files[i];
        const QString displayName = QStringLiteral("%1. %2").arg(i + 1).arg(QFileInfo(filePath).fileName());
        auto* act = m_recentMenu->addAction(displayName);
        act->setToolTip(filePath);
        connect(act, &QAction::triggered, this, [this, filePath]() {
            openFileOrSequence(filePath, true);
        });
    }

    m_recentMenu->addSeparator();
    auto* clearAct = m_recentMenu->addAction(tr("&Clear Recent Files"));
    connect(clearAct, &QAction::triggered, this, []() {
        SettingsModel::instance().clearRecentFiles();
    });
}

void MainWindow::showSettingsDialog()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

}  // namespace sview
