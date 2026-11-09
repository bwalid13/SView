#include "SettingsModel.h"

#include <QDir>
#include <QFile>
#include <QSettings>

#include <algorithm>
#include <cmath>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace sview {

namespace {
constexpr const char* kKeyCacheBudgetMB = "cache/budget_MB";
constexpr const char* kKeyOiioUnlimited = "cache/oiio_unlimited_imagesize";
constexpr const char* kKeyDefaultFps = "playback/default_fps";
constexpr const char* kKeyTheme = "ui/theme";
constexpr const char* kKeyGeometry = "ui/geometry";
constexpr const char* kKeyDockLayout = "ui/dock_layout";
constexpr const char* kKeyFloatingDockEnabled = "ui/floatingDockEnabled";
constexpr const char* kKeyRecentFiles = "files/recent";
constexpr const char* kKeyAppIconPreset = "icon/preset";
constexpr const char* kKeyCustomIconPath = "icon/custom_path";
}  // namespace

SettingsModel& SettingsModel::instance()
{
    static SettingsModel s_instance;
    return s_instance;
}

std::size_t SettingsModel::defaultBudgetMB()
{
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        // Recommend 50% of installed physical RAM, clamped between 4 GB and 256 GB
        const qulonglong totalMB = memInfo.ullTotalPhys / (1024ULL * 1024ULL);
        const qulonglong halfMB = totalMB / 2ULL;
        return static_cast<std::size_t>(std::clamp<qulonglong>(halfMB, 4096ULL, 262144ULL));
    }
#endif
    return 4096ULL; // Fallback default 4 GB
}

SettingsModel::SettingsModel(QObject* parent) : QObject(parent)
{
    load();
}

SettingsModel::~SettingsModel()
{
    sync();
}

void SettingsModel::load()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SView"), QStringLiteral("SView"));

    bool ok = false;
    const qulonglong valMB = settings.value(QString::fromLatin1(kKeyCacheBudgetMB)).toULongLong(&ok);
    if (ok && valMB >= 512ULL) {
        m_cacheBudgetMB = static_cast<std::size_t>(valMB);
    } else {
        m_cacheBudgetMB = defaultBudgetMB();
    }

    m_oiioUnlimitedImageSize = settings.value(QString::fromLatin1(kKeyOiioUnlimited), true).toBool();
    m_defaultFps = settings.value(QString::fromLatin1(kKeyDefaultFps), 24.0).toDouble();
    m_theme = settings.value(QString::fromLatin1(kKeyTheme), QStringLiteral("usdview")).toString();
    m_geometry = settings.value(QString::fromLatin1(kKeyGeometry)).toByteArray();
    m_dockLayout = settings.value(QString::fromLatin1(kKeyDockLayout)).toByteArray();
    m_floatingDockEnabled = settings.value(QString::fromLatin1(kKeyFloatingDockEnabled), true).toBool();
    m_recentFiles = settings.value(QString::fromLatin1(kKeyRecentFiles)).toStringList();
    m_appIconPreset = settings.value(QString::fromLatin1(kKeyAppIconPreset), QStringLiteral("default")).toString();
    m_customIconPath = settings.value(QString::fromLatin1(kKeyCustomIconPath), QString()).toString();
}

void SettingsModel::save()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SView"), QStringLiteral("SView"));
    settings.setValue(QString::fromLatin1(kKeyCacheBudgetMB), static_cast<qulonglong>(m_cacheBudgetMB));
    settings.setValue(QString::fromLatin1(kKeyOiioUnlimited), m_oiioUnlimitedImageSize);
    settings.setValue(QString::fromLatin1(kKeyDefaultFps), m_defaultFps);
    settings.setValue(QString::fromLatin1(kKeyTheme), m_theme);
    settings.setValue(QString::fromLatin1(kKeyFloatingDockEnabled), m_floatingDockEnabled);
    settings.setValue(QString::fromLatin1(kKeyRecentFiles), m_recentFiles);
    settings.setValue(QString::fromLatin1(kKeyAppIconPreset), m_appIconPreset);
    settings.setValue(QString::fromLatin1(kKeyCustomIconPath), m_customIconPath);
    if (!m_geometry.isEmpty())
        settings.setValue(QString::fromLatin1(kKeyGeometry), m_geometry);
    if (!m_dockLayout.isEmpty())
        settings.setValue(QString::fromLatin1(kKeyDockLayout), m_dockLayout);
    settings.sync();
}

void SettingsModel::sync()
{
    save();
}

void SettingsModel::resetToDefaults()
{
    m_cacheBudgetMB = defaultBudgetMB();
    m_oiioUnlimitedImageSize = true;
    m_defaultFps = 24.0;
    m_theme = QStringLiteral("usdview");
    m_geometry.clear();
    m_dockLayout.clear();
    m_floatingDockEnabled = true;
    m_recentFiles.clear();
    m_appIconPreset = QStringLiteral("default");
    m_customIconPath.clear();

    save();

    emit cacheBudgetChanged(m_cacheBudgetMB);
    emit oiioLimitsChanged(m_oiioUnlimitedImageSize);
    emit defaultFpsChanged(m_defaultFps);
    emit themeChanged(m_theme);
    emit floatingDockEnabledChanged(m_floatingDockEnabled);
    emit recentFilesChanged(m_recentFiles);
    emit appIconPresetChanged(m_appIconPreset, effectiveIconPath());
    emit appIconChanged(effectiveIconPath());
    emit settingsChanged();
}

QStringList SettingsModel::recentFiles() const
{
    return m_recentFiles;
}

void SettingsModel::addRecentFile(const QString& path)
{
    if (path.isEmpty())
        return;

    const QString clean = QDir::cleanPath(path);
    m_recentFiles.removeAll(clean);
    m_recentFiles.prepend(clean);

    while (m_recentFiles.size() > kMaxRecentFiles) {
        m_recentFiles.removeLast();
    }

    save();
    emit recentFilesChanged(m_recentFiles);
    emit settingsChanged();
}

void SettingsModel::clearRecentFiles()
{
    if (m_recentFiles.isEmpty())
        return;

    m_recentFiles.clear();
    save();
    emit recentFilesChanged(m_recentFiles);
    emit settingsChanged();
}

std::size_t SettingsModel::cacheBudgetMB() const
{
    return m_cacheBudgetMB;
}

std::size_t SettingsModel::cacheBudgetBytes() const
{
    return m_cacheBudgetMB * 1024ULL * 1024ULL;
}

double SettingsModel::cacheBudgetGB() const
{
    return static_cast<double>(m_cacheBudgetMB) / 1024.0;
}

void SettingsModel::setCacheBudgetMB(std::size_t mb)
{
    if (mb < 512ULL)
        mb = 512ULL;

    if (m_cacheBudgetMB != mb) {
        m_cacheBudgetMB = mb;
        save();
        emit cacheBudgetChanged(m_cacheBudgetMB);
        emit settingsChanged();
    }
}

void SettingsModel::setCacheBudgetGB(double gb)
{
    setCacheBudgetMB(static_cast<std::size_t>(gb * 1024.0));
}

bool SettingsModel::oiioUnlimitedImageSize() const
{
    return m_oiioUnlimitedImageSize;
}

void SettingsModel::setOiioUnlimitedImageSize(bool unlimited)
{
    if (m_oiioUnlimitedImageSize != unlimited) {
        m_oiioUnlimitedImageSize = unlimited;
        save();
        emit oiioLimitsChanged(m_oiioUnlimitedImageSize);
        emit settingsChanged();
    }
}

double SettingsModel::defaultFps() const
{
    return m_defaultFps;
}

void SettingsModel::setDefaultFps(double fps)
{
    if (std::abs(m_defaultFps - fps) > 0.001) {
        m_defaultFps = fps;
        save();
        emit defaultFpsChanged(m_defaultFps);
        emit settingsChanged();
    }
}

QString SettingsModel::currentTheme() const
{
    return m_theme;
}

void SettingsModel::setCurrentTheme(const QString& theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        save();
        emit themeChanged(m_theme);
        emit settingsChanged();
    }
}

QByteArray SettingsModel::windowGeometry() const
{
    return m_geometry;
}

void SettingsModel::setWindowGeometry(const QByteArray& geom)
{
    m_geometry = geom;
    save();
}

QByteArray SettingsModel::dockLayoutState() const
{
    return m_dockLayout;
}

void SettingsModel::setDockLayoutState(const QByteArray& state)
{
    m_dockLayout = state;
    save();
}

bool SettingsModel::isFloatingDockEnabled() const
{
    return m_floatingDockEnabled;
}

void SettingsModel::setFloatingDockEnabled(bool enabled)
{
    if (m_floatingDockEnabled != enabled) {
        m_floatingDockEnabled = enabled;
        save();
        emit floatingDockEnabledChanged(m_floatingDockEnabled);
        emit settingsChanged();
    }
}

QString SettingsModel::appIconPreset() const
{
    return m_appIconPreset;
}

void SettingsModel::setAppIconPreset(const QString& preset)
{
    if (m_appIconPreset != preset) {
        m_appIconPreset = preset;
        save();
        emit appIconPresetChanged(m_appIconPreset, effectiveIconPath());
        emit appIconChanged(effectiveIconPath());
        emit settingsChanged();
    }
}

QString SettingsModel::customIconPath() const
{
    return m_customIconPath;
}

void SettingsModel::setCustomIconPath(const QString& path)
{
    if (m_customIconPath != path) {
        m_customIconPath = path;
        save();
        if (m_appIconPreset == QStringLiteral("custom")) {
            emit appIconPresetChanged(m_appIconPreset, effectiveIconPath());
            emit appIconChanged(effectiveIconPath());
        }
        emit settingsChanged();
    }
}

QString SettingsModel::effectiveIconPath() const
{
    if (m_appIconPreset == QStringLiteral("alt1")) {
        return QStringLiteral(":/icons/sview_alt1.ico");
    } else if (m_appIconPreset == QStringLiteral("alt2")) {
        return QStringLiteral(":/icons/sview_alt2.ico");
    } else if (m_appIconPreset == QStringLiteral("custom")) {
        if (!m_customIconPath.isEmpty() && QFile::exists(m_customIconPath)) {
            return m_customIconPath;
        }
        return QStringLiteral(":/icons/sview.ico");
    }
    return QStringLiteral(":/icons/sview.ico");
}

int SettingsModel::effectiveIconResourceIndex() const
{
    if (m_appIconPreset == QStringLiteral("alt1")) {
        return 1;
    } else if (m_appIconPreset == QStringLiteral("alt2")) {
        return 2;
    }
    return 0;
}

void SettingsModel::setAppIcon(int index, const QString& path)
{
    if (index == 1) {
        m_appIconPreset = QStringLiteral("alt1");
    } else if (index == 2) {
        m_appIconPreset = QStringLiteral("alt2");
    } else if (index == 3 || (!path.isEmpty() && !path.startsWith(QLatin1String(":/")))) {
        m_appIconPreset = QStringLiteral("custom");
        m_customIconPath = path;
    } else {
        m_appIconPreset = QStringLiteral("default");
    }
    save();
    emit appIconPresetChanged(m_appIconPreset, effectiveIconPath());
    emit appIconChanged(effectiveIconPath());
    emit settingsChanged();
}

}  // namespace sview
