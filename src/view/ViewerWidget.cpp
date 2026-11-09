#include "ViewerWidget.h"
#include "FloatingControlDock.h"
#include "LoadingSpinnerWidget.h"

#include "../model/FolderModel.h"
#include "../model/PlaybackController.h"
#include "../viewmodel/ViewerViewModel.h"

#include <QClipboard>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QLabel>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QToolTip>
#include <QUrl>
#include <QVector2D>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace sview {

namespace {

// Triangle plein écran généré à partir de gl_VertexID (pas de VBO)
const char* const kVertexShader = R"GLSL(#version 330 core
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* const kFragmentHeader = R"GLSL(#version 330 core
uniform sampler2D uImage;
uniform sampler2D uImageB;
uniform vec2  uViewport;     // pixels device
uniform vec2  uImageSize;    // pixels image
uniform vec2  uPan;          // pixels device
uniform float uZoom;         // pixels device par pixel image
uniform float uExposure;     // multiplicateur lineaire (2^EV)
uniform float uGamma;        // display gamma (default 1.0)
uniform int   uChannel;      // 0 RGB, 1 R, 2 G, 3 B, 4 A, 5 Luma
uniform int   uWipeEnabled;  // 0 = Single, 1 = A/B Wipe
uniform int   uWipeMode;     // 0 = Vertical, 1 = Horizontal
uniform float uWipePos;      // 0.0 to 1.0
uniform int   uCheckerboard; // 0 = solid dark, 1 = checkerboard
uniform int   uMode360;      // 0 = 2D Planar, 1 = 360 Equirectangular
uniform float uYaw;          // radians [-pi, pi]
uniform float uPitch;        // radians [-pi/2, pi/2]
uniform float uFov;          // radians [15 deg, 120 deg]
uniform int   uShowSpheres;  // 0 = Off, 1 = Chrome & Grey spheres
uniform vec2  uRigAnchor;    // coin d'ancrage : (0,0)=bas-gauche, (1,1)=haut-droite
uniform int   uShowChart;    // 1 = color checker Macbeth affiché
uniform int   uFlipH;        // 0 = Normal, 1 = Flipped Horizontal
uniform int   uFlipV;        // 0 = Normal, 1 = Flipped Vertical
uniform int   uRotation;     // 0, 90, 180, 270
out vec4 fragColor;

const float PI = 3.14159265358979323846;
const float TWO_PI = 6.28318530717958647692;

vec3 rotateToWorld(vec3 v, float yaw, float pitch) {
    float cp = cos(pitch);
    float sp = sin(pitch);
    vec3 v1 = vec3(v.x, v.y * cp + v.z * sp, -v.y * sp + v.z * cp);
    float cy = cos(yaw);
    float sy = sin(yaw);
    return vec3(v1.x * cy + v1.z * sy, v1.y, -v1.x * sy + v1.z * cy);
}

vec2 dirToEquirect(vec3 d) {
    float u = 0.5 + atan(d.x, d.z) / TWO_PI;
    float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / PI;
    return vec2(fract(u), clamp(v, 0.0, 1.0));
}

// Sample equirectangular environment texture using explicit screen-space derivatives
// to eliminate mipmap boundary seam artifact at u = 0.0 / u = 1.0
vec4 sampleEquirect(sampler2D tex, vec2 uv) {
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    if (dx.x > 0.5)  dx.x -= 1.0;
    else if (dx.x < -0.5) dx.x += 1.0;
    if (dy.x > 0.5)  dy.x -= 1.0;
    else if (dy.x < -0.5) dy.x += 1.0;
    return textureGrad(tex, uv, dx, dy);
}

// 32-Sample Cosine-Weighted Fibonacci Hemisphere Irradiance
vec3 sampleHemisphereFibonacci(vec3 N, sampler2D tex, float maxLod, int flipH, int flipV) {
    vec3 up = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);

    const int SAMPLES = 32;
    const float GOLDEN_ANGLE = 2.39996323; // radians: 2 * PI * (1 - 1/phi)
    float lod = clamp(maxLod - 3.5, 2.0, 7.0);

    vec3 accum = vec3(0.0);
    for (int i = 0; i < SAMPLES; ++i) {
        float u = (float(i) + 0.5) / float(SAMPLES);
        float cosTheta = sqrt(1.0 - u); // cosine-weighted
        float sinTheta = sqrt(u);
        float phi = float(i) * GOLDEN_ANGLE;

        vec3 dirLocal = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        vec3 tapDir = normalize(T * dirLocal.x + B * dirLocal.y + N * dirLocal.z);

        vec2 uv = dirToEquirect(tapDir);
        if (flipH == 1) uv.x = 1.0 - uv.x;
        if (flipV == 1) uv.y = 1.0 - uv.y;

        accum += textureLod(tex, uv, lod).rgb;
    }
    return accum / float(SAMPLES);
}

// Macbeth ColorChecker 24 Standard Linear Albedos (BabelColor / ISO 17321-1)
vec3 macbethAlbedo(int index) {
    const vec3 c[24] = vec3[24](
        // Row 1: Natural Colors
        vec3(0.174, 0.084, 0.058), // 0: Dark Skin
        vec3(0.534, 0.306, 0.231), // 1: Light Skin
        vec3(0.122, 0.198, 0.339), // 2: Blue Sky
        vec3(0.097, 0.155, 0.057), // 3: Foliage
        vec3(0.237, 0.218, 0.435), // 4: Blue Flower
        vec3(0.137, 0.505, 0.404), // 5: Bluish Green
        // Row 2: Misc Colors
        vec3(0.669, 0.211, 0.026), // 6: Orange
        vec3(0.079, 0.106, 0.378), // 7: Purplish Blue
        vec3(0.528, 0.103, 0.126), // 8: Moderate Red
        vec3(0.113, 0.045, 0.155), // 9: Purple
        vec3(0.339, 0.499, 0.052), // 10: Yellow Green
        vec3(0.742, 0.367, 0.028), // 11: Orange Yellow
        // Row 3: Primary / Secondary Colors
        vec3(0.038, 0.047, 0.306), // 12: Blue
        vec3(0.061, 0.297, 0.066), // 13: Green
        vec3(0.421, 0.037, 0.046), // 14: Red
        vec3(0.796, 0.569, 0.015), // 15: Yellow
        vec3(0.493, 0.094, 0.302), // 16: Magenta
        vec3(0.003, 0.237, 0.360), // 17: Cyan
        // Row 4: Grayscale Patches
        vec3(0.899, 0.899, 0.891), // 18: White (Neutral 9.5)
        vec3(0.578, 0.578, 0.578), // 19: Neutral 8
        vec3(0.354, 0.354, 0.354), // 20: Neutral 6.5
        vec3(0.180, 0.180, 0.180), // 21: Neutral 5 (18% Grey)
        vec3(0.092, 0.092, 0.092), // 22: Neutral 3.5
        vec3(0.033, 0.033, 0.033)  // 23: Black (Neutral 2)
    );
    return c[clamp(index, 0, 23)];
}

vec3 getDiffuseIrradiance(vec3 N, sampler2D tex) {
    vec3 up = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);

    vec3 accum = textureLod(tex, dirToEquirect(N), 6.5).rgb * 2.0;
    float totalWeight = 2.0;

    for (int i = 0; i < 8; ++i) {
        float phi = float(i) * (TWO_PI / 8.0);
        vec3 tapDir = normalize(N * 0.75 + (T * cos(phi) + B * sin(phi)) * 0.66);
        accum += textureLod(tex, dirToEquirect(tapDir), 6.0).rgb;
        totalWeight += 1.0;
    }
    return accum / totalWeight;
}
)GLSL";

// Fallback si OCIO est indisponible : suppose une entrée linéaire -> sRGB
const char* const kFallbackOcio = R"GLSL(
vec4 OCIODisplay(vec4 c) {
    vec3 l  = max(c.rgb, vec3(0.0));
    vec3 lo = l * 12.92;
    vec3 hi = 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055;
    vec3 s  = step(l, vec3(0.0031308));
    return vec4(mix(hi, lo, s), c.a);
}
)GLSL";

const char* const kFragmentMain = R"GLSL(
void main() {
    vec2 frag = vec2(gl_FragCoord.x, uViewport.y - gl_FragCoord.y);   // origine en haut a gauche

    // 1. Divider line across entire viewport (modes 0 and 1 only; mode 2 is Onion Skin)
    float splitCoord = (uWipeMode == 0) ? frag.x : frag.y;
    float splitTarget = (uWipeMode == 0) ? (uWipePos * uViewport.x) : (uWipePos * uViewport.y);
    if (uWipeEnabled == 1 && uWipeMode != 2) {
        if (abs(splitCoord - splitTarget) <= 0.75) {
            fragColor = vec4(0.95, 0.95, 0.95, 1.0);
            return;
        }
    }

    bool isSideB = (uWipeEnabled == 1) && (uWipeMode != 2) && (splitCoord > splitTarget);
    vec4 c = vec4(0.0);

    if (uMode360 == 1) {
        float aspect = uViewport.x / max(uViewport.y, 1.0);
        float tanHalfFov = tan(uFov * 0.5);
        vec2 ndc = (frag / uViewport - 0.5) * 2.0;
        vec3 rayCam = normalize(vec3(ndc.x * aspect * tanHalfFov, -ndc.y * tanHalfFov, 1.0));
        vec3 rayWorld = rotateToWorld(rayCam, uYaw, uPitch);

        vec2 envUV = dirToEquirect(rayWorld);
        if (uFlipH == 1) envUV.x = 1.0 - envUV.x;
        if (uFlipV == 1) envUV.y = 1.0 - envUV.y;
        if (uWipeEnabled == 1 && uWipeMode == 2) {
            vec4 cA = sampleEquirect(uImage, envUV);
            vec4 cB = sampleEquirect(uImageB, envUV);
            c = mix(cA, cB, clamp(uWipePos, 0.0, 1.0));
        } else {
            c = isSideB ? sampleEquirect(uImageB, envUV) : sampleEquirect(uImage, envUV);
        }

        if (uShowSpheres == 1) {
            float z0 = 2.0;

            // Le rig ne doit jamais sortir de l'écran : on borne son échelle par
            // l'espace réellement disponible depuis le coin d'ancrage.
            const float kRigSpacing = 0.36;   // écart chrome <-> grise (fixe)
            float rigWidth  = 0.16 + kRigSpacing + 0.08 + 0.06 + 0.26;
            float rigHeight = 0.18 + 0.08;
            float maxScale = min(aspect / max(rigWidth, 0.01), 1.0 / max(rigHeight, 0.01)) * 0.98;
            const float kRigSize = 1.2;   // taille fixe, un peu plus grande que l'originale
            float k = min(kRigSize, maxScale);

            // s0 = échelle du BORD de l'écran (ne dépend pas de la taille du rig).
            // s  = échelle du rig, utilisée uniquement pour ses dimensions.
            // Confondre les deux décalait l'ancre hors de l'écran dès que k > 1.
            float s0 = tanHalfFov * z0;
            float s  = s0 * k;
            float R = 0.08 * s;
            float maxLod = log2(max(max(uImageSize.x, uImageSize.y), 1.0));

            // Coin d'ancrage : on part du bord réel de l'écran, et on rentre.
            vec2 inward = vec2(uRigAnchor.x < 0.5 ? 1.0 : -1.0,
                               uRigAnchor.y < 0.5 ? 1.0 : -1.0);
            float edgeX = mix(-aspect, aspect, uRigAnchor.x) * s0;
            float edgeY = mix(-1.0, 1.0, uRigAnchor.y) * s0;

            // Sphere Centers in Camera Space (grey then chrome, toward the interior)
            vec3 cGrey   = vec3(edgeX + inward.x * 0.16 * s, edgeY + inward.y * 0.18 * s, z0);
            vec3 cChrome = cGrey + vec3(inward.x * kRigSpacing * s, 0.0, 0.0);

            // 1. Chrome Sphere (Mirror Specular)
            float bCh = dot(rayCam, cChrome);
            float dPerpChSq = dot(cChrome, cChrome) - bCh * bCh;
            float deltaCh = R * R - dPerpChSq;
            float sCh = R - sqrt(max(dPerpChSq, 0.0));
            float fwCh = max(fwidth(sCh), 1e-4);

            if (deltaCh >= 0.0 || sCh > -fwCh) {
                float t = bCh - sqrt(max(deltaCh, 0.0));
                vec3 pCam = t * rayCam;
                vec3 nCam = (pCam - cChrome) / R;
                vec3 nWorld = rotateToWorld(nCam, uYaw, uPitch);
                vec3 rWorld = reflect(rayWorld, nWorld);
                vec2 reflUV = dirToEquirect(rWorld);
                if (uFlipH == 1) reflUV.x = 1.0 - reflUV.x;
                if (uFlipV == 1) reflUV.y = 1.0 - reflUV.y;
                vec4 chromeColor = isSideB ? sampleEquirect(uImageB, reflUV) : sampleEquirect(uImage, reflUV);

                float edgeAlpha = clamp(0.5 + sCh / fwCh, 0.0, 1.0);
                c = mix(c, chromeColor, edgeAlpha);
            }

            // 2. Grey Sphere (18% Diffuse + Semi-Matte Specular Sheen)
            float bGr = dot(rayCam, cGrey);
            float dPerpGrSq = dot(cGrey, cGrey) - bGr * bGr;
            float deltaGr = R * R - dPerpGrSq;
            float sGr = R - sqrt(max(dPerpGrSq, 0.0));
            float fwGr = max(fwidth(sGr), 1e-4);

            if (deltaGr >= 0.0 || sGr > -fwGr) {
                float t = bGr - sqrt(max(deltaGr, 0.0));
                vec3 pCam = t * rayCam;
                vec3 nCam = (pCam - cGrey) / R;
                vec3 nWorld = rotateToWorld(nCam, uYaw, uPitch);

                // Diffuse irradiance: 32-tap Fibonacci hemisphere
                vec3 irr = isSideB
                    ? sampleHemisphereFibonacci(nWorld, uImageB, maxLod, uFlipH, uFlipV)
                    : sampleHemisphereFibonacci(nWorld, uImage, maxLod, uFlipH, uFlipV);

                // Balle grise 18% : Lambert pur (mate), sans aucun spéculaire.
                vec3 greyRgb = irr * 0.18;
                vec4 greyColor = vec4(greyRgb, 1.0);

                float edgeAlpha = clamp(0.5 + sGr / fwGr, 0.0, 1.0);
                c = mix(c, greyColor, edgeAlpha);
            }

            // 3. Macbeth Color Checker Chart (6 cols x 4 rows)
            // Placé à la suite de la sphère chrome, aligné sur l'axe des sphères.
            if (uShowChart == 1) {
            float chartW  = 0.26 * s;
            float chartH  = 0.17 * s;
            float chartGap = 0.06 * s;
            float chartCx = cChrome.x + inward.x * (R + chartGap + chartW * 0.5);
            float chartCy = cGrey.y;
            float chartX0 = chartCx - chartW * 0.5;
            float chartX1 = chartCx + chartW * 0.5;
            float chartY0 = chartCy - chartH * 0.5;
            float chartY1 = chartCy + chartH * 0.5;

            // Hit point of camera ray on chart plane Z = z0
            float px = (rayCam.x / rayCam.z) * z0;
            float py = (rayCam.y / rayCam.z) * z0;

            // Signed distance to chart rectangle edges (positive outside)
            float sBoxX = max(chartX0 - px, px - chartX1);
            float sBoxY = max(chartY0 - py, py - chartY1);
            float sBox = max(sBoxX, sBoxY);
            float fwBox = max(fwidth(sBox), 1e-4);

            if (sBox <= fwBox) {
                // UV across chart: u in [0, 1] left->right, v in [0, 1] top->bottom
                float uChart = clamp((px - chartX0) / (chartX1 - chartX0), 0.0, 1.0);
                float vChart = clamp((chartY1 - py) / (chartY1 - chartY0), 0.0, 1.0);

                // Ambient HDRI irradiance arriving at chart surface (facing camera)
                vec3 nChartCam = vec3(0.0, 0.0, -1.0);
                vec3 nChartWorld = rotateToWorld(nChartCam, uYaw, uPitch);
                vec3 chartIrr = isSideB
                    ? sampleHemisphereFibonacci(nChartWorld, uImageB, maxLod, uFlipH, uFlipV)
                    : sampleHemisphereFibonacci(nChartWorld, uImage, maxLod, uFlipH, uFlipV);

                // Chart frame and patch grid borders
                float borderOuter = 0.035;
                float borderInner = 0.04;
                vec3 chartColorRgb = chartIrr * vec3(0.015); // dark matte bezel

                if (uChart > borderOuter && uChart < (1.0 - borderOuter) &&
                    vChart > borderOuter && vChart < (1.0 - borderOuter)) {
                    float uInner = (uChart - borderOuter) / (1.0 - 2.0 * borderOuter);
                    float vInner = (vChart - borderOuter) / (1.0 - 2.0 * borderOuter);

                    float colF = uInner * 6.0;
                    float rowF = vInner * 4.0;
                    int col = clamp(int(floor(colF)), 0, 5);
                    int row = clamp(int(floor(rowF)), 0, 3);

                    float uCell = fract(colF);
                    float vCell = fract(rowF);

                    if (uCell > borderInner && uCell < (1.0 - borderInner) &&
                        vCell > borderInner && vCell < (1.0 - borderInner)) {
                        int patchIdx = row * 6 + col;
                        vec3 albedo = macbethAlbedo(patchIdx);
                        chartColorRgb = chartIrr * albedo;
                    }
                }

                float edgeAlpha = clamp(0.5 - sBox / fwBox, 0.0, 1.0);
                c = mix(c, vec4(chartColorRgb, 1.0), edgeAlpha);
            }
            }
        }
    } else {
        vec2 v = (frag - uViewport * 0.5 - uPan) / uZoom;
        if (uFlipH == 1) v.x = -v.x;
        if (uFlipV == 1) v.y = -v.y;

        vec2 dispSize = (uRotation == 90 || uRotation == 270) ? vec2(uImageSize.y, uImageSize.x) : uImageSize;
        if (abs(v.x) > dispSize.x * 0.5 || abs(v.y) > dispSize.y * 0.5) {
            fragColor = vec4(0.11, 0.11, 0.11, 1.0);
            return;
        }

        vec2 d;
        if (uRotation == 90) {
            d = vec2(v.y, -v.x);
        } else if (uRotation == 180) {
            d = vec2(-v.x, -v.y);
        } else if (uRotation == 270) {
            d = vec2(-v.y, v.x);
        } else {
            d = v;
        }

        vec2 uv = (d + uImageSize * 0.5) / uImageSize;
        if (uWipeEnabled == 1 && uWipeMode == 2) {
            vec4 cA = texture(uImage, uv);
            vec4 cB = texture(uImageB, uv);
            c = mix(cA, cB, clamp(uWipePos, 0.0, 1.0));
        } else {
            c = isSideB ? texture(uImageB, uv) : texture(uImage, uv);
        }
    }

    if (uChannel == 4) {                       // alpha : donnee brute, pas de transformation
        fragColor = vec4(vec3(clamp(c.a, 0.0, 1.0)), 1.0);
        return;
    }

    vec3 rgb = c.rgb;
    if (uChannel == 1)      rgb = vec3(c.r);
    else if (uChannel == 2) rgb = vec3(c.g);
    else if (uChannel == 3) rgb = vec3(c.b);
    else if (uChannel == 5) rgb = vec3(dot(c.rgb, vec3(0.2126, 0.7152, 0.0722)));

    // Pre-OCIO linear exposure gain
    vec4 d = OCIODisplay(vec4(rgb * uExposure, 1.0));

    // Post-OCIO display gamma
    d.rgb = pow(max(d.rgb, vec3(0.0)), vec3(1.0 / max(uGamma, 0.0001)));

    if (uChannel == 0) {
        vec3 bg;
        if (uCheckerboard == 1) {
            vec2 cell = floor(frag / 12.0);
            bg = mix(vec3(0.18), vec3(0.26), mod(cell.x + cell.y, 2.0));
        } else {
            bg = vec3(0.11);
        }
        d.rgb = mix(bg, d.rgb, clamp(c.a, 0.0, 1.0));
    }
    fragColor = vec4(d.rgb, 1.0);
}
)GLSL";

// Réduction box-filter d'une image RGBA float.
// Sert uniquement de repli quand l'image dépasse GL_MAX_TEXTURE_SIZE : sans
// ça, glTexImage2D échoue et l'écran reste noir sans aucun message.
std::vector<float> downscaleRgba(const std::vector<float>& src, int srcW, int srcH, int dstW, int dstH)
{
    std::vector<float> dst(size_t(dstW) * size_t(dstH) * 4, 0.0f);
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return dst;

    const double xRatio = double(srcW) / double(dstW);
    const double yRatio = double(srcH) / double(dstH);

    for (int y = 0; y < dstH; ++y) {
        const int y0 = std::min(srcH - 1, int(double(y) * yRatio));
        const int y1 = std::min(srcH, std::max(y0 + 1, int(double(y + 1) * yRatio)));
        for (int x = 0; x < dstW; ++x) {
            const int x0 = std::min(srcW - 1, int(double(x) * xRatio));
            const int x1 = std::min(srcW, std::max(x0 + 1, int(double(x + 1) * xRatio)));

            double acc[4] = {0.0, 0.0, 0.0, 0.0};
            int count = 0;
            for (int sy = y0; sy < y1; ++sy) {
                for (int sx = x0; sx < x1; ++sx) {
                    const size_t i = (size_t(sy) * size_t(srcW) + size_t(sx)) * 4;
                    acc[0] += src[i];
                    acc[1] += src[i + 1];
                    acc[2] += src[i + 2];
                    acc[3] += src[i + 3];
                    ++count;
                }
            }

            float* o = &dst[(size_t(y) * size_t(dstW) + size_t(x)) * 4];
            if (count > 0) {
                o[0] = float(acc[0] / count);
                o[1] = float(acc[1] / count);
                o[2] = float(acc[2] / count);
                o[3] = float(acc[3] / count);
            }
        }
    }
    return dst;
}

}  // namespace

ViewerWidget::ViewerWidget(ViewerViewModel* vm, QWidget* parent)
    : QOpenGLWidget(parent), m_vm(vm)
{
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(250, 200);

    connect(m_vm, &ViewerViewModel::imageChanged, this, &ViewerWidget::onImageChanged);
    connect(m_vm, &ViewerViewModel::imageBChanged, this, &ViewerWidget::onImageBChanged);
    connect(m_vm, &ViewerViewModel::transformChanged, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::displayChanged, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::wipeChanged, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::mode360Changed, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::camera360Changed, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::previewSpheresChanged, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::rigChanged, this, qOverload<>(&ViewerWidget::update));
    connect(m_vm, &ViewerViewModel::rotationChanged, this, qOverload<>(&ViewerWidget::update));

    // Feature 3: Minimalist Viewport OSD Pill
    m_osdLabel = new QLabel(this);
    m_osdLabel->setObjectName(QStringLiteral("viewerOsdLabel"));
    m_osdLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_osdLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e0e0e0; background: rgba(18, 18, 18, 190);"
        " border: 1px solid rgba(255, 255, 255, 30); border-radius: 4px;"
        " padding: 4px 10px; font-family: Consolas, 'DejaVu Sans Mono', monospace; font-size: 11px; }"));
    m_osdLabel->move(12, 12);
    updateOsd();

    connect(m_vm, &ViewerViewModel::imageChanged, this, &ViewerWidget::updateOsd);
    connect(m_vm, &ViewerViewModel::displayChanged, this, &ViewerWidget::updateOsd);
    connect(m_vm, &ViewerViewModel::overlayChanged, this, &ViewerWidget::updateOsd);

    m_currentTransform = { m_vm ? m_vm->flipH() : false,
                           m_vm ? m_vm->flipV() : false,
                           m_vm ? m_vm->rotation() : 0 };
    connect(m_vm, &ViewerViewModel::displayChanged, this, &ViewerWidget::onTransformStateMaybeChanged);
    connect(m_vm, &ViewerViewModel::rotationChanged, this, &ViewerWidget::onTransformStateMaybeChanged);
    connect(m_vm, &ViewerViewModel::imageChanged, this, &ViewerWidget::resetTransformHistory);

    // Animated Loading Spinner overlay
    m_spinner = new LoadingSpinnerWidget(this);
    connect(m_vm, &ViewerViewModel::loadingChanged, m_spinner, &LoadingSpinnerWidget::setLoading);
}

ViewerWidget::~ViewerWidget()
{
    if (m_glReady) {
        makeCurrent();
        if (m_tex)
            glDeleteTextures(1, &m_tex);
        if (m_texB)
            glDeleteTextures(1, &m_texB);
        if (m_vao)
            glDeleteVertexArrays(1, &m_vao);
        destroyLuts();
        doneCurrent();
    }
}

ViewerViewModel* ViewerWidget::viewModel() const
{
    return m_vm;
}

double ViewerWidget::zoomFactor() const
{
    return m_vm ? m_vm->zoom() : 1.0;
}

void ViewerWidget::setZoom(double z)
{
    if (m_vm)
        m_vm->setZoom(z);
}

void ViewerWidget::fitToScreen()
{
    if (m_vm) {
        m_vm->fitToWindow();
    }
}

QImage ViewerWidget::captureViewport()
{
    if (isValid()) {
        makeCurrent();
        QImage frame = grabFramebuffer();
        doneCurrent();
        if (!frame.isNull()) {
            return frame;
        }
    }
    return grab().toImage();
}

void ViewerWidget::onTransformStateMaybeChanged()
{
    if (m_isApplyingUndoRedo || !m_vm)
        return;
    ViewportTransform current{ m_vm->flipH(), m_vm->flipV(), m_vm->rotation() };
    if (current == m_currentTransform)
        return;
    m_undoStack.push_back(m_currentTransform);
    if (m_undoStack.size() > 500) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_currentTransform = current;
    m_redoStack.clear();
    emit transformHistoryChanged(!m_undoStack.empty(), !m_redoStack.empty());
}

bool ViewerWidget::undoTransform()
{
    if (m_undoStack.empty() || !m_vm)
        return false;
    ViewportTransform prev = m_undoStack.back();
    m_undoStack.pop_back();
    m_redoStack.push_back(m_currentTransform);

    m_isApplyingUndoRedo = true;
    m_currentTransform = prev;
    m_vm->setFlipH(prev.flipH);
    m_vm->setFlipV(prev.flipV);
    m_vm->setRotation(prev.rotation);
    m_isApplyingUndoRedo = false;

    emit transformHistoryChanged(!m_undoStack.empty(), !m_redoStack.empty());
    update();
    return true;
}

bool ViewerWidget::redoTransform()
{
    if (m_redoStack.empty() || !m_vm)
        return false;
    ViewportTransform next = m_redoStack.back();
    m_redoStack.pop_back();
    m_undoStack.push_back(m_currentTransform);

    m_isApplyingUndoRedo = true;
    m_currentTransform = next;
    m_vm->setFlipH(next.flipH);
    m_vm->setFlipV(next.flipV);
    m_vm->setRotation(next.rotation);
    m_isApplyingUndoRedo = false;

    emit transformHistoryChanged(!m_undoStack.empty(), !m_redoStack.empty());
    update();
    return true;
}

void ViewerWidget::resetTransformHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
    if (m_vm) {
        m_currentTransform = { m_vm->flipH(), m_vm->flipV(), m_vm->rotation() };
    } else {
        m_currentTransform = {};
    }
    emit transformHistoryChanged(false, false);
}

// ---------------------------------------------------------------------------
// OpenGL lifecycle
// ---------------------------------------------------------------------------

void ViewerWidget::initializeGL()
{
    initializeOpenGLFunctions();
    m_glReady = true;

    m_uploaded.reset();
    m_uploadedB.reset();
    m_hasTexture = false;
    m_hasTextureB = false;
    m_key.clear();

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    rebuildProgram();
    uploadImage();
    uploadImageB();
}

void ViewerWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void ViewerWidget::resizeEvent(QResizeEvent* e)
{
    QOpenGLWidget::resizeEvent(e);
    const QSize s = (e && e->size().isValid()) ? e->size() : size();
    m_vm->setViewportSize(s);
    if (m_floatingDock) {
        m_floatingDock->reposition();
    }
    if (m_spinner) {
        m_spinner->move((s.width() - m_spinner->width()) / 2, (s.height() - m_spinner->height()) / 2);
    }
}

void ViewerWidget::paintGL()
{
    const qreal dpr = devicePixelRatio();
    glViewport(0, 0, static_cast<GLsizei>(width() * dpr), static_cast<GLsizei>(height() * dpr));

    glClearColor(0.11f, 0.11f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (width() <= 0 || height() <= 0)
        return;

    const auto img = m_vm->image();
    if (!img)
        return;

    rebuildProgram();
    uploadImage();
    uploadImageB();

    if (!m_prog.isLinked())
        return;

    m_prog.bind();
    glBindVertexArray(m_vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    m_prog.setUniformValue("uImage", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (m_hasTextureB && m_texB) ? m_texB : m_tex);
    m_prog.setUniformValue("uImageB", 1);

    for (const auto& lut : m_luts) {
        glActiveTexture(GL_TEXTURE0 + lut.unit);
        glBindTexture(lut.target, lut.tex);
        m_prog.setUniformValue(lut.sampler.constData(), lut.unit);
    }

    m_prog.setUniformValue("uWipeEnabled", m_vm->wipeEnabled() ? 1 : 0);
    m_prog.setUniformValue("uWipeMode", m_vm->wipeMode());
    m_prog.setUniformValue("uWipePos", m_vm->wipePos());
    m_prog.setUniformValue("uExposure", m_vm->exposureMultiplier());
    m_prog.setUniformValue("uGamma", static_cast<float>(m_vm->gamma()));
    m_prog.setUniformValue("uChannel", static_cast<int>(m_vm->channel()));
    m_prog.setUniformValue("uCheckerboard", m_vm->checkerboardEnabled() ? 1 : 0);
    m_prog.setUniformValue("uFlipH", m_vm->flipH() ? 1 : 0);
    m_prog.setUniformValue("uFlipV", m_vm->flipV() ? 1 : 0);
    m_prog.setUniformValue("uRotation", m_vm->rotation());
    m_prog.setUniformValue("uMode360", m_vm->is360Mode() ? 1 : 0);
    m_prog.setUniformValue("uYaw", m_vm->yaw());
    m_prog.setUniformValue("uPitch", m_vm->pitch());
    m_prog.setUniformValue("uFov", m_vm->fov());
    m_prog.setUniformValue("uShowSpheres", m_vm->showPreviewSpheres() ? 1 : 0);
    // Coin d'ancrage du rig lookdev : 0=BL, 1=BR, 2=TL, 3=TR
    const int rigCorner = m_vm->rigCorner();
    m_prog.setUniformValue("uRigAnchor",
                           QVector2D((rigCorner == 1 || rigCorner == 3) ? 1.0f : 0.0f,
                                     (rigCorner >= 2) ? 1.0f : 0.0f));
    m_prog.setUniformValue("uShowChart", m_vm->showColorChart() ? 1 : 0);
    const float vpW = std::max(1.0f, float(width() * dpr));
    const float vpH = std::max(1.0f, float(height() * dpr));
    m_prog.setUniformValue("uViewport", QVector2D(vpW, vpH));
    m_prog.setUniformValue("uImageSize", QVector2D(float(img->width), float(img->height)));
    m_prog.setUniformValue("uPan", QVector2D(float(m_vm->pan().x() * dpr), float(m_vm->pan().y() * dpr)));
    m_prog.setUniformValue("uZoom", float(m_vm->zoom() * dpr));

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    m_prog.release();
}

// ---------------------------------------------------------------------------
// GPU data upload & shader compilation
// ---------------------------------------------------------------------------

void ViewerWidget::onImageChanged()
{
    if (!m_glReady)
        return;
    makeCurrent();
    uploadImage();
    rebuildProgram();
    doneCurrent();
    resetTransformHistory();
    update();
}

void ViewerWidget::onImageBChanged()
{
    if (!m_glReady)
        return;
    makeCurrent();
    uploadImageB();
    doneCurrent();
    update();
}

const float* ViewerWidget::textureSource(const ImagePtr& img, std::vector<float>& scratch,
                                         int& width, int& height, float& scale)
{
    width = img->width;
    height = img->height;
    scale = 1.0f;
    scratch.clear();

    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (maxSize <= 0) {
        maxSize = 4096;  // valeur prudente si le driver ne répond pas
    }

    if (width <= maxSize && height <= maxSize) {
        return img->rgba.data();
    }

    const double factor = double(maxSize) / double(std::max(width, height));
    const int dstW = std::max(1, int(double(width) * factor));
    const int dstH = std::max(1, int(double(height) * factor));

    scratch = downscaleRgba(img->rgba, width, height, dstW, dstH);
    if (scratch.empty()) {
        return img->rgba.data();
    }

    qWarning().noquote() << QStringLiteral("SView: %1x%2 dépasse GL_MAX_TEXTURE_SIZE (%3) — proxy %4x%5")
                                .arg(width).arg(height).arg(maxSize).arg(dstW).arg(dstH);

    width = dstW;
    height = dstH;
    scale = float(factor);
    return scratch.data();
}

void ViewerWidget::uploadImage()
{
    const auto img = m_vm->image();
    if (!img || img == m_uploaded)
        return;

    std::vector<float> scratch;
    int texW = img->width;
    int texH = img->height;
    float scale = 1.0f;
    const float* pixels = textureSource(img, scratch, texW, texH, scale);
    if (!pixels || texW <= 0 || texH <= 0) {
        return;
    }
    m_textureScale = scale;

    glBindTexture(GL_TEXTURE_2D, m_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 texW, texH, 0,
                 GL_RGBA, GL_FLOAT, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_uploaded = img;
    m_hasTexture = true;
}

void ViewerWidget::uploadImageB()
{
    const auto imgB = m_vm->imageB();
    if (imgB == m_uploadedB)
        return;

    m_uploadedB = imgB;
    if (!imgB) {
        m_hasTextureB = false;
        if (m_texB != 0) {
            glDeleteTextures(1, &m_texB);
            m_texB = 0;
        }
        return;
    }

    if (m_texB == 0) {
        glGenTextures(1, &m_texB);
    }

    std::vector<float> scratch;
    int texW = imgB->width;
    int texH = imgB->height;
    float scale = 1.0f;
    const float* pixels = textureSource(imgB, scratch, texW, texH, scale);
    if (!pixels || texW <= 0 || texH <= 0) {
        m_hasTextureB = false;
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_texB);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 texW, texH, 0,
                 GL_RGBA, GL_FLOAT, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_hasTextureB = true;
}

void ViewerWidget::rebuildProgram()
{
    const QString key = m_vm->gpuProcessorKey();
    if (key == m_key && m_prog.isLinked())
        return;

    m_key = key;
    auto gpu = m_vm->gpuProcessor();
    if (gpu) {
        auto desc = OCIO::GpuShaderDesc::CreateShaderDesc();
        desc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_3);
        desc->setFunctionName("OCIODisplay");
        desc->setResourcePrefix("ocio_");
        gpu->extractGpuShaderInfo(desc);

        buildProgram(QString::fromUtf8(desc->getShaderText()));
        uploadLuts(desc);
    } else {
        buildProgram(QString::fromUtf8(kFallbackOcio));
        destroyLuts();
    }
}

bool ViewerWidget::buildProgram(const QString& ocioFunctionSource)
{
    m_prog.removeAllShaders();

    if (!m_prog.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)) {
        qWarning() << "Vertex shader compile error:" << m_prog.log();
        return false;
    }

    const QString fsSource = QString::fromLatin1(kFragmentHeader)
                           + ocioFunctionSource
                           + QString::fromLatin1(kFragmentMain);

    if (!m_prog.addShaderFromSourceCode(QOpenGLShader::Fragment, fsSource)) {
        qWarning() << "Fragment shader compile error:" << m_prog.log();
        return false;
    }

    if (!m_prog.link()) {
        qWarning() << "Shader link error:" << m_prog.log();
        return false;
    }

    return true;
}

void ViewerWidget::uploadLuts(const OCIO::GpuShaderDescRcPtr& desc)
{
    destroyLuts();
    if (!desc)
        return;

    int unit = 2; // Texture units 0 (uImage) and 1 (uImageB) are reserved

    // 1) Textures 3D
    const unsigned int num3D = desc->getNum3DTextures();
    for (unsigned int i = 0; i < num3D; ++i) {
        const char* name = nullptr;
        const char* sampler = nullptr;
        unsigned int edgelen = 0;
        OCIO::Interpolation interp = OCIO::INTERP_LINEAR;
        desc->get3DTexture(i, name, sampler, edgelen, interp);

        const float* data = nullptr;
        desc->get3DTextureValues(i, data);
        if (!data || edgelen == 0)
            continue;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER,
                        interp == OCIO::INTERP_NEAREST ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER,
                        interp == OCIO::INTERP_NEAREST ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F,
                     edgelen, edgelen, edgelen, 0,
                     GL_RGB, GL_FLOAT, data);
        glBindTexture(GL_TEXTURE_3D, 0);

        m_luts.push_back({tex, GL_TEXTURE_3D, unit++, sampler});
    }

    // 2) Textures 1D / 2D
    const unsigned int num2D = desc->getNumTextures();
    for (unsigned int i = 0; i < num2D; ++i) {
        const char* name = nullptr;
        const char* sampler = nullptr;
        unsigned int width = 0;
        unsigned int height = 0;
        OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        OCIO::GpuShaderDesc::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_2D;
        OCIO::Interpolation interp = OCIO::INTERP_LINEAR;
        desc->getTexture(i, name, sampler, width, height, channel, dimensions, interp);

        const float* data = nullptr;
        desc->getTextureValues(i, data);
        if (!data || width == 0 || height == 0)
            continue;

        const GLenum format = (channel == OCIO::GpuShaderDesc::TEXTURE_RED_CHANNEL) ? GL_RED : GL_RGB;
        const GLint internalFormat = (channel == OCIO::GpuShaderDesc::TEXTURE_RED_CHANNEL) ? GL_R32F : GL_RGB32F;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        interp == OCIO::INTERP_NEAREST ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        interp == OCIO::INTERP_NEAREST ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                     width, height, 0,
                     format, GL_FLOAT, data);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_luts.push_back({tex, GL_TEXTURE_2D, unit++, sampler});
    }
}

void ViewerWidget::destroyLuts()
{
    for (const auto& lut : m_luts) {
        if (lut.tex)
            glDeleteTextures(1, &lut.tex);
    }
    m_luts.clear();
}

// ---------------------------------------------------------------------------
// Interactions : Drag & Drop, Souris, Clavier
// ---------------------------------------------------------------------------

void ViewerWidget::dragEnterEvent(QDragEnterEvent* e)
{
    if (!FolderModel::allSupportedPaths(e->mimeData()).isEmpty()) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void ViewerWidget::dragMoveEvent(QDragMoveEvent* e)
{
    if (!FolderModel::allSupportedPaths(e->mimeData()).isEmpty()) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void ViewerWidget::dropEvent(QDropEvent* e)
{
    const QStringList paths = FolderModel::allSupportedPaths(e->mimeData());
    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        emit fileDropped(paths.first());
        if (receivers(SIGNAL(filesDropped(QStringList))) == 0 && receivers(SIGNAL(fileDropped(QString))) == 0) {
            m_vm->openPath(paths.first());
            if (paths.size() >= 2) {
                m_vm->openImageB(paths.at(1));
            }
        }
        if (e->possibleActions() & Qt::CopyAction)
            e->setDropAction(Qt::CopyAction);
        e->accept();
        setFocus();
        return;
    }
    e->ignore();
}

void ViewerWidget::leaveEvent(QEvent* e)
{
    m_vm->probe(QPointF(-1, -1));
    unsetCursor();
    QOpenGLWidget::leaveEvent(e);
}

void ViewerWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton) {
        if (m_vm) {
            m_vm->probe(e->position());
            const auto& probe = m_vm->currentProbeData();
            if (probe.hasPixel) {
                const QString rgbaText = QString::asprintf("R: %1.4f  G: %1.4f  B: %1.4f  A: %1.4f",
                                                           probe.r, probe.g, probe.b, probe.a);
                QGuiApplication::clipboard()->setText(rgbaText);
                QToolTip::showText(QCursor::pos(),
                                   tr("Pixel Copied (X: %1, Y: %2)\n%3")
                                       .arg(probe.x).arg(probe.y).arg(rgbaText),
                                   this, QRect(), 1500);
            }
        }
        emit copyPixelRequested();
        e->accept();
        return;
    }

    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) {
        if (m_vm->wipeEnabled() && m_vm->wipeMode() != 2 && e->button() == Qt::LeftButton) {
            const float mouseCoord = (m_vm->wipeMode() == 0) ? float(e->position().x()) : float(e->position().y());
            const float splitCoord = (m_vm->wipeMode() == 0) ? (m_vm->wipePos() * float(width()))
                                                             : (m_vm->wipePos() * float(height()));
            if (std::abs(mouseCoord - splitCoord) <= 6.0f) {
                m_draggingWipe = true;
                e->accept();
                return;
            }
        }
        m_dragging = true;
        m_lastPos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ViewerWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_floatingDock) {
        m_floatingDock->notifyUserActivity();
    }

    // A/B Wipe divider hover & drag (Onion Skin mode 2 has no divider line)
    if (m_vm->wipeEnabled() && m_vm->wipeMode() != 2) {
        const float mouseCoord = (m_vm->wipeMode() == 0) ? float(e->position().x()) : float(e->position().y());
        const float splitCoord = (m_vm->wipeMode() == 0) ? (m_vm->wipePos() * float(width()))
                                                         : (m_vm->wipePos() * float(height()));

        if (m_draggingWipe) {
            const float newPos = (m_vm->wipeMode() == 0)
                ? (float(e->position().x()) / float(std::max(1, width())))
                : (float(e->position().y()) / float(std::max(1, height())));
            m_vm->setWipePos(std::clamp(newPos, 0.0f, 1.0f));
            update();
            m_vm->probe(e->position());
            return;
        }

        if (std::abs(mouseCoord - splitCoord) <= 6.0f) {
            setCursor(m_vm->wipeMode() == 0 ? Qt::SplitHCursor : Qt::SplitVCursor);
        } else if (!m_dragging) {
            unsetCursor();
        }
    } else if (!m_dragging) {
        unsetCursor();
    }

    if (m_dragging) {
        const QPoint delta = e->pos() - m_lastPos;
        m_lastPos = e->pos();
        if (m_vm->is360Mode()) {
            const float deltaYaw = -float(delta.x()) * 0.005f;
            const float deltaPitch = float(delta.y()) * 0.005f;
            m_vm->rotate360(deltaYaw, deltaPitch);
        } else {
            m_vm->panBy(delta);
        }
    }

    m_vm->probe(e->position());
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) {
        if (m_draggingWipe) {
            m_draggingWipe = false;
        }
        if (m_dragging) {
            m_dragging = false;
        }

        if (m_vm->wipeEnabled() && m_vm->wipeMode() != 2) {
            const float mouseCoord = (m_vm->wipeMode() == 0) ? float(e->position().x()) : float(e->position().y());
            const float splitCoord = (m_vm->wipeMode() == 0) ? (m_vm->wipePos() * float(width()))
                                                             : (m_vm->wipePos() * float(height()));
            if (std::abs(mouseCoord - splitCoord) <= 6.0f) {
                setCursor(m_vm->wipeMode() == 0 ? Qt::SplitHCursor : Qt::SplitVCursor);
            } else {
                unsetCursor();
            }
        } else {
            unsetCursor();
        }
    }
}

void ViewerWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        if (m_vm && m_vm->is360Mode()) {
            m_vm->resetCamera360();
        } else {
            emit fullscreenToggleRequested();
        }
        e->accept();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(e);
}

void ViewerWidget::wheelEvent(QWheelEvent* e)
{
    if (!m_vm) {
        e->ignore();
        return;
    }
    if (m_vm->is360Mode()) {
        const double degrees = e->angleDelta().y() / 8.0;
        const float deltaFov = -float(degrees * (3.14159265358979323846 / 180.0) * 0.25);
        m_vm->setFov(m_vm->fov() + deltaFov);
        m_vm->probe(e->position());
        e->accept();
    } else {
        if (e->angleDelta().y() == 0) {
            e->ignore();
            return;
        }
        m_vm->setViewportSize(size());
        const double degrees = e->angleDelta().y() / 8.0;
        const double factor = std::pow(1.0015, degrees * 15.0);
        m_vm->zoomBy(factor, e->position());
        m_vm->probe(e->position());
        e->accept();
    }
}

void ViewerWidget::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Space:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        if (m_floatingDock) {
            m_floatingDock->notifyKeyboardNavigation();
        }
        break;
    default:
        break;
    }

    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_PageUp:
        m_vm->prev();
        break;
    case Qt::Key_Right:
    case Qt::Key_PageDown:
        m_vm->next();
        break;
    case Qt::Key_Space:
        // La lecture de séquence n'existe plus : Space passe à l'image suivante.
        m_vm->next();
        break;
    case Qt::Key_Home:
        m_vm->first();
        break;
    case Qt::Key_End:
        m_vm->last();
        break;

    case Qt::Key_3:
        m_vm->toggle360Mode();
        break;

    case Qt::Key_F:
        m_vm->fitToWindow();
        break;
    case Qt::Key_1:
        m_vm->actualSize();
        break;

    case Qt::Key_Plus:
    case Qt::Key_Equal:
        m_vm->adjustExposure(+0.25);
        break;
    case Qt::Key_Minus:
        m_vm->adjustExposure(-0.25);
        break;
    case Qt::Key_0:
        m_vm->resetExposure();
        break;

    case Qt::Key_C:
        if (e->modifiers() & Qt::ControlModifier) {
            emit copyPixelRequested();
            if (m_vm && m_vm->currentProbeData().hasPixel) {
                const auto& d = m_vm->currentProbeData();
                const QString text = QStringLiteral("X: %1  Y: %2  (U: %3, V: %4)\n"
                                                    "R: %5  (%6)\n"
                                                    "G: %7  (%8)\n"
                                                    "B: %9  (%10)\n"
                                                    "A: %11 (%12)\n"
                                                    "Luma: %13\n"
                                                    "Hex: %14")
                                         .arg(d.x).arg(d.y)
                                         .arg(QString::asprintf("%1.4f", d.u))
                                         .arg(QString::asprintf("%1.4f", d.v))
                                         .arg(QString::asprintf("%1.4f", d.r)).arg(QString::asprintf("%1.4e", d.r))
                                         .arg(QString::asprintf("%1.4f", d.g)).arg(QString::asprintf("%1.4e", d.g))
                                         .arg(QString::asprintf("%1.4f", d.b)).arg(QString::asprintf("%1.4e", d.b))
                                         .arg(QString::asprintf("%1.4f", d.a)).arg(QString::asprintf("%1.4e", d.a))
                                         .arg(QString::asprintf("%1.4f", d.luminance))
                                         .arg(d.hexCode);
                QGuiApplication::clipboard()->setText(text);
                QToolTip::showText(QCursor::pos(),
                                   tr("Pixel Copied (X: %1, Y: %2)\nR: %3 G: %4 B: %5 A: %6")
                                       .arg(d.x).arg(d.y)
                                       .arg(QString::asprintf("%1.4f", d.r))
                                       .arg(QString::asprintf("%1.4f", d.g))
                                       .arg(QString::asprintf("%1.4f", d.b))
                                       .arg(QString::asprintf("%1.4f", d.a)),
                                   this, QRect(), 1500);
            }
            e->accept();
            return;
        } else {
            m_vm->cycleView(+1);
        }
        break;

    case Qt::Key_R:
        if (m_vm->is360Mode())
            m_vm->resetCamera360();
        else
            m_vm->setChannel(m_vm->channel() == ChannelMode::Red ? ChannelMode::RGB : ChannelMode::Red);
        break;
    case Qt::Key_G:
        m_vm->setChannel(m_vm->channel() == ChannelMode::Green ? ChannelMode::RGB : ChannelMode::Green);
        break;
    case Qt::Key_B:
        m_vm->setChannel(m_vm->channel() == ChannelMode::Blue ? ChannelMode::RGB : ChannelMode::Blue);
        break;
    case Qt::Key_A:
        m_vm->setChannel(m_vm->channel() == ChannelMode::Alpha ? ChannelMode::RGB : ChannelMode::Alpha);
        break;
    case Qt::Key_L:
        if (m_vm->is360Mode())
            m_vm->togglePreviewSpheres();
        else
            m_vm->setChannel(m_vm->channel() == ChannelMode::Luma ? ChannelMode::RGB : ChannelMode::Luma);
        break;

    case Qt::Key_I:
    case Qt::Key_Tab:
        toggleOsd();
        break;

    case Qt::Key_Z:
        if (e->modifiers() & Qt::ControlModifier) {
            if (e->modifiers() & Qt::ShiftModifier) {
                redoTransform();
            } else {
                undoTransform();
            }
            e->accept();
            return;
        }
        break;

    case Qt::Key_Y:
        if (e->modifiers() & Qt::ControlModifier) {
            redoTransform();
            e->accept();
            return;
        }
        break;

    case Qt::Key_W:
        if (e->modifiers() & Qt::ControlModifier) {
            if (m_vm && m_vm->image()) {
                m_vm->closeImage();
            } else if (window()) {
                window()->close();
            }
            e->accept();
            return;
        }
        break;

    default:
        QOpenGLWidget::keyPressEvent(e);
        break;
    }
}

void ViewerWidget::setPlaybackController(PlaybackController* controller)
{
    if (m_playback == controller)
        return;
    m_playback = controller;
    if (m_playback) {
        connect(m_playback, &PlaybackController::frameChanged, this, &ViewerWidget::updateOsd);
        connect(m_playback, &PlaybackController::fpsChanged, this, &ViewerWidget::updateOsd);
    }
    updateOsd();
}

void ViewerWidget::setOsdVisible(bool visible)
{
    m_osdVisible = visible;
    updateOsd();
}

void ViewerWidget::toggleOsd()
{
    setOsdVisible(!m_osdVisible);
}

void ViewerWidget::updateOsd()
{
    if (!m_osdLabel)
        return;
    if (m_osdVisible) {
        m_osdLabel->setText(osdText());
        m_osdLabel->adjustSize();
        m_osdLabel->show();
        m_osdLabel->raise();
    } else {
        m_osdLabel->hide();
    }
}

QString ViewerWidget::osdText() const
{
    if (!m_vm || !m_vm->image() || m_vm->image()->path.isEmpty()) {
        return QStringLiteral("No Media");
    }

    QString fileName = QFileInfo(m_vm->image()->path).fileName();
    if (m_playback && m_playback->sequence() && m_playback->sequence()->isSequence()) {
        const int frameNum = m_playback->currentFrameNumber();
        fileName += QStringLiteral(" [frame %1]").arg(frameNum);
    }

    double fps = 24.0;
    if (m_playback) {
        fps = m_playback->fps();
    }

    QString cs = m_vm ? m_vm->currentColorSpace() : QStringLiteral("sRGB");
    if (cs.isEmpty()) {
        cs = QStringLiteral("sRGB");
    }

    QString reference = QStringLiteral("%1 | %2 fps | %3")
        .arg(fileName)
        .arg(QString::number(fps, 'f', 1))
        .arg(cs);

    // Signale une réduction de texture (image plus grande que le GPU).
    if (m_textureScale < 0.999f) {
        reference += QStringLiteral(" | proxy %1%").arg(int(m_textureScale * 100.0f));
    }
    return reference;
}

void ViewerWidget::setFloatingControlDock(FloatingControlDock* dock)
{
    m_floatingDock = dock;
    if (m_floatingDock) {
        m_floatingDock->setParent(this);
        m_floatingDock->reposition();
    }
}

void ViewerWidget::setFloatingDockEnabled(bool enabled)
{
    if (m_floatingDock) {
        m_floatingDock->setDockEnabled(enabled);
    }
}

}  // namespace sview
