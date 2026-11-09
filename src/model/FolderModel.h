#pragma once

#include <QString>
#include <QStringList>

class QMimeData;

namespace sview {

// Liste triée (ordre naturel : img2 avant img10) des images d'un dossier + index courant.
class FolderModel {
public:
    static inline QStringList supportedExtensions()
    {
        static const QStringList exts{
            QStringLiteral("exr"), QStringLiteral("hdr"), QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("bmp"), QStringLiteral("tga"), QStringLiteral("webp"),
            QStringLiteral("dpx"), QStringLiteral("cin"), QStringLiteral("psd"), QStringLiteral("tx"), QStringLiteral("pic"),
            QStringLiteral("gif"), QStringLiteral("ico"), QStringLiteral("iff"), QStringLiteral("sgi"), QStringLiteral("rgb"),
            QStringLiteral("rgba"), QStringLiteral("jp2"), QStringLiteral("j2k"), QStringLiteral("dds"), QStringLiteral("ptex"),
            QStringLiteral("ptx"), QStringLiteral("rla"), QStringLiteral("rat"), QStringLiteral("tex")};
        return exts;
    }
    static bool isSupportedPath(const QString& path);
    static bool isSupportedImage(const QString& path);
    static QString firstSupportedPath(const QMimeData* mime);
    static QStringList allSupportedPaths(const QMimeData* mime);
    static QString firstSupportedPathInDir(const QString& dirPath);

    // Ouvre un fichier (scanne son dossier et le sélectionne) ou un dossier.
    bool open(const QString& path);
    void clear() { m_files.clear(); m_index = 0; }

    bool isEmpty() const { return m_files.isEmpty(); }
    int count() const { return int(m_files.size()); }
    int index() const { return m_index; }
    const QStringList& files() const { return m_files; }

    QString currentPath() const;
    QString pathAt(int offsetFromCurrent) const;   // avec bouclage

    void step(int delta);                          // avec bouclage
    void first() { m_index = 0; }
    void last() { m_index = m_files.isEmpty() ? 0 : int(m_files.size()) - 1; }

private:
    void scan(const QString& dir, const QString& select);

    QStringList m_files;
    int m_index = 0;
};

}  // namespace sview
