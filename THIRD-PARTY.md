# Third-party components

SView is built on top of open-source libraries. This file lists them, their
licenses, and where to obtain their source code, as required by those licenses.

The **LICENSES.txt** file shipped next to `sview.exe` is the runtime copy of
this information.

---

## LGPL components (dynamically linked, replaceable)

### Qt 6 — LGPL-3.0

Copyright © The Qt Company Ltd. and contributors.

SView ships the **unmodified** Qt runtime DLLs (Qt6Core, Qt6Gui, Qt6Widgets,
Qt6OpenGL, Qt6OpenGLWidgets, Qt6Svg, Qt6Network, Qt6Pdf) together with the
platform, image-format and style plugins. They are dynamically linked, so you
may replace them with any compatible Qt build.

- License text: <https://www.gnu.org/licenses/lgpl-3.0.html>
- Qt source code: <https://download.qt.io/official_releases/qt/6.8/>
- Written offer: the corresponding source is available at the URL above for
  at least three years from the date of distribution.

### Qt Advanced Docking System — LGPL-2.1

Copyright © Uwe Kindler.

Ships as a separate DLL (`qtadvanceddocking-qt6.dll`), unmodified.

- Source code: <https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System>
- License text: <https://www.gnu.org/licenses/lgpl-2.1.html>

---

## Permissive components

| Component | License | Copyright |
|---|---|---|
| OpenImageIO | Apache-2.0 | © OpenImageIO contributors |
| OpenColorIO | BSD-3-Clause | © Contributors to the OpenColorIO Project |
| OpenEXR | BSD-3-Clause | © Contributors to the OpenEXR Project |
| Imath | BSD-3-Clause | © Contributors to the OpenEXR Project |
| oneTBB | Apache-2.0 | © Intel Corporation |
| zlib | zlib | © Jean-loup Gailly and Mark Adler |
| libpng | libpng | © PNG Reference Library authors |
| libjpeg-turbo | BSD-3 / IJG / zlib | © libjpeg-turbo contributors |
| lcms2 | MIT | © Marti Maria Saguer |
| OpenJPEG | BSD-2-Clause | © OpenJPEG contributors |
| Ptex | Apache-2.0 | © Walt Disney Animation Studios |
| WebP | BSD-3-Clause | © Google Inc. |
| LibRaw | LGPL-2.1 / CDDL-1.0 | © LibRaw LLC |
| spdlog (build only) | MIT | © Gabi Melman |

OpenImageIO, OpenColorIO, OpenEXR and Imath are used through their public APIs
and are redistributed as unmodified DLLs.

---

## Notes for redistribution

- The installer includes `LICENSES.txt`, which reproduces the notices above.
- No component has been modified.
- Users are free to replace the Qt and Qt-Advanced-Docking-System DLLs with
  their own builds; the application loads them dynamically at runtime.
- The SView source code itself is licensed under the MIT License (see
  `LICENSE`).
