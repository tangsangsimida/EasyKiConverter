#ifndef PREVIEWIMAGEEXPORTER_H
#define PREVIEWIMAGEEXPORTER_H

#include "ExportService.h"

#include <QDir>
#include <QObject>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class PreviewImageExporter : public QObject {
    Q_OBJECT
public:
    explicit PreviewImageExporter(QObject* parent = nullptr);
    ~PreviewImageExporter() override;

    void setOptions(const ExportOptions& options);
    void setNetworkManager(QNetworkAccessManager* manager);

    bool exportFromUrls(const QStringList& imageUrls, const QString& outputPath, const QString& componentName);
    bool exportFromCache(const QStringList& imagePaths, const QString& outputPath, const QString& componentName);

signals:
    void exportCompleted(int successCount);
    void exportFailed(const QString& error);

private:
    bool createPreviewDirectory(const QString& dirPath);
    QString generateFileName(const QString& componentName, const QString& extension, int index, int total);

private:
    ExportOptions m_options;
    QNetworkAccessManager* m_networkManager;
};

}  // namespace EasyKiConverter

#endif  // PREVIEWIMAGEEXPORTER_H