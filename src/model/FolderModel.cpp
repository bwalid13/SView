#include "FolderModel.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <algorithm>

namespace sview {

bool FolderModel::isSupportedImage(const QString& path)
{
    if (path.isEmpty())
        return false;
    const QFileInfo fi(path);
    if (fi.exists() && fi.isDir())
        return false;
    return supportedExtensions().contains(fi.suffix().toLower());
}

bool FolderModel::isSupportedPath(const QString& path)
{
    if (path.isEmpty())
        return false;
    if (isSupportedImage(path))
        return true;
    const QFileInfo fi(path);
    return fi.isDir();
}

QString FolderModel::firstSupportedPath(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls())
        return {};

    QString firstDir;
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (isSupportedImage(path))
            return path;
        if (firstDir.isEmpty()) {
            const QFileInfo fi(path);
            if (fi.isDir())
                firstDir = path;
        }
    }
    return firstDir;
}

QStringList FolderModel::allSupportedPaths(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls())
        return {};

    QStringList images;
    QStringList dirs;

    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (isSupportedImage(path)) {
            images.append(path);
        } else {
            const QFileInfo fi(path);
            if (fi.isDir()) {
                const QString dirImg = firstSupportedPathInDir(path);
                if (!dirImg.isEmpty())
                    dirs.append(dirImg);
                else
                    dirs.append(path);
            }
        }
    }

    if (!images.isEmpty()) {
        images.append(dirs);
        return images;
    }
    return dirs;
}

QString FolderModel::firstSupportedPathInDir(const QString& dirPath)
{
    const QDir dir(dirPath);
    if (!dir.exists())
        return {};
    QStringList filters;
    for (const QString& e : supportedExtensions())
        filters << QStringLiteral("*.") + e;
    const auto infos = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
    if (infos.isEmpty())
        return {};
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    auto sortedInfos = infos;
    std::sort(sortedInfos.begin(), sortedInfos.end(), [&](const QFileInfo& a, const QFileInfo& b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });
    return QDir::cleanPath(sortedInfos.first().absoluteFilePath());
}

bool FolderModel::open(const QString& path)
{
    const QFileInfo fi(path);
    if (!fi.exists()) {
        m_files.clear();
        m_index = 0;
        return false;
    }

    if (fi.isDir())
        scan(QDir::cleanPath(fi.absoluteFilePath()), QString());
    else
        scan(QDir::cleanPath(fi.absolutePath()), QDir::cleanPath(fi.absoluteFilePath()));

    return !m_files.isEmpty();
}

void FolderModel::scan(const QString& dirPath, const QString& select)
{
    QStringList filters;
    for (const QString& e : supportedExtensions())
        filters << QStringLiteral("*.") + e;

    const QDir dir(dirPath);
    QStringList files;
    const auto infos = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
    for (const QFileInfo& info : infos)
        files << QDir::cleanPath(info.absoluteFilePath());

    // Le fichier demandé est toujours affiché, même si son extension n'est pas dans la liste
    if (!select.isEmpty() && !files.contains(select, Qt::CaseInsensitive))
        files << select;

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), [&](const QString& a, const QString& b) {
        return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
    });

    m_files = files;
    m_index = 0;
    if (!select.isEmpty()) {
        for (int i = 0; i < m_files.size(); ++i) {
            if (m_files[i].compare(select, Qt::CaseInsensitive) == 0) {
                m_index = i;
                break;
            }
        }
    }
}

QString FolderModel::currentPath() const
{
    return m_files.isEmpty() ? QString() : m_files[m_index];
}

QString FolderModel::pathAt(int offset) const
{
    if (m_files.isEmpty())
        return QString();
    const int n = int(m_files.size());
    const int i = ((m_index + offset) % n + n) % n;
    return m_files[i];
}

void FolderModel::step(int delta)
{
    if (m_files.isEmpty())
        return;
    const int n = int(m_files.size());
    m_index = ((m_index + delta) % n + n) % n;
}

}  // namespace sview
