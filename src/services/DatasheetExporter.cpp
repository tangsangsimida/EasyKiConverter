#include "DatasheetExporter.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace EasyKiConverter {

DatasheetExporter::DatasheetExporter(QObject* parent) : QObject(parent) {}

DatasheetExporter::~DatasheetExporter() = default;

void DatasheetExporter::setOptions(const ExportOptions& options) {
    m_options = options;
}

bool DatasheetExporter::exportDatasheet(const QString& datasheetUrl,
                                        const QString& outputPath,
                                        const QString& componentName) {
    if (datasheetUrl.isEmpty()) {
        return true;
    }

    QString datasheetDirPath = QString("%1/%2.datasheet").arg(outputPath, m_options.libName);
    if (!createDatasheetDirectory(datasheetDirPath)) {
        qWarning() << "Failed to create datasheet directory:" << datasheetDirPath;
        return false;
    }

    QString fileExtension = "pdf";
    QFileInfo urlInfo(datasheetUrl);
    if (!urlInfo.suffix().isEmpty()) {
        fileExtension = urlInfo.suffix();
    }

    QString fileName = QString("%1.%2").arg(componentName, fileExtension);
    QString datasheetPath = QString("%1/%2").arg(datasheetDirPath, fileName);

    if (!m_options.overwriteExistingFiles && QFile::exists(datasheetPath)) {
        qWarning() << "Datasheet already exists:" << datasheetPath;
        return true;
    }

    // Use NetworkClient with default retry policy (3 retries, 30s timeout)
    NetworkResult result = NetworkClient::instance().get(QUrl(datasheetUrl));

    if (result.success) {
        QFile file(datasheetPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(result.data);
            file.close();
            qDebug() << "Datasheet exported successfully:" << datasheetPath;
            return true;
        } else {
            qWarning() << "Failed to open file for writing:" << datasheetPath;
            return false;
        }
    } else {
        qWarning() << "Failed to download datasheet:" << datasheetUrl << result.error;
        return false;
    }
}

bool DatasheetExporter::createDatasheetDirectory(const QString& dirPath) {
    QDir dir;
    if (dir.exists(dirPath)) {
        return true;
    }
    return dir.mkpath(dirPath);
}

}  // namespace EasyKiConverter
