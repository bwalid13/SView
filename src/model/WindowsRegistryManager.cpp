#include "WindowsRegistryManager.h"
#include "FolderModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QSet>
#include <QSettings>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace sview {

namespace {
QSet<QString> s_simulatedExtensions;
QString s_simulatedIconPath;
int s_simulatedIconIndex = 0;
bool s_iconSimulatedMode = false;
}  // namespace

QStringList WindowsRegistryManager::allSupportedExtensions()
{
    QStringList exts;
    for (const QString& e : FolderModel::supportedExtensions()) {
        QString norm = e.trimmed().toLower();
        if (!norm.startsWith('.')) {
            norm.prepend('.');
        }
        if (!exts.contains(norm)) {
            exts.append(norm);
        }
    }
    return exts;
}

bool WindowsRegistryManager::registerAllFileAssociations()
{
    return registerDefaultApp(allSupportedExtensions());
}

bool WindowsRegistryManager::unregisterAllFileAssociations()
{
    return unregisterDefaultApp(allSupportedExtensions());
}

bool WindowsRegistryManager::registerDefaultApp()
{
    return registerAllFileAssociations();
}

bool WindowsRegistryManager::unregisterDefaultApp()
{
    return unregisterAllFileAssociations();
}

QString WindowsRegistryManager::currentExecutablePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

bool WindowsRegistryManager::simulateRegistration()
{
    return simulateRegistration(allSupportedExtensions());
}

bool WindowsRegistryManager::simulateRegistration(const QStringList& extensions)
{
    for (const QString& ext : extensions) {
        QString norm = ext.trimmed().toLower();
        if (!norm.startsWith('.')) {
            norm.prepend('.');
        }
        s_simulatedExtensions.insert(norm);
    }
    return true;
}

void WindowsRegistryManager::clearSimulatedRegistration()
{
    s_simulatedExtensions.clear();
}

bool WindowsRegistryManager::simulateAppIcon(const QString& icoPath, int iconIndex)
{
    s_simulatedIconPath = icoPath;
    s_simulatedIconIndex = iconIndex;
    s_iconSimulatedMode = true;
    return true;
}

void WindowsRegistryManager::clearSimulatedIcon()
{
    s_simulatedIconPath.clear();
    s_simulatedIconIndex = 0;
    s_iconSimulatedMode = false;
}

QString WindowsRegistryManager::appIconPath()
{
    return s_simulatedIconPath;
}

int WindowsRegistryManager::appIconIndex()
{
    return s_simulatedIconIndex;
}

bool WindowsRegistryManager::isIconSimulated()
{
    return s_iconSimulatedMode;
}

QString WindowsRegistryManager::appIconValue()
{
    if (s_simulatedIconPath.isEmpty()) {
        return QString();
    }
    if (s_simulatedIconPath.startsWith(QLatin1String(":/"))) {
        return QStringLiteral("\"%1\",%2").arg(currentExecutablePath()).arg(s_simulatedIconIndex);
    }
    if (s_simulatedIconPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        return QStringLiteral("\"%1\",%2").arg(QDir::toNativeSeparators(s_simulatedIconPath)).arg(s_simulatedIconIndex);
    }
    return QDir::toNativeSeparators(s_simulatedIconPath);
}

bool WindowsRegistryManager::setAppIcon(const QString& icoPath, int iconIndex)
{
    s_simulatedIconPath = icoPath;
    s_simulatedIconIndex = iconIndex;

#ifdef _WIN32
    QString regVal;
    if (icoPath.startsWith(QLatin1String(":/"))) {
        const QString exePath = currentExecutablePath();
        regVal = QStringLiteral("\"%1\",%2").arg(exePath).arg(iconIndex);
    } else if (icoPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        regVal = QStringLiteral("\"%1\",%2").arg(QDir::toNativeSeparators(icoPath)).arg(iconIndex);
    } else {
        regVal = QDir::toNativeSeparators(icoPath);
    }

    // 1. Create or open ProgID DefaultIcon under HKCU\Software\Classes\SView.VFX\DefaultIcon
    HKEY hKeyIcon = nullptr;
    const std::wstring iconKeyPath = L"Software\\Classes\\SView.VFX\\DefaultIcon";
    LONG res = RegCreateKeyExW(HKEY_CURRENT_USER, iconKeyPath.c_str(), 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKeyIcon, nullptr);
    if (res != ERROR_SUCCESS) {
        s_iconSimulatedMode = true;
        qWarning("Registry DefaultIcon write restricted (error %ld); simulating gracefully.", res);
        return true;
    }

    const std::wstring valW = regVal.toStdWString();
    res = RegSetValueExW(hKeyIcon, nullptr, 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(valW.c_str()),
                         static_cast<DWORD>((valW.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKeyIcon);
    if (res != ERROR_SUCCESS) {
        s_iconSimulatedMode = true;
        qWarning("Failed to set DefaultIcon value in registry; simulating gracefully.");
        return true;
    }

    // 2. Flush shell icon cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#else
    s_iconSimulatedMode = true;
#endif

    return true;
}

bool WindowsRegistryManager::registerDefaultApp(const QStringList& extensions)
{
    // Always update internal state for testing and headless execution
    simulateRegistration(extensions);

#ifdef _WIN32
    const QString exePath = currentExecutablePath();
    if (exePath.isEmpty()) {
        return true;
    }

    const QString progId = QStringLiteral("SView.VFX");
    const QString progDesc = QStringLiteral("VFX Image File");
    const QString openCmd = QStringLiteral("\"%1\" \"%2\"").arg(exePath, QStringLiteral("%1"));

    // 1. Create ProgID under HKCU\Software\Classes\SView.VFX
    HKEY hKeyProg = nullptr;
    const std::wstring progKeyPath = L"Software\\Classes\\" + progId.toStdWString();
    if (RegCreateKeyExW(HKEY_CURRENT_USER, progKeyPath.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKeyProg, nullptr) == ERROR_SUCCESS) {
        const std::wstring descW = progDesc.toStdWString();
        RegSetValueExW(hKeyProg, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(descW.c_str()),
                       static_cast<DWORD>((descW.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKeyProg);
    }

    // 2. Create shell\open\command under HKCU\Software\Classes\SView.VFX\shell\open\command
    HKEY hKeyCmd = nullptr;
    const std::wstring cmdKeyPath = progKeyPath + L"\\shell\\open\\command";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, cmdKeyPath.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKeyCmd, nullptr) == ERROR_SUCCESS) {
        const std::wstring cmdW = openCmd.toStdWString();
        RegSetValueExW(hKeyCmd, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmdW.c_str()),
                       static_cast<DWORD>((cmdW.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKeyCmd);
    }

    // 3. Associate each extension to the ProgID under HKCU\Software\Classes\<ext>
    for (const QString& ext : extensions) {
        QString norm = ext.trimmed().toLower();
        if (!norm.startsWith('.')) {
            norm.prepend('.');
        }

        const std::wstring extKeyPath = L"Software\\Classes\\" + norm.toStdWString();
        HKEY hKeyExt = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, extKeyPath.c_str(), 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKeyExt, nullptr) == ERROR_SUCCESS) {
            const std::wstring progIdW = progId.toStdWString();
            RegSetValueExW(hKeyExt, nullptr, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(progIdW.c_str()),
                           static_cast<DWORD>((progIdW.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKeyExt);
        }
    }

    // If an app icon was set, write DefaultIcon for ProgID
    if (!s_simulatedIconPath.isEmpty()) {
        setAppIcon(s_simulatedIconPath, s_simulatedIconIndex);
    }

    // 4. Notify Windows Shell of file association updates
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif

    return true;
}

bool WindowsRegistryManager::unregisterDefaultApp(const QStringList& extensions)
{
    clearSimulatedRegistration();

#ifdef _WIN32
    for (const QString& ext : extensions) {
        QString norm = ext.trimmed().toLower();
        if (!norm.startsWith('.')) {
            norm.prepend('.');
        }
        const std::wstring extKeyPath = L"Software\\Classes\\" + norm.toStdWString();
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, extKeyPath.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            wchar_t buf[256] = {0};
            DWORD bufSize = sizeof(buf);
            DWORD type = REG_SZ;
            if (RegQueryValueExW(hKey, nullptr, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &bufSize) == ERROR_SUCCESS) {
                if (QString::fromWCharArray(buf) == QStringLiteral("SView.VFX")) {
                    RegDeleteValueW(hKey, nullptr);
                    RegCloseKey(hKey);
                    RegDeleteKeyW(HKEY_CURRENT_USER, (extKeyPath + L"\\DefaultIcon").c_str());
                    RegDeleteKeyW(HKEY_CURRENT_USER, extKeyPath.c_str());
                    continue;
                }
            }
            RegCloseKey(hKey);
        }
    }

    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\SView.VFX\\DefaultIcon");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\SView.VFX\\shell\\open\\command");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\SView.VFX\\shell\\open");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\SView.VFX\\shell");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\SView.VFX");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#endif

    return true;
}

bool WindowsRegistryManager::isDefaultAppRegistered(const QString& ext)
{
    QString norm = ext.trimmed().toLower();
    if (!norm.startsWith('.')) {
        norm.prepend('.');
    }

    if (s_simulatedExtensions.contains(norm)) {
        return true;
    }

#ifdef _WIN32
    const std::wstring extKeyPath = L"Software\\Classes\\" + norm.toStdWString();
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, extKeyPath.c_str(), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256] = {0};
        DWORD bufSize = sizeof(buf);
        DWORD type = REG_SZ;
        if (RegQueryValueExW(hKey, nullptr, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &bufSize) == ERROR_SUCCESS) {
            const QString val = QString::fromWCharArray(buf);
            RegCloseKey(hKey);
            if (val == QStringLiteral("SView.VFX")) {
                return true;
            }
        } else {
            RegCloseKey(hKey);
        }
    }
#endif

    return false;
}

}  // namespace sview
