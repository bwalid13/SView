#include "ThemeManager.h"
#include "../model/SettingsModel.h"

#include <DockManager.h>
#include <FloatingDockContainer.h>
#include <ads_globals.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFile>
#include <QMainWindow>
#include <QMenu>
#include <QStyle>
#include <QStyleFactory>
#include <QTextStream>
#include <QWidget>

namespace sview {

namespace {

QString colorToCss(const QColor& c)
{
    if (c.alpha() == 255) {
        return c.name(QColor::HexRgb);
    }
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(c.alphaF(), 0, 'f', 2);
}

QString generateAdsBlock(const ThemeManager::PaletteTokens& p, ThemeManager::Theme theme)
{
    const QString bgWin = colorToCss(p.bgWindow);
    const QString bgPan = colorToCss(p.bgPanel);
    const QString bgTit = colorToCss(p.bgTitleBar);
    const QString tabAct = colorToCss(p.tabActive);
    const QString tabIna = colorToCss(p.tabInactive);
    const QString tabHov = colorToCss(p.tabHover);
    const QString bor = colorToCss(p.border);
    const QString spl = colorToCss(p.splitter);
    const QString txt = colorToCss(p.text);
    const QString txtMut = colorToCss(p.textMuted);
    const QString txtSel = colorToCss(p.textSelected);
    const QString acc = colorToCss(p.accent);
    const QString bgHov = colorToCss(p.bgHover);
    const QString bgPre = colorToCss(p.bgPressed);
    const int r = p.borderRadius;
    const int tr = p.tabRadius;

    QString ads;
    ads.reserve(3072);

    ads += QStringLiteral(
        "/* ========================================================================= */\n"
        "/* QtAdvancedDocking System (ADS) Selectors (Tier 2)                        */\n"
        "/* ========================================================================= */\n"
        "ads--CDockContainerWidget {\n"
        "    background-color: ") + bgWin + QStringLiteral(";\n"
        "}\n\n"
        "ads--CDockAreaWidget {\n"
        "    background-color: ") + bgPan + QStringLiteral(";\n"
        "    border: 1px solid ") + bor + QStringLiteral(";\n"
        "    border-radius: ") + QString::number(r) + QStringLiteral("px;\n"
        "}\n\n"
        "ads--CDockAreaTitleBar {\n"
        "    background-color: ") + bgTit + QStringLiteral(";\n"
        "    border-bottom: 1px solid ") + bor + QStringLiteral(";\n"
        "    min-height: 24px;\n"
        "    max-height: 28px;\n"
        "    padding: 0px 4px;\n"
        "}\n\n"
        "ads--CDockAreaTabBar {\n"
        "    background-color: transparent;\n"
        "    border: none;\n"
        "}\n\n"
        "ads--CDockWidgetTab {\n"
        "    background-color: ") + tabIna + QStringLiteral(";\n"
        "    color: ") + txtMut + QStringLiteral(";\n"
        "    border: 1px solid transparent;\n"
        "    border-top-left-radius: ") + QString::number(tr) + QStringLiteral("px;\n"
        "    border-top-right-radius: ") + QString::number(tr) + QStringLiteral("px;\n"
        "    padding: 3px 8px;\n"
        "    margin-right: 2px;\n"
        "    font-size: 11px;\n"
        "    min-height: 18px;\n"
        "    max-height: 22px;\n"
        "}\n\n"
        "ads--CDockWidgetTab:hover {\n"
        "    background-color: ") + tabHov + QStringLiteral(";\n"
        "    color: ") + txt + QStringLiteral(";\n"
        "}\n\n"
        "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
        "    background-color: ") + tabAct + QStringLiteral(";\n"
        "    color: ") + txtSel + QStringLiteral(";\n"
        "    border-top: 2px solid ") + acc + QStringLiteral(";\n"
        "    font-weight: bold;\n"
        "}\n\n"
        "ads--CDockWidgetTab QLabel {\n"
        "    color: inherit;\n"
        "    background: transparent;\n"
        "}\n\n"
        "ads--CTitleBarButton {\n"
        "    background-color: transparent;\n"
        "    border: none;\n"
        "    padding: 2px;\n"
        "    border-radius: 2px;\n"
        "    min-width: 24px;\n"
        "    min-height: 24px;\n"
        "    qproperty-iconSize: 20px 20px;\n"
        "}\n\n"
        "ads--CTitleBarButton:hover {\n"
        "    background-color: ") + bgHov + QStringLiteral(";\n"
        "}\n\n"
        "ads--CTitleBarButton:pressed {\n"
        "    background-color: ") + bgPre + QStringLiteral(";\n"
        "}\n\n"
        "ads--CDockSplitter {\n"
        "    background-color: ") + spl + QStringLiteral(";\n"
        "}\n\n"
        "ads--CDockSplitter::handle {\n"
        "    background-color: ") + spl + QStringLiteral(";\n"
        "}\n\n"
        "ads--CAutoHideDockContainer {\n"
        "    background-color: ") + bgPan + QStringLiteral(";\n"
        "    border: 1px solid ") + bor + QStringLiteral(";\n"
        "}\n\n"
        "ads--CAutoHideSideBar {\n"
        "    background-color: ") + bgTit + QStringLiteral(";\n"
        "    border: 1px solid ") + bor + QStringLiteral(";\n"
        "}\n\n"
        "ads--CAutoHideTab {\n"
        "    background-color: ") + tabIna + QStringLiteral(";\n"
        "    color: ") + txtMut + QStringLiteral(";\n"
        "    padding: 4px 8px;\n"
        "    border-radius: ") + QString::number(tr) + QStringLiteral("px;\n"
        "}\n\n"
        "ads--CAutoHideTab:hover {\n"
        "    background-color: ") + tabHov + QStringLiteral(";\n"
        "    color: ") + txt + QStringLiteral(";\n"
        "}\n\n"
        "ads--CAutoHideTab[activeTab=\"true\"] {\n"
        "    background-color: ") + tabAct + QStringLiteral(";\n"
        "    color: ") + txtSel + QStringLiteral(";\n"
        "    border-left: 2px solid ") + acc + QStringLiteral(";\n"
        "    font-weight: bold;\n"
        "}\n");

    switch (theme) {
    case ThemeManager::Theme::Maya:
        ads += QStringLiteral(
            "\n/* Maya Razor ADS Specifics */\n"
            "ads--CDockAreaWidget {\n"
            "    border-radius: 0px;\n"
            "    border: 1px solid #1c1c1c;\n"
            "}\n"
            "ads--CDockAreaTitleBar {\n"
            "    min-height: 24px;\n"
            "    max-height: 24px;\n"
            "    background-color: #272727;\n"
            "    border-bottom: 1px solid #1c1c1c;\n"
            "    border-radius: 0px;\n"
            "}\n"
            "ads--CDockWidgetTab {\n"
            "    border-radius: 0px;\n"
            "    padding: 3px 12px;\n"
            "    margin-right: 1px;\n"
            "    min-height: 18px;\n"
            "    max-height: 20px;\n"
            "    background-color: #242424;\n"
            "    border: 1px solid #1c1c1c;\n"
            "    border-bottom: none;\n"
            "    font-size: 11px;\n"
            "}\n"
            "ads--CDockWidgetTab:hover {\n"
            "    background-color: #343434;\n"
            "    color: #ffffff;\n"
            "}\n"
            "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
            "    border-radius: 0px;\n"
            "    background-color: #383838;\n"
            "    border-top: 2px solid #26a69a;\n"
            "    color: #ffffff;\n"
            "    font-weight: bold;\n"
            "}\n"
            "ads--CTitleBarButton {\n"
            "    border-radius: 0px;\n"
            "    min-width: 20px;\n"
            "    min-height: 20px;\n"
            "}\n");
        break;
    case ThemeManager::Theme::Blender:
        ads += QStringLiteral(
            "\n/* Blender Tactile Pill ADS Specifics */\n"
            "ads--CDockAreaWidget {\n"
            "    border-radius: 6px;\n"
            "    border: 1px solid #3d3d3d;\n"
            "}\n"
            "ads--CDockAreaTitleBar {\n"
            "    min-height: 28px;\n"
            "    max-height: 28px;\n"
            "    background-color: #282828;\n"
            "    border-bottom: 1px solid #1a1a1a;\n"
            "    border-top-left-radius: 6px;\n"
            "    border-top-right-radius: 6px;\n"
            "}\n"
            "ads--CDockWidgetTab {\n"
            "    border-top-left-radius: 6px;\n"
            "    border-top-right-radius: 6px;\n"
            "    padding: 4px 14px;\n"
            "    margin-right: 3px;\n"
            "    min-height: 20px;\n"
            "    max-height: 22px;\n"
            "    background-color: #222222;\n"
            "    border: 1px solid #1a1a1a;\n"
            "    border-bottom: none;\n"
            "}\n"
            "ads--CDockWidgetTab:hover {\n"
            "    background-color: #363636;\n"
            "    color: #ffffff;\n"
            "}\n"
            "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
            "    background-color: #353535;\n"
            "    border-top: 2px solid #e67e22;\n"
            "    color: #ffffff;\n"
            "    font-weight: bold;\n"
            "}\n"
            "ads--CTitleBarButton {\n"
            "    border-radius: 4px;\n"
            "    min-width: 22px;\n"
            "    min-height: 22px;\n"
            "}\n");
        break;
    case ThemeManager::Theme::Nuke:
        ads += QStringLiteral(
            "\n/* Nuke Compact ADS Specifics */\n"
            "ads--CDockAreaWidget {\n"
            "    border-radius: 2px;\n"
            "    border: 1px solid #161616;\n"
            "}\n"
            "ads--CDockAreaTitleBar {\n"
            "    min-height: 20px;\n"
            "    max-height: 22px;\n"
            "    padding: 0px 2px;\n"
            "    background-color: #202020;\n"
            "    border-bottom: 1px solid #121212;\n"
            "}\n"
            "ads--CDockWidgetTab {\n"
            "    border-top-left-radius: 2px;\n"
            "    border-top-right-radius: 2px;\n"
            "    padding: 1px 7px;\n"
            "    margin-right: 1px;\n"
            "    min-height: 16px;\n"
            "    max-height: 18px;\n"
            "    font-size: 10px;\n"
            "    background-color: #1a1a1a;\n"
            "    border: 1px solid #141414;\n"
            "    border-bottom: none;\n"
            "}\n"
            "ads--CDockWidgetTab:hover {\n"
            "    background-color: #2c2c2c;\n"
            "    color: #ffffff;\n"
            "}\n"
            "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
            "    background-color: #2c2c2c;\n"
            "    border-top: 2px solid #e5a93b;\n"
            "    color: #ffffff;\n"
            "    font-weight: bold;\n"
            "}\n"
            "ads--CTitleBarButton {\n"
            "    border-radius: 2px;\n"
            "    min-width: 18px;\n"
            "    min-height: 18px;\n"
            "}\n");
        break;
    case ThemeManager::Theme::Houdini:
        ads += QStringLiteral(
            "\n/* Houdini SideFX Metallic ADS Specifics */\n"
            "ads--CDockAreaWidget {\n"
            "    border-radius: 0px;\n"
            "    border: 1px solid #161616;\n"
            "}\n"
            "ads--CDockAreaTitleBar {\n"
            "    min-height: 26px;\n"
            "    max-height: 26px;\n"
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #444444, stop:1 #2d2d2d);\n"
            "    border-bottom: 1px solid #161616;\n"
            "}\n"
            "ads--CDockWidgetTab {\n"
            "    border-radius: 0px;\n"
            "    padding: 3px 10px;\n"
            "    margin-right: 0px;\n"
            "    min-height: 18px;\n"
            "    max-height: 20px;\n"
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #565656, stop:1 #2e2e2e);\n"
            "    border: 1px solid #161616;\n"
            "    border-left: none;\n"
            "}\n"
            "ads--CDockWidgetTab:hover {\n"
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5f5f5f, stop:1 #363636);\n"
            "    color: #ffffff;\n"
            "}\n"
            "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
            "    border-radius: 0px;\n"
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #565656, stop:1 #3a3a3a);\n"
            "    border-top: 3px solid #ea580c;\n"
            "    color: #ffffff;\n"
            "    font-weight: bold;\n"
            "}\n"
            "ads--CTitleBarButton {\n"
            "    border-radius: 0px;\n"
            "    min-width: 22px;\n"
            "    min-height: 22px;\n"
            "}\n");
        break;
    case ThemeManager::Theme::Usdview:
        ads += QStringLiteral(
            "\n/* USDView Pixar Clean ADS Specifics */\n"
            "ads--CDockAreaWidget {\n"
            "    border-radius: 5px;\n"
            "    border: 1px solid #282828;\n"
            "}\n"
            "ads--CDockAreaTitleBar {\n"
            "    min-height: 26px;\n"
            "    max-height: 26px;\n"
            "    background-color: #292929;\n"
            "    border-bottom: 1px solid #222222;\n"
            "    border-top-left-radius: 5px;\n"
            "    border-top-right-radius: 5px;\n"
            "}\n"
            "ads--CDockWidgetTab {\n"
            "    border-top-left-radius: 4px;\n"
            "    border-top-right-radius: 4px;\n"
            "    padding: 4px 12px;\n"
            "    margin-right: 2px;\n"
            "    min-height: 18px;\n"
            "    max-height: 20px;\n"
            "    background-color: #2a2a2a;\n"
            "    border: 1px solid #222222;\n"
            "    border-bottom: none;\n"
            "}\n"
            "ads--CDockWidgetTab:hover {\n"
            "    background-color: #404040;\n"
            "    color: #ffffff;\n"
            "}\n"
            "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
            "    background-color: #383838;\n"
            "    border-top: 2px solid #bd9b54;\n"
            "    color: #ffffff;\n"
            "    font-weight: bold;\n"
            "}\n"
            "ads--CTitleBarButton {\n"
            "    border-radius: 3px;\n"
            "    min-width: 22px;\n"
            "    min-height: 22px;\n"
            "}\n");
        break;
    case ThemeManager::Theme::ThreeDsMax:
        ads += QStringLiteral(
            "\n/* 3ds Max Command Panel ADS Specifics */\n"
            "ads--CDockAreaWidget {\n"
            "    border-radius: 2px;\n"
            "    border: 1px solid #242424;\n"
            "}\n"
            "ads--CDockAreaTitleBar {\n"
            "    min-height: 26px;\n"
            "    max-height: 26px;\n"
            "    background-color: #303030;\n"
            "    border-bottom: 1px solid #202020;\n"
            "}\n"
            "ads--CDockWidgetTab {\n"
            "    border-top-left-radius: 2px;\n"
            "    border-top-right-radius: 2px;\n"
            "    padding: 4px 10px;\n"
            "    margin-right: 2px;\n"
            "    min-height: 18px;\n"
            "    max-height: 20px;\n"
            "    background-color: #262626;\n"
            "    border: 1px solid #202020;\n"
            "    border-bottom: none;\n"
            "}\n"
            "ads--CDockWidgetTab:hover {\n"
            "    background-color: #383838;\n"
            "    color: #ffffff;\n"
            "}\n"
            "ads--CDockWidgetTab[activeTab=\"true\"] {\n"
            "    background-color: #3a3a3a;\n"
            "    border-top: 2px solid #00a2e8;\n"
            "    color: #ffffff;\n"
            "    font-weight: bold;\n"
            "}\n"
            "ads--CTitleBarButton {\n"
            "    border-radius: 2px;\n"
            "    min-width: 22px;\n"
            "    min-height: 22px;\n"
            "}\n");
        break;
    }

    // Bouton de fermeture des onglets : la règle générique ads--CTitleBarButton
    // (24 px, icône 20 px) est calibrée pour la barre de titre du dock et
    // déborde sur les onglets de 18-20 px. Sélecteur plus spécifique pour
    // qu'il gagne sur les blocs par thème ci-dessus.
    ads += QStringLiteral(
        "\n/* Tab close button — compact, must fit inside an 18-20px tab */\n"
        "ads--CDockWidgetTab ads--CTitleBarButton,\n"
        "QAbstractButton#tabCloseButton {\n"
        "    min-width: 14px;\n"
        "    min-height: 14px;\n"
        "    max-width: 14px;\n"
        "    max-height: 14px;\n"
        "    padding: 0px;\n"
        "    margin: 0px 2px 0px 4px;\n"
        "    border: none;\n"
        "    border-radius: 2px;\n"
        "    background: transparent;\n"
        "    qproperty-iconSize: 10px 10px;\n"
        "}\n\n"
        "ads--CDockWidgetTab ads--CTitleBarButton:hover,\n"
        "QAbstractButton#tabCloseButton:hover {\n"
        "    background-color: rgba(232, 17, 35, 0.85);\n"
        "}\n");

    return ads;
}

QString generateIsolationGuards()
{
    const auto p = ThemeManager::paletteTokens(ThemeManager::currentTheme());
    const QString acc = colorToCss(p.accent);
    QColor accBg = p.accent;
    accBg.setAlpha(115);
    const QString accBgCss = colorToCss(accBg);

    return QStringLiteral(
        "/* ========================================================================= */\n"
        "/* SVIEW CRITICAL COMPONENT ISOLATION & OVERRIDE GUARDS (Tier 3)            */\n"
        "/* ========================================================================= */\n\n"
        "/* --- 1. FloatingControlDock Isolation --- */\n"
        "#floatingControlDock QPushButton {\n"
        "    background: transparent !important;\n"
        "    color: ") + colorToCss(p.text) + QStringLiteral(" !important;\n"
        "    border: none !important;\n"
        "    border-radius: 14px !important;\n"
        "    padding: 0px !important;\n"
        "    margin: 0px !important;\n"
        "    min-width: 0px !important;\n"
        "    min-height: 0px !important;\n"
        "}\n\n"
        "#floatingControlDock QPushButton:hover {\n"
        "    background-color: rgba(255, 255, 255, 0.16) !important;\n"
        "    color: #ffffff !important;\n"
        "}\n\n"
        "#floatingControlDock QPushButton:pressed {\n"
        "    background-color: rgba(255, 255, 255, 0.28) !important;\n"
        "    color: #ffffff !important;\n"
        "}\n\n"
        "#floatingControlDock QPushButton:checked {\n"
        "    background-color: ") + accBgCss + QStringLiteral(" !important;\n"
        "    color: #ffffff !important;\n"
        "    border: 1px solid ") + acc + QStringLiteral(" !important;\n"
        "}\n\n"
        "#floatingControlDock QPushButton:disabled {\n"
        "    color: rgba(255, 255, 255, 0.25) !important;\n"
        "}\n\n"
        "#floatingControlDock QFrame#floatingSeparator {\n"
        "    background-color: rgba(255, 255, 255, 0.20) !important;\n"
        "    border: none !important;\n"
        "    width: 1px !important;\n"
        "    min-width: 1px !important;\n"
        "    max-width: 1px !important;\n"
        "    margin: 6px 3px !important;\n"
        "}\n\n"
        "/* --- 2. TimelineDock Transport Buttons & Scrubber Isolation --- */\n"
        "#TimelineDockWidgetContent QPushButton {\n"
        "    padding: 0px !important;\n"
        "    margin: 0px 1px !important;\n"
        "    min-width: 28px !important;\n"
        "    min-height: 22px !important;\n"
        "    font-size: 12px !important;\n"
        "}\n\n"
        "#TimelineDockWidgetContent QPushButton#timelineLoopButton {\n"
        "    min-width: 72px !important;\n"
        "    padding: 2px 8px !important;\n"
        "}\n\n"
        "#TimelineDockWidgetContent QSlider#timelineSlider::groove:horizontal {\n"
        "    height: 8px !important;\n"
        "    background: #1e1e1e !important;\n"
        "    border: 1px solid #383838 !important;\n"
        "    border-radius: 4px !important;\n"
        "}\n\n"
        "#TimelineDockWidgetContent QSlider#timelineSlider::sub-page:horizontal {\n"
        "    background: transparent !important;\n"
        "}\n\n"
        "#TimelineDockWidgetContent QSlider#timelineSlider::handle:horizontal {\n"
        "    width: 14px !important;\n"
        "    height: 14px !important;\n"
        "    margin: -3px 0 !important;\n"
        "    border-radius: 7px !important;\n"
        "}\n\n"
        "/* --- 3. AnalysisDock Compact Reset Buttons Scoping --- */\n"
        "QWidget#AnalysisDockWidget QPushButton#exposureResetBtn,\n"
        "QWidget#AnalysisDockWidget QPushButton#gammaResetBtn,\n"
        "QWidget#AnalysisDockWidget QPushButton[text=\"Reset\"] {\n"
        "    min-width: 48px !important;\n"
        "    max-width: 54px !important;\n"
        "    padding: 2px 4px !important;\n"
        "}\n\n"
        "/* --- 4. Viewport OpenGL Surface Immunity --- */\n"
        "QOpenGLWidget, ViewerWidget {\n"
        "    background-color: transparent !important;\n"
        "    border: none !important;\n"
        "}\n"
    );
}

}  // namespace

ThemeManager& ThemeManager::instance()
{
    static ThemeManager s_instance;
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    const QString saved = SettingsModel::instance().currentTheme();
    m_currentTheme = themeFromId(saved);
}

ThemeManager::Theme ThemeManager::currentTheme()
{
    return instance().m_currentTheme;
}

QList<ThemeManager::Theme> ThemeManager::availableThemes()
{
    return {
        Theme::Usdview,
        Theme::Maya,
        Theme::Blender,
        Theme::Nuke,
        Theme::Houdini,
        Theme::ThreeDsMax
    };
}

QString ThemeManager::themeName(Theme theme)
{
    switch (theme) {
    case Theme::Usdview:     return QStringLiteral("USDView");
    case Theme::Maya:        return QStringLiteral("Maya");
    case Theme::Blender:     return QStringLiteral("Blender");
    case Theme::Nuke:        return QStringLiteral("Nuke");
    case Theme::Houdini:     return QStringLiteral("Houdini");
    case Theme::ThreeDsMax:  return QStringLiteral("3ds Max");
    }
    return QStringLiteral("USDView");
}

QString ThemeManager::themeId(Theme theme)
{
    switch (theme) {
    case Theme::Usdview:     return QStringLiteral("usdview");
    case Theme::Maya:        return QStringLiteral("maya");
    case Theme::Blender:     return QStringLiteral("blender");
    case Theme::Nuke:        return QStringLiteral("nuke");
    case Theme::Houdini:     return QStringLiteral("houdini");
    case Theme::ThreeDsMax:  return QStringLiteral("3dsmax");
    }
    return QStringLiteral("usdview");
}

ThemeManager::Theme ThemeManager::themeFromId(const QString& id)
{
    const QString clean = id.trimmed().toLower();
    if (clean == QLatin1StringView("maya") || clean == QLatin1StringView("teal green"))
        return Theme::Maya;
    if (clean == QLatin1StringView("blender") || clean == QLatin1StringView("bright orange"))
        return Theme::Blender;
    if (clean == QLatin1StringView("nuke") || clean == QLatin1StringView("golden yellow"))
        return Theme::Nuke;
    if (clean == QLatin1StringView("houdini") || clean == QLatin1StringView("burnt orange"))
        return Theme::Houdini;
    if (clean == QLatin1StringView("3dsmax") || clean == QLatin1StringView("3ds max") ||
        clean == QLatin1StringView("3ds_max") || clean == QLatin1StringView("max") ||
        clean == QLatin1StringView("threedsmax") || clean == QLatin1StringView("cyan blue"))
        return Theme::ThreeDsMax;
    if (clean == QLatin1StringView("usdview") || clean == QLatin1StringView("usd view") ||
        clean == QLatin1StringView("usd") || clean == QLatin1StringView("openusd") ||
        clean == QLatin1StringView("classic blue") ||
        clean == QLatin1StringView("liquidglass") || clean == QLatin1StringView("liquid_glass") ||
        clean == QLatin1StringView("liquid glass"))
        return Theme::Usdview;
    return Theme::Usdview;
}

ThemeManager::PaletteTokens ThemeManager::paletteTokens(Theme theme)
{
    PaletteTokens p;

    switch (theme) {
    case Theme::Usdview:
        p.name = QStringLiteral("USDView");
        p.id = QStringLiteral("usdview");
        p.bgWindow = QColor(0x2e, 0x2e, 0x2e);
        p.bgPanel = QColor(0x38, 0x38, 0x38);
        p.bgInput = QColor(0x22, 0x22, 0x22);
        p.bgTitleBar = QColor(0x29, 0x29, 0x29);
        p.bgHover = QColor(0x44, 0x44, 0x44);
        p.bgPressed = QColor(0x19, 0x19, 0x19);
        p.tabActive = QColor(0x38, 0x38, 0x38);
        p.tabInactive = QColor(0x2a, 0x2a, 0x2a);
        p.tabHover = QColor(0x40, 0x40, 0x40);
        p.border = QColor(0x22, 0x22, 0x22);
        p.borderFocus = QColor(0xbd, 0x9b, 0x54);
        p.splitter = QColor(0x28, 0x28, 0x28);
        p.text = QColor(0xe3, 0xe3, 0xe3);
        p.textMuted = QColor(0x7a, 0x7a, 0x7a);
        p.textSelected = QColor(0xff, 0xff, 0xff);
        p.accent = QColor(0xbd, 0x9b, 0x54);
        p.accentHover = QColor(0xde, 0x9e, 0x2e);
        p.accentPressed = QColor(0xa0, 0x80, 0x3e);
        p.accentText = QColor(0x22, 0x22, 0x22);
        p.borderRadius = 5;
        p.tabRadius = 3;
        p.isTranslucent = false;
        break;

    case Theme::Maya:
        p.name = QStringLiteral("Maya");
        p.id = QStringLiteral("maya");
        p.bgWindow = QColor(0x2b, 0x2b, 0x2b);
        p.bgPanel = QColor(0x32, 0x32, 0x32);
        p.bgInput = QColor(0x24, 0x24, 0x24);
        p.bgTitleBar = QColor(0x27, 0x27, 0x27);
        p.bgHover = QColor(0x3d, 0x3d, 0x3d);
        p.bgPressed = QColor(0x1f, 0x1f, 0x1f);
        p.tabActive = QColor(0x3c, 0x3c, 0x3c);
        p.tabInactive = QColor(0x28, 0x28, 0x28);
        p.tabHover = QColor(0x34, 0x34, 0x34);
        p.border = QColor(0x1c, 0x1c, 0x1c);
        p.borderFocus = QColor(0x26, 0xa6, 0x9a);
        p.splitter = QColor(0x22, 0x22, 0x22);
        p.text = QColor(0xe0, 0xe0, 0xe0);
        p.textMuted = QColor(0x85, 0x85, 0x85);
        p.textSelected = QColor(0xff, 0xff, 0xff);
        p.accent = QColor(0x26, 0xa6, 0x9a);
        p.accentHover = QColor(0x4d, 0xb6, 0xac);
        p.accentPressed = QColor(0x00, 0x79, 0x6b);
        p.accentText = QColor(0xff, 0xff, 0xff);
        p.borderRadius = 0;  // Sharp bevels
        p.tabRadius = 0;
        p.isTranslucent = false;
        break;

    case Theme::Blender:
        p.name = QStringLiteral("Blender");
        p.id = QStringLiteral("blender");
        p.bgWindow = QColor(0x30, 0x30, 0x30);
        p.bgPanel = QColor(0x23, 0x23, 0x23);
        p.bgInput = QColor(0x18, 0x18, 0x18);
        p.bgTitleBar = QColor(0x28, 0x28, 0x28);
        p.bgHover = QColor(0x38, 0x38, 0x38);
        p.bgPressed = QColor(0x14, 0x14, 0x14);
        p.tabActive = QColor(0x3e, 0x3e, 0x3e);
        p.tabInactive = QColor(0x26, 0x26, 0x26);
        p.tabHover = QColor(0x33, 0x33, 0x33);
        p.border = QColor(0x3d, 0x3d, 0x3d);
        p.borderFocus = QColor(0xe6, 0x7e, 0x22);
        p.splitter = QColor(0x28, 0x28, 0x28);
        p.text = QColor(0xff, 0xff, 0xff);
        p.textMuted = QColor(0x99, 0x99, 0x99);
        p.textSelected = QColor(0xff, 0xff, 0xff);
        p.accent = QColor(0xe6, 0x7e, 0x22);
        p.accentHover = QColor(0xf3, 0x9c, 0x12);
        p.accentPressed = QColor(0xd3, 0x54, 0x00);
        p.accentText = QColor(0xff, 0xff, 0xff);
        p.borderRadius = 6;  // Smooth rounded pill corners
        p.tabRadius = 6;
        p.isTranslucent = false;
        break;

    case Theme::Nuke:
        p.name = QStringLiteral("Nuke");
        p.id = QStringLiteral("nuke");
        p.bgWindow = QColor(0x1e, 0x1e, 0x1e);
        p.bgPanel = QColor(0x25, 0x25, 0x25);
        p.bgInput = QColor(0x18, 0x18, 0x18);
        p.bgTitleBar = QColor(0x20, 0x20, 0x20);
        p.bgHover = QColor(0x2d, 0x2d, 0x2d);
        p.bgPressed = QColor(0x14, 0x14, 0x14);
        p.tabActive = QColor(0x32, 0x32, 0x32);
        p.tabInactive = QColor(0x20, 0x20, 0x20);
        p.tabHover = QColor(0x2a, 0x2a, 0x2a);
        p.border = QColor(0x16, 0x16, 0x16);
        p.borderFocus = QColor(0xd4, 0xac, 0x0d);
        p.splitter = QColor(0x18, 0x18, 0x18);
        p.text = QColor(0xd8, 0xd8, 0xd8);
        p.textMuted = QColor(0x7c, 0x7c, 0x7c);
        p.textSelected = QColor(0xff, 0xff, 0xff);
        p.accent = QColor(0xd4, 0xac, 0x0d);
        p.accentHover = QColor(0xf1, 0xc4, 0x0f);
        p.accentPressed = QColor(0xb7, 0x95, 0x0b);
        p.accentText = QColor(0x1a, 0x1a, 0x1a);
        p.borderRadius = 2;  // Compact VFX studio feel
        p.tabRadius = 2;
        p.isTranslucent = false;
        break;

    case Theme::Houdini:
        p.name = QStringLiteral("Houdini");
        p.id = QStringLiteral("houdini");
        p.bgWindow = QColor(0x28, 0x28, 0x28);
        p.bgPanel = QColor(0x2c, 0x2c, 0x2c);
        p.bgInput = QColor(0x1f, 0x1f, 0x1f);
        p.bgTitleBar = QColor(0x33, 0x33, 0x33);
        p.bgHover = QColor(0x3a, 0x3a, 0x3a);
        p.bgPressed = QColor(0x18, 0x18, 0x18);
        p.tabActive = QColor(0x36, 0x36, 0x36);
        p.tabInactive = QColor(0x24, 0x24, 0x24);
        p.tabHover = QColor(0x2f, 0x2f, 0x2f);
        p.border = QColor(0x1c, 0x1c, 0x1c);
        p.borderFocus = QColor(0xea, 0x58, 0x0c);
        p.splitter = QColor(0x20, 0x20, 0x20);
        p.text = QColor(0xc8, 0xc8, 0xc8);
        p.textMuted = QColor(0x7e, 0x7e, 0x7e);
        p.textSelected = QColor(0xff, 0xff, 0xff);
        p.accent = QColor(0xea, 0x58, 0x0c);
        p.accentHover = QColor(0xf9, 0x73, 0x16);
        p.accentPressed = QColor(0xc2, 0x41, 0x0c);
        p.accentText = QColor(0xff, 0xff, 0xff);
        p.borderRadius = 0;  // Industrial chamfer
        p.tabRadius = 0;
        p.isTranslucent = false;
        break;

    case Theme::ThreeDsMax:
        p.name = QStringLiteral("3ds Max");
        p.id = QStringLiteral("3dsmax");
        p.bgWindow = QColor(0x38, 0x38, 0x38);
        p.bgPanel = QColor(0x32, 0x32, 0x32);
        p.bgInput = QColor(0x24, 0x24, 0x24);
        p.bgTitleBar = QColor(0x2c, 0x2c, 0x2c);
        p.bgHover = QColor(0x44, 0x44, 0x44);
        p.bgPressed = QColor(0x20, 0x20, 0x20);
        p.tabActive = QColor(0x38, 0x38, 0x38);
        p.tabInactive = QColor(0x29, 0x29, 0x29);
        p.tabHover = QColor(0x33, 0x33, 0x33);
        p.border = QColor(0x22, 0x22, 0x22);
        p.borderFocus = QColor(0x00, 0xa2, 0xe8);
        p.splitter = QColor(0x26, 0x26, 0x26);
        p.text = QColor(0xe0, 0xe0, 0xe0);
        p.textMuted = QColor(0x8a, 0x8a, 0x8a);
        p.textSelected = QColor(0xff, 0xff, 0xff);
        p.accent = QColor(0x00, 0xa2, 0xe8);
        p.accentHover = QColor(0x38, 0xbd, 0xf8);
        p.accentPressed = QColor(0x02, 0x84, 0xc7);
        p.accentText = QColor(0xff, 0xff, 0xff);
        p.borderRadius = 2;
        p.tabRadius = 2;
        p.isTranslucent = false;
        break;
    }

    return p;
}

QPalette ThemeManager::generatePalette(Theme theme)
{
    const PaletteTokens p = paletteTokens(theme);

    QPalette pal;
    pal.setColor(QPalette::Window, p.bgWindow);
    pal.setColor(QPalette::WindowText, p.text);
    pal.setColor(QPalette::Base, p.bgInput);
    pal.setColor(QPalette::AlternateBase, p.bgPanel);
    pal.setColor(QPalette::ToolTipBase, p.bgPanel);
    pal.setColor(QPalette::ToolTipText, p.text);
    pal.setColor(QPalette::Text, p.text);
    pal.setColor(QPalette::Button, p.bgPanel);
    pal.setColor(QPalette::ButtonText, p.text);
    pal.setColor(QPalette::BrightText, p.textSelected);
    pal.setColor(QPalette::Highlight, p.accent);
    pal.setColor(QPalette::HighlightedText, p.accentText);

    pal.setColor(QPalette::Disabled, QPalette::WindowText, p.textMuted);
    pal.setColor(QPalette::Disabled, QPalette::Text, p.textMuted);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, p.textMuted);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, p.bgPanel);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, p.textMuted);

    pal.setColor(QPalette::Dark, p.border);
    pal.setColor(QPalette::Mid, p.bgPanel);
    pal.setColor(QPalette::Shadow, QColor(0, 0, 0, 120));

    return pal;
}


QString ThemeManager::generateStyleSheet(Theme theme)
{
    const QString id = themeId(theme);
    const PaletteTokens p = paletteTokens(theme);

    // Tier 1: Authentic DCC stylesheet from Qt resources or local assets
    QString baseQss;
    for (const QString& path : { QStringLiteral(":/themes/themes/") + id + QStringLiteral(".qss"),
                                 QStringLiteral("res/themes/") + id + QStringLiteral(".qss") }) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            baseQss = stream.readAll();
            if (!baseQss.isEmpty()) {
                break;
            }
        }
    }

    if (baseQss.isEmpty()) {
        baseQss = QStringLiteral("QWidget { background-color: #2b2b2b; color: #d4d4d4; }");
    }

    // Tier 2: Theme-tailored ADS Docking System Block (all 12 required ads-- selectors)
    const QString adsQss = generateAdsBlock(p, theme);

    // Tier 3: Sview Component Isolation Guards
    const QString isolationQss = generateIsolationGuards();

    return baseQss + QStringLiteral("\n\n") + adsQss + QStringLiteral("\n\n") + isolationQss;
}

void repolishWidget(QWidget* w)
{
    if (!w)
        return;
    if (w->style()) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
    const auto children = w->children();
    for (QObject* child : children) {
        if (QWidget* cw = qobject_cast<QWidget*>(child)) {
            repolishWidget(cw);
        }
    }
}

void ThemeManager::repolishStyle(QMainWindow* mainWindow, ads::CDockManager* dockManager)
{
    // Clear internal stylesheet and repolish dockManager hierarchy
    if (dockManager) {
        dockManager->setStyleSheet(QString());
        repolishWidget(dockManager);

        for (ads::CFloatingDockContainer* floating : dockManager->floatingWidgets()) {
            if (floating) {
                repolishWidget(floating);
                floating->update();
            }
        }
    }

    if (mainWindow) {
        repolishWidget(mainWindow);
        mainWindow->update();
    }

    // Repolish all remaining top-level windows (e.g. dialogs, floating toolbars)
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
        if (topLevel && topLevel != mainWindow) {
            repolishWidget(topLevel);
            topLevel->update();
        }
    }
}

void ThemeManager::applyTheme(Theme theme, QMainWindow* mainWindow, ads::CDockManager* dockManager)
{
    instance().m_currentTheme = theme;

    // Persist to SettingsModel
    const QString id = themeId(theme);
    if (SettingsModel::instance().currentTheme() != id) {
        SettingsModel::instance().setCurrentTheme(id);
    }

    // Apply baseline style and global stylesheet
    if (QApplication::instance()) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(generatePalette(theme));
        qApp->setStyleSheet(generateStyleSheet(theme));
    }

    // Repolish all widgets and floating docks
    repolishStyle(mainWindow, dockManager);

    emit instance().themeApplied(theme);
}

void ThemeManager::applyThemeByName(const QString& nameOrId, QMainWindow* mainWindow, ads::CDockManager* dockManager)
{
    applyTheme(themeFromId(nameOrId), mainWindow, dockManager);
}

void ThemeManager::initFromSettings(QMainWindow* mainWindow, ads::CDockManager* dockManager)
{
    const QString saved = SettingsModel::instance().currentTheme();
    applyTheme(themeFromId(saved), mainWindow, dockManager);
}

QMenu* ThemeManager::createThemeMenu(QMainWindow* mainWindow, ads::CDockManager* dockManager, QWidget* parent)
{
    auto* menu = new QMenu(QStringLiteral("&Theme"), parent);
    auto* group = new QActionGroup(menu);
    group->setExclusive(true);

    const Theme current = currentTheme();
    for (Theme theme : availableThemes()) {
        auto* action = new QAction(themeName(theme), menu);
        action->setCheckable(true);
        action->setData(themeId(theme));
        action->setChecked(theme == current);
        group->addAction(action);
        menu->addAction(action);

        QObject::connect(action, &QAction::triggered, [theme, mainWindow, dockManager]() {
            applyTheme(theme, mainWindow, dockManager);
        });
    }

    return menu;
}

}  // namespace sview
