#include "CommandLineParser.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>

namespace sview {

namespace {

QString unquote(QString str)
{
    str = str.trimmed();
    while (str.length() >= 2 &&
           ((str.startsWith(QLatin1Char('"')) && str.endsWith(QLatin1Char('"'))) ||
            (str.startsWith(QLatin1Char('\'')) && str.endsWith(QLatin1Char('\''))))) {
        str = str.mid(1, str.length() - 2).trimmed();
    }
    return str;
}

} // namespace

LaunchOptions CommandLineParser::parse(const QStringList& rawArgs)
{
    LaunchOptions options;
    if (rawArgs.isEmpty()) {
        return options;
    }

    int startIndex = 0;
    const QString firstArg = rawArgs.first();
    if (firstArg.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) ||
        firstArg == QStringLiteral("sview") ||
        firstArg.endsWith(QStringLiteral("/sview"), Qt::CaseInsensitive) ||
        firstArg.endsWith(QStringLiteral("\\sview"), Qt::CaseInsensitive)) {
        startIndex = 1;
    }

    for (int i = startIndex; i < rawArgs.size(); ++i) {
        QString arg = rawArgs.at(i).trimmed();
        if (arg.isEmpty())
            continue;

        // Help and Version
        if (arg == QStringLiteral("-h") || arg == QStringLiteral("--help") || arg == QStringLiteral("/?") ||
            arg == QStringLiteral("/help") || arg == QStringLiteral("/h")) {
            options.helpRequested = true;
            return options;
        }
        if (arg == QStringLiteral("-v") || arg == QStringLiteral("--version") ||
            arg == QStringLiteral("/version") || arg == QStringLiteral("/v")) {
            options.versionRequested = true;
            return options;
        }

        // Auto-play control flags
        if (arg == QStringLiteral("--no-play") || arg == QStringLiteral("--pause") || arg == QStringLiteral("-p")) {
            options.autoPlay = false;
            continue;
        }
        if (arg == QStringLiteral("--play") || arg == QStringLiteral("--auto-play")) {
            options.autoPlay = true;
            continue;
        }

        // FPS override flags: --fps <val>, --fps=<val>, -r <val>, -r=<val>, --rate <val>, --rate=<val>
        if (arg.startsWith(QStringLiteral("--fps=")) || arg.startsWith(QStringLiteral("-r=")) || arg.startsWith(QStringLiteral("--rate="))) {
            const QString valStr = unquote(arg.section(QLatin1Char('='), 1));
            bool ok = false;
            const double v = valStr.toDouble(&ok);
            if (ok && std::isfinite(v) && v > 0.0) {
                options.fps = v;
            } else {
                options.isValid = false;
                options.errorMessage = QStringLiteral("Invalid FPS value: %1").arg(valStr);
            }
            continue;
        }
        if (arg == QStringLiteral("--fps") || arg == QStringLiteral("-r") || arg == QStringLiteral("--rate")) {
            if (i + 1 < rawArgs.size()) {
                ++i;
                const QString valStr = unquote(rawArgs.at(i));
                bool ok = false;
                const double v = valStr.toDouble(&ok);
                if (ok && std::isfinite(v) && v > 0.0) {
                    options.fps = v;
                } else {
                    options.isValid = false;
                    options.errorMessage = QStringLiteral("Invalid FPS value: %1").arg(valStr);
                }
            } else {
                options.isValid = false;
                options.errorMessage = QStringLiteral("Missing value for option %1").arg(arg);
            }
            continue;
        }

        // Theme override flags: --theme <name>, --theme=<name>, -t <name>, -t=<name>
        if (arg.startsWith(QStringLiteral("--theme=")) || arg.startsWith(QStringLiteral("-t="))) {
            options.theme = unquote(arg.section(QLatin1Char('='), 1));
            continue;
        }
        if (arg == QStringLiteral("--theme") || arg == QStringLiteral("-t")) {
            if (i + 1 < rawArgs.size()) {
                ++i;
                options.theme = unquote(rawArgs.at(i));
            } else {
                options.isValid = false;
                options.errorMessage = QStringLiteral("Missing value for option %1").arg(arg);
            }
            continue;
        }

        // Explicit file option: -f <path>, --file <path>, --path <path>
        if (arg.startsWith(QStringLiteral("--file=")) || arg.startsWith(QStringLiteral("--path="))) {
            options.filePath = QDir::fromNativeSeparators(unquote(arg.section(QLatin1Char('='), 1)));
            continue;
        }
        if (arg == QStringLiteral("-f") || arg == QStringLiteral("--file") || arg == QStringLiteral("--path")) {
            if (i + 1 < rawArgs.size()) {
                ++i;
                options.filePath = QDir::fromNativeSeparators(unquote(rawArgs.at(i)));
            } else {
                options.isValid = false;
                options.errorMessage = QStringLiteral("Missing value for option %1").arg(arg);
            }
            continue;
        }

        // Positional argument (file or sequence path)
        if (!arg.startsWith(QLatin1Char('-'))) {
            if (options.filePath.isEmpty()) {
                options.filePath = QDir::fromNativeSeparators(unquote(arg));
            }
            continue;
        }

        // Unknown option
        options.isValid = false;
        options.errorMessage = QStringLiteral("Unknown option: %1").arg(arg);
    }

    return options;
}

QString CommandLineParser::helpText()
{
    return QStringLiteral(
        "Usage: sview [options] [path]\n\n"
        "SView — High-Performance VFX Image & Sequence Viewer\n\n"
        "Arguments:\n"
        "  path                 File, sequence frame, or directory to open\n\n"
        "Options:\n"
        "  -f, --file <path>    Explicit path to image or sequence\n"
        "  -r, --fps <val>      Playback frame rate (e.g. 24, 30, 60)\n"
        "  -p, --no-play        Open sequence paused without auto-playing\n"
        "      --play           Explicitly enable sequence auto-play (default)\n"
        "  -t, --theme <name>   UI theme (usdview, Maya, Blender, Nuke, Houdini, 3dsmax, Liquid Glass)\n"
        "  -h, --help           Display this help text and exit\n"
        "  -v, --version        Display application version and exit\n"
    );
}

QString CommandLineParser::versionText()
{
    return QStringLiteral("SView 1.0.0 (Phase 3 VFX Suite)");
}

} // namespace sview
