#include "PreviewImageExporter.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

PreviewImageExporter::PreviewImageExporter(QObject* parent) : QObject(parent) {}

PreviewImageExporter::~PreviewImageExporter() = default;

void PreviewImageExporter::setOptions(const ExportOptions& options) {
    m_options = options;
}

bool PreviewImageExporter::exportFromUrls(const QStringList& imageUrls,
                                          const QString& outputPath,
                                          const QString& componentName) {
    if (imageUrls.isEmpty()) {
        return true;
    }

    QString imagesDirPath = QString("%1/%2.preview").arg(outputPath, m_options.libName);
    if (!createPreviewDirectory(imagesDirPath)) {
        qWarning() << "Failed to create preview images directory:" << imagesDirPath;
        return false;
    }

    bool allSuccess = true;
    for (int i = 0; i < imageUrls.size(); ++i) {
        QString imageUrl = imageUrls[i];
        QString fileExtension = QFileInfo(imageUrl).suffix();
        if (fileExtension.isEmpty()) {
            fileExtension = "jpg";
        }

        QString fileName = generateFileName(componentName, fileExtension, i + 1, imageUrls.size());
        QString imagePath = QString("%1/%2").arg(imagesDirPath, fileName);

        if (!m_options.overwriteExistingFiles && QFile::exists(imagePath)) {
            qWarning() << "Preview image already exists:" << imagePath;
            continue;
        }

        // Use NetworkClient with default retry policy (3 retries, 30s timeout)
        NetworkResult result = NetworkClient::instance().get(QUrl(imageUrl));

        if (result.success) {
            QFile file(imagePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(result.data);
                file.close();
                qDebug() << "Preview image exported successfully:" << imagePath;
            } else {
                qWarning() << "Failed to open file for writing:" << imagePath;
                allSuccess = false;
            }
        } else {
            qWarning() << "Failed to download preview image:" << imageUrl << result.error;
            allSuccess = false;
        }
    }

    return allSuccess;
}

bool PreviewImageExporter::exportFromCache(const QStringList& imagePaths,
                                           const QString& outputPath,
                                           const QString& componentName) {
    if (imagePaths.isEmpty()) {
        return true;
    }

    // exportFromCache 只进行文件复制操作，不需要网络管理器
    QString imagesDirPath = QString("%1/%2.preview").arg(outputPath, m_options.libName);
    if (!createPreviewDirectory(imagesDirPath)) {
        qWarning() << "Failed to create preview images directory:" << imagesDirPath;
        return false;
    }

    bool allSuccess = true;
    for (int i = 0; i < imagePaths.size(); ++i) {
        QString sourcePath = imagePaths[i];

        if (!QFile::exists(sourcePath)) {
            qWarning() << "Source preview image file not found:" << sourcePath;
            allSuccess = false;
            continue;
        }

        QString fileExtension = QFileInfo(sourcePath).suffix();
        if (fileExtension.isEmpty()) {
            fileExtension = "jpg";
        }

        QString fileName = generateFileName(componentName, fileExtension, i + 1, imagePaths.size());
        QString destPath = QString("%1/%2").arg(imagesDirPath, fileName);

        if (!m_options.overwriteExistingFiles && QFile::exists(destPath)) {
            qWarning() << "Preview image already exists:" << destPath;
            continue;
        }

        if (QFile::copy(sourcePath, destPath)) {
            qDebug() << "Preview image copied from cache:" << destPath;
        } else {
            qWarning() << "Failed to copy preview image from cache:" << sourcePath << "to" << destPath;
            allSuccess = false;
        }
    }

    return allSuccess;
}

bool PreviewImageExporter::createPreviewDirectory(const QString& dirPath) {
    QDir dir;
    if (dir.exists(dirPath)) {
        return true;
    }
    return dir.mkpath(dirPath);
}

QString PreviewImageExporter::generateFileName(const QString& componentName,
                                               const QString& extension,
                                               int index,
                                               int total) {
    if (total == 1) {
        return QString("%1.%2").arg(componentName, extension);
    }
    return QString("%1_%2.%3").arg(componentName).arg(index).arg(extension);
}

}  // namespace EasyKiConverter
