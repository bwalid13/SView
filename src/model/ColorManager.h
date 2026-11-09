#pragma once

#include "ImageData.h"

#include <OpenColorIO/OpenColorIO.h>

#include <QString>
#include <QStringList>

namespace OCIO = OCIO_NAMESPACE;

namespace sview {

// Enveloppe OCIO : config (variable d'env OCIO, sinon config intégrée "ocio://default"),
// display/view courants, et fabrication du processeur GPU pour un espace source donné.
class ColorManager {
public:
    ColorManager();

    bool isValid() const { return static_cast<bool>(m_config); }
    QString display() const { return m_display; }
    QString view() const { return m_view; }
    QStringList views() const;
    void cycleView(int direction);

    // Available color spaces / display views and active color space selection
    QStringList colorSpaces() const;
    void setColorSpace(const QString& name);
    QString currentColorSpace() const;

    QString inputSpaceName(SourceSpace space, const QString& colorSpaceHint = QString()) const;

    // Nul si OCIO est indisponible (la vue utilise alors un fallback sRGB)
    OCIO::ConstGPUProcessorRcPtr gpuProcessor(SourceSpace space,
                                              const QString& colorSpaceHint = QString()) const;

private:
    OCIO::ConstConfigRcPtr m_config;
    QString m_display;
    QString m_view;
};

}  // namespace sview
