cmake_minimum_required(VERSION 3.21)
project(SView VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Chemins de tes libs déjà compilées
#   -DSVIEW_DEPS_PREFIX=D:/vfx/install       (contient lib/cmake/OpenImageIO, OpenColorIO, ...)
#   -DSVIEW_VFX_BIN_DIR=D:/vfx/install/bin   (contient les DLL à copier à côté de sview.exe)
# ---------------------------------------------------------------------------
set(SVIEW_DEPS_PREFIX "" CACHE PATH "Préfixe d'installation des libs VFX (OIIO, OCIO, OpenEXR, Imath...)")
set(SVIEW_VFX_BIN_DIR "" CACHE PATH "Dossier contenant les DLL VFX à copier à côté de l'exécutable")

if(NOT SVIEW_DEPS_PREFIX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps")
    set(SVIEW_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/deps" CACHE PATH "Préfixe d'installation des libs VFX" FORCE)
endif()
if(NOT SVIEW_VFX_BIN_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/bin")
    set(SVIEW_VFX_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/bin" CACHE PATH "Dossier contenant les DLL VFX" FORCE)
endif()

if(SVIEW_DEPS_PREFIX)
    list(PREPEND CMAKE_PREFIX_PATH "${SVIEW_DEPS_PREFIX}")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Concurrent)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(OpenColorIO CONFIG REQUIRED)
find_package(qtadvanceddocking-qt6 CONFIG REQUIRED)

qt_add_executable(sview
    src/main.cpp
    res/sview.rc
    res/icons.qrc
    res/themes.qrc

    # Model (aucune dépendance à l'UI)
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/FolderContentDock.h
    src/view/FolderContentDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
)

set_target_properties(sview PROPERTIES WIN32_EXECUTABLE ON)

target_link_libraries(sview PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")

qt_add_executable(sview_tests
    tests/test_main.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)

set_target_properties(sview_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_adversarial_tests
    tests/adversarial_sequence_tests.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
)
set_target_properties(sview_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_adversarial_tests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(sview_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_sequence_cli_tests
    tests/test_sequence_cli.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
)
set_target_properties(sview_sequence_cli_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_sequence_cli_tests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(sview_sequence_cli_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_sequence_cli_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_empirical_m2_challenger_tests
    tests/test_empirical_m2_challenger.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
)
set_target_properties(sview_empirical_m2_challenger_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_empirical_m2_challenger_tests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(sview_empirical_m2_challenger_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_empirical_m2_challenger_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_empirical_m3_challenger_tests
    tests/test_empirical_m3_challenger.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)
set_target_properties(sview_empirical_m3_challenger_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_empirical_m3_challenger_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_empirical_m3_challenger_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_empirical_m3_challenger_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_empirical_m3_ux_challenger_tests
    tests/test_empirical_m3_ux_challenger.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)
set_target_properties(sview_empirical_m3_ux_challenger_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_empirical_m3_ux_challenger_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_empirical_m3_ux_challenger_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_empirical_m3_ux_challenger_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()


qt_add_executable(sview_timeline_adversarial_tests
    tests/adversarial_timeline_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
)
set_target_properties(sview_timeline_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_timeline_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_timeline_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_timeline_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_zoom_pan_persistence_adversarial_tests
    tests/adversarial_zoom_pan_persistence_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
)
set_target_properties(sview_zoom_pan_persistence_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_zoom_pan_persistence_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_zoom_pan_persistence_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_zoom_pan_persistence_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_docking_theming_tests
    tests/adversarial_docking_theming_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)
set_target_properties(sview_docking_theming_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_docking_theming_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_docking_theming_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_docking_theming_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_probe_aov_adversarial_tests
    tests/adversarial_probe_aov_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)
set_target_properties(sview_probe_aov_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_probe_aov_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_probe_aov_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_probe_aov_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_shader_wipe_adversarial_tests
    tests/adversarial_shader_wipe_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)
set_target_properties(sview_shader_wipe_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_shader_wipe_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_shader_wipe_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_shader_wipe_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_preview_spheres_360_adversarial_tests
    tests/adversarial_preview_spheres_360_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)
set_target_properties(sview_preview_spheres_360_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_preview_spheres_360_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_preview_spheres_360_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_preview_spheres_360_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_360_camera_adversarial_tests
    tests/adversarial_360_camera_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)
set_target_properties(sview_360_camera_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_360_camera_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_360_camera_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_360_camera_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_verification_v2_adversarial_tests
    tests/adversarial_verification_v2_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)
set_target_properties(sview_verification_v2_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_verification_v2_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_verification_v2_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_verification_v2_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_layout_flip_adversarial_tests
    tests/adversarial_layout_flip_tests.cpp
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)
set_target_properties(sview_layout_flip_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_layout_flip_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)
if(MSVC)
    target_compile_options(sview_layout_flip_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_layout_flip_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# Milestone 2: Automated Tests for Themes & Floating Control Dock
# ---------------------------------------------------------------------------
qt_add_executable(sview_themes_floating_dock_tests
    tests/test_themes_floating_dock.cpp

    # Model
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)

set_target_properties(sview_themes_floating_dock_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_themes_floating_dock_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_themes_floating_dock_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_themes_floating_dock_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# Milestone 3: Automated Tests for Professional VFX Features
# ---------------------------------------------------------------------------
qt_add_executable(sview_vfx_features_tests
    tests/test_vfx_features.cpp

    # Model
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)

set_target_properties(sview_vfx_features_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_vfx_features_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_vfx_features_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_vfx_features_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_m3_adversarial_tests
    tests/adversarial_m3_tests.cpp

    # Model
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)

set_target_properties(sview_m3_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_m3_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_m3_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_m3_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

# ---------------------------------------------------------------------------
# Milestone 4: Automated Tests for Dynamic Icon Management & Registry
# ---------------------------------------------------------------------------
qt_add_executable(sview_icon_registry_tests
    tests/test_icon_registry.cpp
    res/icons.qrc

    # Model
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # View
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp

    # Util
    src/util/IconConverter.h
    src/util/IconConverter.cpp
)

set_target_properties(sview_icon_registry_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_icon_registry_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
)

if(MSVC)
    target_compile_options(sview_icon_registry_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_icon_registry_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_icon_adversarial_tests
    tests/adversarial_icon_converter_tests.cpp
    res/icons.qrc

    # Model
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # View
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp

    # Util
    src/util/IconConverter.h
    src/util/IconConverter.cpp
)

set_target_properties(sview_icon_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_icon_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
)

if(MSVC)
    target_compile_options(sview_icon_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_icon_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_m4_p3_registry_adversarial_tests
    tests/adversarial_m4_icon_tests.cpp
    res/icons.qrc

    # Model
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # View
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp

    # Util
    src/util/IconConverter.h
    src/util/IconConverter.cpp
)

set_target_properties(sview_m4_p3_registry_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_m4_p3_registry_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
)

if(MSVC)
    target_compile_options(sview_m4_p3_registry_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_m4_p3_registry_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()



# ---------------------------------------------------------------------------
# Déploiement Windows : Qt + uniquement les DLL réellement nécessaires
# ---------------------------------------------------------------------------
if(WIN32)
    # Qt
    get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
    find_program(SVIEW_WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}")
    if(SVIEW_WINDEPLOYQT)
        add_custom_command(TARGET sview POST_BUILD
            COMMAND "${SVIEW_WINDEPLOYQT}" --no-translations --no-system-d3d-compiler
                    --no-opengl-sw "$<TARGET_FILE:sview>")
    endif()

    # Déploiement explicite de qoffscreen.dll pour l'exécution des tests headless
    set(_qoffscreen_dll "${_qt_bin_dir}/../plugins/platforms/qoffscreen.dll")
    if(EXISTS "${_qoffscreen_dll}")
        add_custom_command(TARGET sview POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for headless tests")
        add_custom_command(TARGET sview_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_tests")
        add_custom_command(TARGET sview_verification_v2_adversarial_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_verification_v2_adversarial_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_verification_v2_adversarial_tests")
        add_custom_command(TARGET sview_themes_floating_dock_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_themes_floating_dock_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_themes_floating_dock_tests")
        add_custom_command(TARGET sview_vfx_features_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_vfx_features_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_vfx_features_tests")
        add_custom_command(TARGET sview_m3_adversarial_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_m3_adversarial_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_m3_adversarial_tests")
        add_custom_command(TARGET sview_icon_registry_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_icon_registry_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_icon_registry_tests")
        add_custom_command(TARGET sview_icon_adversarial_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_icon_adversarial_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_icon_adversarial_tests")
        add_custom_command(TARGET sview_m4_p3_registry_adversarial_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qoffscreen_dll}" "$<TARGET_FILE_DIR:sview_m4_p3_registry_adversarial_tests>/platforms/qoffscreen.dll"
            COMMENT "Deploying qoffscreen.dll for sview_m4_p3_registry_adversarial_tests")
    endif()

    # VFX : copie des DLL de SVIEW_VFX_BIN_DIR
    if(SVIEW_VFX_BIN_DIR)
        file(GLOB _sview_all_dlls "${SVIEW_VFX_BIN_DIR}/*.dll")
        foreach(_dll IN LISTS _sview_all_dlls)
            add_custom_command(TARGET sview POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview>")
            add_custom_command(TARGET sview_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_tests>")
            add_custom_command(TARGET sview_docking_theming_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_docking_theming_tests>")
            add_custom_command(TARGET sview_verification_v2_adversarial_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_verification_v2_adversarial_tests>")
            add_custom_command(TARGET sview_themes_floating_dock_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_themes_floating_dock_tests>")
            add_custom_command(TARGET sview_vfx_features_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_vfx_features_tests>")
            add_custom_command(TARGET sview_m3_adversarial_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_m3_adversarial_tests>")
            add_custom_command(TARGET sview_icon_registry_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:sview_icon_registry_tests>")
        endforeach()
    endif()

    # Déploiement explicite de qtadvanceddocking-qt6.dll
    add_custom_command(TARGET sview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview binary folder")
    add_custom_command(TARGET sview_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview_tests>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview_tests binary folder")
    add_custom_command(TARGET sview_docking_theming_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview_docking_theming_tests>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview_docking_theming_tests binary folder")
    add_custom_command(TARGET sview_verification_v2_adversarial_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview_verification_v2_adversarial_tests>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview_verification_v2_adversarial_tests binary folder")
    add_custom_command(TARGET sview_themes_floating_dock_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview_themes_floating_dock_tests>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview_themes_floating_dock_tests binary folder")
    add_custom_command(TARGET sview_vfx_features_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview_vfx_features_tests>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview_vfx_features_tests binary folder")
    add_custom_command(TARGET sview_m3_adversarial_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:ads::qtadvanceddocking-qt6>" "$<TARGET_FILE_DIR:sview_m3_adversarial_tests>"
        COMMENT "Deploying qtadvanceddocking-qt6.dll to sview_m3_adversarial_tests binary folder")
endif()

qt_add_executable(sview_m1_stress_tests
    tests/adversarial_m1_stress_tests.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
)
set_target_properties(sview_m1_stress_tests PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(sview_m1_stress_tests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(sview_m1_stress_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_m1_stress_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_cli_autoplay_adversarial_tests
    tests/adversarial_cli_autoplay_tests.cpp

    # Model
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
)

set_target_properties(sview_cli_autoplay_adversarial_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_cli_autoplay_adversarial_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_cli_autoplay_adversarial_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_cli_autoplay_adversarial_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_m4_features_tests
    tests/test_m4_features.cpp

    # Model
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)

set_target_properties(sview_m4_features_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_m4_features_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_m4_features_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_m4_features_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

qt_add_executable(sview_empirical_m4_challenger_tests
    tests/test_empirical_m4_challenger.cpp

    # Model
    src/model/ImageData.h
    src/model/ImageLoader.h
    src/model/ImageLoader.cpp
    src/model/FolderModel.h
    src/model/FolderModel.cpp
    src/model/ImageCache.h
    src/model/ImageCache.cpp
    src/model/ColorManager.h
    src/model/ColorManager.cpp
    src/model/SequenceModel.h
    src/model/SequenceModel.cpp
    src/model/PlaybackController.h
    src/model/PlaybackController.cpp
    src/model/SettingsModel.h
    src/model/SettingsModel.cpp
    src/model/WindowsRegistryManager.h
    src/model/WindowsRegistryManager.cpp

    # ViewModel
    src/viewmodel/ViewerViewModel.h
    src/viewmodel/ViewerViewModel.cpp

    # View
    src/view/ViewerWidget.h
    src/view/ViewerWidget.cpp
    src/view/ThemeManager.h
    src/view/ThemeManager.cpp
    src/view/FloatingControlDock.h
    src/view/FloatingControlDock.cpp
    src/view/TimelineDock.h
    src/view/TimelineDock.cpp
    src/view/AnalysisDock.h
    src/view/AnalysisDock.cpp
    src/view/PixelProbeDock.h
    src/view/PixelProbeDock.cpp
    src/view/MetadataDock.h
    src/view/MetadataDock.cpp
    src/view/BatchConverterDialog.h
    src/view/BatchConverterDialog.cpp
    src/view/SettingsDialog.h
    src/view/SettingsDialog.cpp
    src/view/CustomTitleBar.h
    src/view/CustomTitleBar.cpp
    src/view/LoadingSpinnerWidget.h
    src/view/LoadingSpinnerWidget.cpp
    src/view/HistoryDock.h
    src/view/HistoryDock.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp

    # Util
    src/util/CommandLineParser.h
    src/util/CommandLineParser.cpp
    src/util/IconConverter.h
    src/util/IconConverter.cpp
    res/icons.qrc
    res/themes.qrc
)

set_target_properties(sview_empirical_m4_challenger_tests PROPERTIES WIN32_EXECUTABLE OFF)

target_link_libraries(sview_empirical_m4_challenger_tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent
    OpenImageIO::OpenImageIO
    OpenColorIO::OpenColorIO
    ads::qtadvanceddocking-qt6
)

if(MSVC)
    target_compile_options(sview_empirical_m4_challenger_tests PRIVATE /utf-8 /permissive- /W3)
    target_compile_definitions(sview_empirical_m4_challenger_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

endif() # EXISTS tests


