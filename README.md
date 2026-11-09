# SView

A fast, focused image viewer for visual effects work — built with C++20, Qt 6,
OpenImageIO, OpenColorIO and OpenGL.

Open a plate, look at it, trust the colors. No project, no asset database, no
node graph: just the image, at full quality, immediately.

## Features

- **Reads what a VFX pipeline produces** — EXR (multi-part, AOV layers), HDR,
  DPX, Cineon, TIFF, PSD, PNG, JPEG, WebP, TGA, BMP, GIF, ICO, JPEG 2000,
  DDS, SGI, IFF, PIC, RLA, Ptex and camera RAW.
- **Color managed** — OpenColorIO display/view transforms, exposure (stops)
  and gamma, all evaluated on the GPU.
- **AOV / layer picker** — every named layer of a multi-channel EXR can be
  isolated without leaving the viewer.
- **Comparison** — A/B wipe (vertical, horizontal), onion skin, per-image
  channel isolation.
- **Pixel probe** — exact float RGBA values, luminance, hex code, normalized
  coordinates; copy to clipboard with `Ctrl+C`.
- **360° panorama** — equirectangular navigation with FOV control, plus an
  optional lookdev rig (chrome sphere, matte 18 % grey sphere, Macbeth color
  checker) that can be sized and moved to any corner.
- **Folder navigation** — thumbnails decoded in the background, no freeze on
  large directories, EXIF orientation handled.
- **Dockable UI** — six authentic DCC themes (USDView, Maya, Blender, Nuke,
  Houdini, 3ds Max), floating transport HUD, saved layouts.
- **HDR aware** — textures are uploaded as RGBA16F; images larger than
  `GL_MAX_TEXTURE_SIZE` are downscaled instead of failing silently.

## Supported formats

EXR · HDR · DPX · Cineon · TIFF · PSD · PNG · JPEG · WebP · TGA · BMP · GIF
· ICO · JPEG 2000 · DDS · SGI · IFF · PIC · RLA · Ptex · RAW

## Download

Grab the installer from the [Releases](../../releases) page:

```
Sview_Setup_v1.0.exe    Windows x64, per-user install, uninstaller included
```

The installer bundles everything needed to run (Qt runtime, VFX libraries,
MSVC runtime) and `LICENSES.txt`.

## Build from source

Requirements:

| Dependency | Version | Notes |
|---|---|---|
| Visual Studio 2022 | 17.x | MSVC x64 |
| Qt | 6.5+ | `msvc2022_64` build |
| OpenImageIO | 2.5+ | CMake config in `<prefix>/lib/cmake` |
| OpenColorIO | 2.3+ | CMake config in `<prefix>/lib/cmake` |
| Qt Advanced Docking System | 4.x | `qtadvanceddocking-qt6` |

```bat
cmake -S . -B build -G "Visual Studio 17 2022" ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" ^
  -DSVIEW_DEPS_PREFIX="C:/path/to/vfx/install" ^
  -DSVIEW_VFX_BIN_DIR="C:/path/to/vfx/install/bin"
cmake --build build --config Release
build\Release\sview.exe image.exr
```

`SVIEW_DEPS_PREFIX` is the prefix that contains `lib/cmake/OpenImageIO` and
`lib/cmake/OpenColorIO`. `SVIEW_VFX_BIN_DIR` is the folder holding the runtime
DLLs; they are copied next to the executable after each build.

> The third-party binaries are **not** part of this repository. Build them with
> the [VFX Reference Platform](https://vfxplatform.com/) or a package manager
> such as vcpkg, then point the two variables above at the result.

## Usage

```bat
sview.exe                          # empty viewer
sview.exe shot.1001.exr            # open an image
sview.exe --help                   # command line options
```

## Keyboard shortcuts

| Key | Action |
|---|---|
| `Right` / `Space` / `PageDown` | Next image |
| `Left` / `PageUp` | Previous image |
| `Home` / `End` | First / last image |
| `F` | Fit to window |
| `1` | Actual size (1:1) |
| Wheel | Zoom (anchored on the cursor) |
| Left / middle drag | Pan |
| Double-click | Toggle full screen |
| `+` / `-` | Exposure ±0.25 EV |
| `0` | Reset exposure |
| `Ctrl+E` | Reset exposure |
| `R` `G` `B` `A` `L` | Channel isolation (RGB / red / green / blue / alpha / luminance) |
| `C` | Next OCIO view |
| `Ctrl+C` | Copy the pixel under the cursor |
| `3` | 360° equirectangular mode |
| `L` (360°) | Lookdev rig |
| `R` (360°) | Reset 360° camera |
| `Ctrl+R` | Rotate 90° |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo view transform |
| `Ctrl+O` | Open image |
| `Ctrl+Shift+O` | Open folder |
| `Ctrl+W` | Close image |
| `Ctrl+Shift+E` | Quick export viewport |
| `Ctrl+M` / `Alt` | Toggle menu bar |
| `Tab` | Toggle info overlay |
| `F11` | Full screen |

## Architecture

```
src/
  model/       ImageData, ImageLoader (OIIO), FolderModel, ImageCache (async LRU),
               ColorManager (OCIO), SettingsModel, PlaybackController
  viewmodel/   ViewerViewModel — state and commands (zoom, pan, exposure, channel,
               AOV, wipe, 360° camera). No widget, no rendering.
  view/        MainWindow (docking, menus), ViewerWidget (QOpenGLWidget + OCIO shader),
               docks (analysis, probe, metadata, folder), ThemeManager (QSS generator)
  util/        CommandLineParser, IconConverter
```

The view contains no business logic: it translates keyboard/mouse into
ViewModel commands and paints what the ViewModel exposes. Images are decoded
off the UI thread and uploaded once per image change.

## License

SView is released under the [MIT License](LICENSE).

Third-party components and their licenses are listed in
[THIRD-PARTY.md](THIRD-PARTY.md). The runtime copy shipped with the application
is `LICENSES.txt`.

## Author

**Walid B.**
- Email: bwalid13@gmail.com
- GitHub: [@bwalid13](https://github.com/bwalid13)
