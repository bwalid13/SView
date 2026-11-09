#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <cstddef>

namespace sview {

class SettingsModel : public QObject {
    Q_OBJECT
public:
    explicit SettingsModel(QObject* parent = nullptr);
    ~SettingsModel() override;

    static SettingsModel& instance();

    // RAM Cache configuration
    std::size_t cacheBudgetMB() const;
    std::size_t cacheBudgetBytes() const;
    double cacheBudgetGB() const;
    void setCacheBudgetMB(std::size_t mb);
    void setCacheBudgetGB(double gb);

    // OIIO limits
    bool oiioUnlimitedImageSize() const;
    void setOiioUnlimitedImageSize(bool unlimited);

    // Playback defaults
    double defaultFps() const;
    void setDefaultFps(double fps);

    // UI Theme & layout
    QString currentTheme() const;
    void setCurrentTheme(const QString& theme);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geom);

    QByteArray dockLayoutState() const;
    void setDockLayoutState(const QByteArray& state);

    // UI Floating Controls Dock (Milestone 2)
    bool isFloatingDockEnabled() const;
    void setFloatingDockEnabled(bool enabled);

    // Recent files history (Milestone 3)
    QStringList recentFiles() const;
    void addRecentFile(const QString& path);
    void clearRecentFiles();
    static constexpr int kMaxRecentFiles = 10;

    // Dynamic Icon Management (Milestone 4)
    QString appIconPreset() const;
    void setAppIconPreset(const QString& preset);

    QString customIconPath() const;
    void setCustomIconPath(const QString& path);

    QString effectiveIconPath() const;
    int effectiveIconResourceIndex() const;

    QString appIconPath() const { return effectiveIconPath(); }
    int appIconIndex() const { return effectiveIconResourceIndex(); }
    void setAppIcon(int index, const QString& path);

    // System physical RAM detection
    static std::size_t defaultBudgetMB();

    // Persistence
    void sync();
    void resetToDefaults();

signals:
    void cacheBudgetChanged(std::size_t budgetMB);
    void oiioLimitsChanged(bool unlimited);
    void defaultFpsChanged(double fps);
    void themeChanged(const QString& theme);
    void floatingDockEnabledChanged(bool enabled);
    void recentFilesChanged(const QStringList& files);
    void appIconChanged(const QString& path);
    void appIconPresetChanged(const QString& preset, const QString& path);
    void settingsChanged();

private:
    void load();
    void save();

    std::size_t m_cacheBudgetMB = 4096;
    bool m_oiioUnlimitedImageSize = true;
    double m_defaultFps = 24.0;
    QString m_theme = QStringLiteral("usdview");
    QByteArray m_geometry;
    QByteArray m_dockLayout;
    bool m_floatingDockEnabled = true;
    QStringList m_recentFiles;

    // Milestone 4: Icon management state
    QString m_appIconPreset = QStringLiteral("default");
    QString m_customIconPath;
};

}  // namespace sview
