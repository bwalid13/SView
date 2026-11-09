#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPalette>
#include <QString>

class QMainWindow;
class QMenu;
class QWidget;

namespace ads {
class CDockManager;
}

namespace sview {

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Theme {
        Usdview,
        Maya,
        Blender,
        Nuke,
        Houdini,
        ThreeDsMax
    };
    Q_ENUM(Theme)

    struct PaletteTokens {
        QString name;
        QString id;

        // Container & Backgrounds
        QColor bgWindow;
        QColor bgPanel;
        QColor bgInput;
        QColor bgTitleBar;
        QColor bgHover;
        QColor bgPressed;

        // Tabs
        QColor tabActive;
        QColor tabInactive;
        QColor tabHover;

        // Borders & Dividers
        QColor border;
        QColor borderFocus;
        QColor splitter;

        // Text
        QColor text;
        QColor textMuted;
        QColor textSelected;

        // Accents
        QColor accent;
        QColor accentHover;
        QColor accentPressed;
        QColor accentText;

        // Radii & Geometry
        int borderRadius = 3;
        int tabRadius = 2;
        bool isTranslucent = false;
    };

    // Core Theme Operations
    static void applyTheme(Theme theme, QMainWindow* mainWindow = nullptr, ads::CDockManager* dockManager = nullptr);
    static void applyThemeByName(const QString& nameOrId, QMainWindow* mainWindow = nullptr, ads::CDockManager* dockManager = nullptr);
    static void initFromSettings(QMainWindow* mainWindow = nullptr, ads::CDockManager* dockManager = nullptr);

    // Queries & Metadata
    static Theme currentTheme();
    static QString themeName(Theme theme);
    static QString themeId(Theme theme);
    static Theme themeFromId(const QString& id);
    static QList<Theme> availableThemes();

    // Palette & Stylesheet Generators
    static PaletteTokens paletteTokens(Theme theme);
    static QPalette generatePalette(Theme theme);
    static QString generateStyleSheet(Theme theme);

    // Dynamic Repolishing
    static void repolishStyle(QMainWindow* mainWindow = nullptr, ads::CDockManager* dockManager = nullptr);

    // UI Menu Helper for MainWindow
    static QMenu* createThemeMenu(QMainWindow* mainWindow, ads::CDockManager* dockManager, QWidget* parent = nullptr);

    // Singleton Instance Access
    static ThemeManager& instance();

signals:
    void themeApplied(Theme theme);

private:
    explicit ThemeManager(QObject* parent = nullptr);

    Theme m_currentTheme = Theme::Usdview;
};

}  // namespace sview
