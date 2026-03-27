#ifndef PIPELINECOMPLETIONHANDLER_H
#define PIPELINECOMPLETIONHANDLER_H

#include "ExportService.h"

#include <QObject>
#include <QRegularExpression>
#include <QString>

namespace EasyKiConverter {

class ComponentData;

class PipelineCompletionHandler : public QObject {
    Q_OBJECT
public:
    explicit PipelineCompletionHandler(QObject* parent = nullptr);
    ~PipelineCompletionHandler() override;

    void exportPreviewImages(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                             const ExportOptions& options);

    void exportDatasheets(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                          const ExportOptions& options);

signals:
    void previewImagesExported(int successCount);
    void datasheetsExported(int successCount);

private:
    bool exportPreviewImagesFromMemory(const QList<QByteArray>& imageDataList,
                                       const QString& outputPath,
                                       const QString& componentName);
    bool exportDatasheetFromMemory(const QByteArray& datasheetData,
                                   const QString& outputPath,
                                   const QString& componentName,
                                   const QString& format);
    static const QRegularExpression INVALID_FILENAME_CHARS;
};

}  // namespace EasyKiConverter

#endif  // PIPELINECOMPLETIONHANDLER_H