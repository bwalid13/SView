#include "model/ImageCache.h"
#include "model/PlaybackController.h"
#include "model/SettingsModel.h"
#include "util/CommandLineParser.h"
#include "view/MainWindow.h"
#include "viewmodel/ViewerViewModel.h"

#include <QApplication>
#include <QIcon>
#include <QSurfaceFormat>
#include <QTimer>
#include <iostream>

int main(int argc, char* argv[])
{
    // Contexte OpenGL core 3.3 (requis par le shader et par QOpenGLFunctions_3_3_Core)
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SView"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // Dynamic Icon: Set initial application window icon
    const QString initialIconPath = sview::SettingsModel::instance().appIconPath();
    const QIcon appIcon(initialIconPath.isEmpty() ? QStringLiteral(":/icons/sview.ico") : initialIconPath);
    if (!appIcon.isNull()) {
        QApplication::setWindowIcon(appIcon);
    }

    // 1. Parse command-line arguments using CommandLineParser
    const sview::LaunchOptions options = sview::CommandLineParser::parse(app.arguments());

    if (options.helpRequested) {
        std::cout << sview::CommandLineParser::helpText().toStdString() << std::endl;
        return 0;
    }
    if (options.versionRequested) {
        std::cout << sview::CommandLineParser::versionText().toStdString() << std::endl;
        return 0;
    }
    if (!options.isValid) {
        std::cerr << "Error: " << options.errorMessage.toStdString() << "\n\n"
                  << sview::CommandLineParser::helpText().toStdString() << std::endl;
        return 1;
    }

    // 2. Global OIIO cache budget and limits initialization
    sview::ImageCache::setOIIOCacheBudgetMB(
        static_cast<double>(sview::SettingsModel::instance().cacheBudgetMB()));

    // 3. UI and Window Setup
    sview::ViewerViewModel viewModel;
    sview::MainWindow window(&viewModel);

    // Apply CLI theme override if specified
    if (!options.theme.isEmpty()) {
        window.applyTheme(options.theme);
    }

    // Apply CLI FPS override if specified
    if (options.fps > 0.0 && window.playbackController()) {
        window.playbackController()->setFps(options.fps);
    }

    window.resize(1280, 720);
    window.show();

    // 4. Deferred sequence/file opening via event loop singleShot
    if (!options.filePath.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, options]() {
            window.openFileOrSequence(options.filePath, options.autoPlay);
        });
    }

    return app.exec();
}
