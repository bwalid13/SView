#pragma once

#include <QString>
#include <QStringList>

namespace sview {

// Windows file association and icon manager for VFX formats (.exr, .hdr, .tx)
// Writes to HKEY_CURRENT_USER\Software\Classes without requiring UAC/Admin privileges.
// Includes transparent simulation fallback for headless and test environments.
class WindowsRegistryManager {
public:
    // File Associations Management
    static QStringList allSupportedExtensions();
    static bool registerAllFileAssociations();
    static bool unregisterAllFileAssociations();
    static bool registerDefaultApp();
    static bool registerDefaultApp(const QStringList& extensions);
    static bool unregisterDefaultApp();
    static bool unregisterDefaultApp(const QStringList& extensions);
    static bool isDefaultAppRegistered(const QString& ext);
    static QString currentExecutablePath();
    static bool simulateRegistration();
    static bool simulateRegistration(const QStringList& extensions);
    static void clearSimulatedRegistration();

    // Dynamic Application & File Icon Management
    static bool setAppIcon(const QString& icoPath, int iconIndex = 0);
    static QString appIconPath();
    static int appIconIndex();
    static QString appIconValue();
    static bool simulateAppIcon(const QString& icoPath, int iconIndex = 0);
    static void clearSimulatedIcon();
    static bool isIconSimulated();
};

}  // namespace sview
