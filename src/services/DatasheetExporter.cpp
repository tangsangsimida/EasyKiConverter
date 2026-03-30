#include "DatasheetExporter.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace EasyKiConverter {

DatasheetExporter::DatasheetExporter(QObject* parent) : QObject(parent), m_networkManager(nullptr) {}

DatasheetExporter::~DatasheetExporter() = default;

void DatasheetExporter::setOptions(const ExportOptions& options) {
    m_options = options;
}

void DatasheetExporter::setNetworkManager(QNetworkAccessManager* manager) {
    m_networkManager = manager;
}

bool DatasheetExporter::exportDatasheet(const QString& datasheetUrl,
                                        const QString& outputPath,
                                        const QString& componentName) {
    if (datasheetUrl.isEmpty()) {
        return true;
    }

    if (!m_networkManager) {
        qWarning() << "DatasheetExporter: No network manager set";
        return false;
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

    QNetworkRequest request(datasheetUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    QNetworkReply* reply = m_networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray datasheetData = reply->readAll();
        QFile file(datasheetPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(datasheetData);
            file.close();
            qDebug() << "Datasheet exported successfully:" << datasheetPath;
            reply->deleteLater();
            return true;
        } else {
            qWarning() << "Failed to open file for writing:" << datasheetPath;
            reply->deleteLater();
            return false;
        }
    } else {
        qWarning() << "Failed to download datasheet:" << datasheetUrl << reply->errorString();
        reply->deleteLater();
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