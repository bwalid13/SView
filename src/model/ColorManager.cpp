#include "ColorManager.h"

#include <QDebug>

#include <algorithm>

namespace sview {

ColorManager::ColorManager()
{
    try {
        if (qEnvironmentVariableIsSet("OCIO"))
            m_config = OCIO::Config::CreateFromEnv();
        else
            m_config = OCIO::Config::CreateFromFile("ocio://default");

        m_display = QString::fromUtf8(m_config->getDefaultDisplay());

        // Vue par défaut : sans tone-mapping si disponible, sinon la vue par défaut du display
        const QStringList available = views();
        const QStringList preferred{"Un-tone-mapped", "Standard", "sRGB"};
        m_view = QString::fromUtf8(m_config->getDefaultView(m_display.toUtf8().constData()));
        for (const QString& p : preferred) {
            if (available.contains(p)) {
                m_view = p;
                break;
            }
        }
    } catch (const OCIO::Exception& e) {
        qWarning() << "OCIO init failed:" << e.what();
        m_config.reset();
    }
}

QStringList ColorManager::views() const
{
    QStringList out;
    if (!m_config)
        return out;
    const QByteArray display = m_display.toUtf8();
    const int n = m_config->getNumViews(display.constData());
    for (int i = 0; i < n; ++i)
        out << QString::fromUtf8(m_config->getView(display.constData(), i));
    return out;
}

void ColorManager::cycleView(int direction)
{
    const QStringList v = views();
    if (v.isEmpty())
        return;
    const int i = std::max(0, int(v.indexOf(m_view)));
    const int n = int(v.size());
    m_view = v[((i + direction) % n + n) % n];
}

QStringList ColorManager::colorSpaces() const
{
    QStringList out;
    if (!m_config) {
        out << QStringLiteral("sRGB") << QStringLiteral("Linear") << QStringLiteral("Raw");
        return out;
    }
    // 1. Display views for the current display
    out = views();
    // 2. All color spaces defined in the config
    const int n = m_config->getNumColorSpaces();
    for (int i = 0; i < n; ++i) {
        const QString name = QString::fromUtf8(m_config->getColorSpaceNameByIndex(i));
        if (!out.contains(name)) {
            out << name;
        }
    }
    if (!out.contains(QStringLiteral("sRGB"), Qt::CaseInsensitive)) {
        out.prepend(QStringLiteral("sRGB"));
    }
    if (!out.contains(QStringLiteral("Linear"), Qt::CaseInsensitive)) {
        out.append(QStringLiteral("Linear"));
    }
    return out;
}

void ColorManager::setColorSpace(const QString& name)
{
    if (name.isEmpty())
        return;
    m_view = name;
}

QString ColorManager::currentColorSpace() const
{
    if (!m_view.isEmpty())
        return m_view;
    return QStringLiteral("sRGB");
}

QString ColorManager::inputSpaceName(SourceSpace space, const QString& colorSpaceHint) const
{
    if (!m_config)
        return {};

    // 1) Espace déduit des chromaticités du fichier : on essaie les noms
    //    utilisés par les configs ACES / studio / cg avant tout le reste.
    if (!colorSpaceHint.isEmpty()) {
        QStringList hintCandidates;
        if (colorSpaceHint == QLatin1String("ACEScg"))
            hintCandidates = {"ACEScg", "ACES - ACEScg", "ACEScg - Scene Linear"};
        else if (colorSpaceHint == QLatin1String("ACES2065-1"))
            hintCandidates = {"ACES2065-1", "ACES - ACES2065-1", "ACES2065-1 - AP0"};
        else if (colorSpaceHint == QLatin1String("Rec.2020"))
            hintCandidates = {"Rec.2020", "Utility - Rec.2020 - Scene Linear",
                              "Rec.2020 - Scene Linear"};
        else if (colorSpaceHint == QLatin1String("P3-D65"))
            hintCandidates = {"P3-D65", "Utility - P3-D65 - Scene Linear",
                              "P3-D65 - Scene Linear"};
        else if (colorSpaceHint == QLatin1String("Rec.709"))
            hintCandidates = {"Linear Rec.709 (sRGB)", "scene-linear Rec.709-sRGB",
                              "lin_rec709", "Utility - Linear - sRGB"};

        for (const QString& name : hintCandidates) {
            if (m_config->getColorSpace(name.toUtf8().constData()))
                return name;
        }
    }

    // Les noms varient selon les configs (ACES 1.x, cg-config, studio-config...)
    const QStringList linearCandidates{"Linear Rec.709 (sRGB)", "scene-linear Rec.709-sRGB",
                                       "lin_rec709", "Utility - Linear - sRGB", "linear"};
    const QStringList srgbCandidates{"sRGB - Texture", "sRGB Encoded Rec.709 (sRGB)", "sRGB",
                                     "Utility - sRGB - Texture", "srgb_tx"};

    const QStringList& list =
        (space == SourceSpace::SceneLinear) ? linearCandidates : srgbCandidates;
    for (const QString& name : list) {
        if (m_config->getColorSpace(name.toUtf8().constData()))
            return name;
    }

    // Repli sur les rôles
    const char* fallbackRole = "scene_linear";
    if (space == SourceSpace::SRGB) {
        if (m_config->hasRole("texture_paint"))
            fallbackRole = "texture_paint";
        else if (m_config->hasRole("srgb_texture"))
            fallbackRole = "srgb_texture";
        else if (m_config->hasRole("matte_paint"))
            fallbackRole = "matte_paint";
    }

    if (m_config->hasRole(fallbackRole)) {
        const char* roleCs = m_config->getRoleColorSpace(fallbackRole);
        if (roleCs && *roleCs)
            return QString::fromUtf8(roleCs);
    }

    if (m_config->hasRole("default")) {
        const char* defCs = m_config->getRoleColorSpace("default");
        if (defCs && *defCs)
            return QString::fromUtf8(defCs);
    }

    if (m_config->getNumColorSpaces() > 0)
        return QString::fromUtf8(m_config->getColorSpaceNameByIndex(0));

    return {};
}

OCIO::ConstGPUProcessorRcPtr ColorManager::gpuProcessor(SourceSpace space,
                                                        const QString& colorSpaceHint) const
{
    if (!m_config)
        return nullptr;

    const QString srcName = inputSpaceName(space, colorSpaceHint);
    if (srcName.isEmpty() || m_display.isEmpty() || m_view.isEmpty())
        return nullptr;

    try {
        const QByteArray src = srcName.toUtf8();
        const QByteArray display = m_display.toUtf8();
        const QByteArray view = m_view.toUtf8();

        OCIO::DisplayViewTransformRcPtr t = OCIO::DisplayViewTransform::Create();
        t->setSrc(src.constData());
        t->setDisplay(display.constData());
        t->setView(view.constData());

        OCIO::ConstProcessorRcPtr proc = m_config->getProcessor(t);
        return proc->getDefaultGPUProcessor();
    } catch (const OCIO::Exception& e) {
        qWarning() << "OCIO processor failed:" << e.what();
        return nullptr;
    }
}

}  // namespace sview
