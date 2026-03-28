#include "PipelineCompletionHandler.h"

#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace EasyKiConverter {

const QRegularExpression PipelineCompletionHandler::INVALID_FILENAME_CHARS("[<>:\"/\\\\|?*]");

PipelineCompletionHandler::PipelineCompletionHandler(QObject* parent) : QObject(parent) {}

PipelineCompletionHandler::~PipelineCompletionHandler() = default;

void PipelineCompletionHandler::exportPreviewImages(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                                                    const ExportOptions& options) {
    if (!options.exportPreviewImages) {
        return;
    }

    int successCount = 0;
    for (auto it = preloadedData.constBegin(); it != preloadedData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();
        QString componentId = componentData->lcscId();

        QList<QByteArray> previewImageDataList = componentData->previewImageData();

        if (!previewImageDataList.isEmpty()) {
            QString componentName = componentData->name().isEmpty() ? componentId : componentData->name();

            if (exportPreviewImagesFromMemory(previewImageDataList, options.outputPath, componentName)) {
                successCount++;
            } else {
                qWarning() << "Failed to export preview images for component:" << componentId;
            }
        }
    }
    qDebug() << "Preview images export completed:" << successCount << "components";
    emit previewImagesExported(successCount);
}

void PipelineCompletionHandler::exportDatasheets(const QMap<QString, QSharedPointer<ComponentData>>& preloadedData,
                                                 const ExportOptions& options) {
    if (!options.exportDatasheet) {
        return;
    }

    int successCount = 0;
    for (auto it = preloadedData.constBegin(); it != preloadedData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();
        if (!componentData->datasheetData().isEmpty()) {
            QString componentName = componentData->name().isEmpty() ? componentData->lcscId() : componentData->name();

            if (exportDatasheetFromMemory(componentData->datasheetData(),
                                          options.outputPath,
                                          componentName,
                                          componentData->datasheetFormat())) {
                successCount++;
            }
        }
    }
    qDebug() << "Datasheets export completed:" << successCount << "components";
    emit datasheetsExported(successCount);
}

bool PipelineCompletionHandler::exportPreviewImagesFromMemory(const QList<QByteArray>& imageDataList,
                                                              const QString& outputPath,
                                                              const QString& componentName) {
    QDir outputDir(outputPath);
    QString imagesDir = outputDir.filePath("images");
    if (!outputDir.exists(imagesDir)) {
        if (!outputDir.mkpath(imagesDir)) {
            qWarning() << "Failed to create images directory:" << imagesDir;
            return false;
        }
    }

    QString safeName = componentName;
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    bool allSuccess = true;
    int exportedCount = 0;

    for (int i = 0; i < imageDataList.size(); ++i) {
        if (imageDataList[i].isEmpty()) {
            continue;
        }

        QString filename = QString("%1_%2.jpg").arg(safeName).arg(i);
        QString filePath = QDir(imagesDir).filePath(filename);

        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(imageDataList[i]);
            file.close();
            exportedCount++;
        } else {
            allSuccess = false;
        }
    }

    return allSuccess;
}

bool PipelineCompletionHandler::exportDatasheetFromMemory(const QByteArray& datasheetData,
                                                          const QString& outputPath,
                                                          const QString& componentName,
                                                          const QString& format) {
    QDir outputDir(outputPath);
    QString datasheetsDir = outputDir.filePath("datasheets");
    if (!outputDir.exists(datasheetsDir)) {
        if (!outputDir.mkpath(datasheetsDir)) {
            qWarning() << "Failed to create datasheets directory:" << datasheetsDir;
            return false;
        }
    }

    QString safeName = componentName;
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    QString extension = format.toLower();
    if (extension != "html") {
        extension = "pdf";
    }

    QString filename = QString("%1.%2").arg(safeName).arg(extension);
    QString filePath = QDir(datasheetsDir).filePath(filename);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(datasheetData);
        file.close();
        return true;
    }
    return false;
}

}  // namespace EasyKiConverter