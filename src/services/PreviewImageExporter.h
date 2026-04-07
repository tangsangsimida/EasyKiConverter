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

    bool exportFromUrls(const QStringList& imageUrls, const QString& outputPath, const QString& componentName);
    bool exportFromCache(const QStringList& imagePaths, const QString& outputPath, const QString& componentName);

signals:
    // 预留接口：当前为同步导出，暂时不使用这些信号
    // 将来如需改为非阻塞异步导出，可启用这些信号进行进度/完成通知
    void exportCompleted(int successCount);
    void exportFailed(const QString& error);

private:
    bool createPreviewDirectory(const QString& dirPath);
    QString generateFileName(const QString& componentName, const QString& extension, int index, int total);

private:
    ExportOptions m_options;
};

}  // namespace EasyKiConverter

#endif  // PREVIEWIMAGEEXPORTER_H
