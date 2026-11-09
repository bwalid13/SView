#pragma once

#include <QString>
#include <QStringList>

namespace sview {

struct LaunchOptions {
    QString filePath;           // Path to image, folder, or sequence frame
    double fps = 0.0;           // Playback FPS override (0.0 if not specified / use default)
    bool autoPlay = true;       // Whether to auto-play sequence on launch (default: true)
    QString theme;              // Theme override (e.g. "usdview", "Maya", "Blender", "Nuke", "Houdini", "3dsmax", "Liquid Glass")
    bool helpRequested = false; // --help or -h or /?
    bool versionRequested = false; // --version or -v
    bool isValid = true;        // false if syntax error or unknown option
    QString errorMessage;       // Error description if !isValid
};

class CommandLineParser {
public:
    static LaunchOptions parse(const QStringList& args);
    static QString helpText();
    static QString versionText();
};

} // namespace sview
