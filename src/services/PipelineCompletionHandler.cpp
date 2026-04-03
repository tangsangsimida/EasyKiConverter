#include "PipelineCompletionHandler.h"

#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"
#include "ui/viewmodels/ComponentListViewModel.h"
#include "ui/viewmodels/ExportProgressViewModel.h"

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

void PipelineCompletionHandler::exportPreviewImagesFromViewModel(ComponentListViewModel* componentListViewModel,
                                                                 ExportProgressViewModel* progressViewModel,
                                                                 const ExportOptions& options) {
    if (!options.exportPreviewImages || !componentListViewModel) {
        return;
    }

    // 从 ViewModel 获取最新的数据
    QMap<QString, QSharedPointer<ComponentData>> allData = componentListViewModel->getAllPreloadedData();
    if (allData.isEmpty()) {
        qDebug() << "No preloaded data available from ViewModel for preview images export";
        emit previewImagesExported(0);
        return;
    }

    int successCount = 0;
    for (auto it = allData.constBegin(); it != allData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();
        QString componentId = componentData->lcscId();

        QList<QByteArray> previewImageDataList = componentData->previewImageData();

        // 1. 如果内存中有数据，直接使用
        // 2. 如果内存中没有但有 URL，尝试从磁盘缓存加载
        if (previewImageDataList.isEmpty() && !componentData->previewImages().isEmpty()) {
            const QStringList& imageUrls = componentData->previewImages();
            for (int i = 0; i < imageUrls.size() && i < 3; ++i) {
                QByteArray imageData = ComponentCacheService::instance()->loadPreviewImage(componentId, i);
                if (!imageData.isEmpty()) {
                    previewImageDataList.append(imageData);
                    qDebug() << "Preview image loaded from disk cache for component:" << componentId << "index:" << i;
                } else {
                    // 3. 磁盘缓存也没有，从 URL 下载并缓存
                    qDebug() << "Preview image not in cache, downloading for component:" << componentId
                             << "index:" << i;
                    imageData = ComponentCacheService::instance()->downloadPreviewImage(componentId, imageUrls[i], i);
                    if (!imageData.isEmpty()) {
                        previewImageDataList.append(imageData);
                    }
                }
            }
        }

        bool exportSuccess = false;
        if (!previewImageDataList.isEmpty()) {
            QString componentName = componentData->name().isEmpty() ? componentId : componentData->name();

            if (exportPreviewImagesFromMemory(previewImageDataList, options.outputPath, componentName)) {
                successCount++;
                exportSuccess = true;
            } else {
                qWarning() << "Failed to export preview images for component:" << componentId;
            }
        } else {
            // 没有预览图数据但不需要导出时，也标记为成功（没有预览图不代表失败）
            if (!options.exportPreviewImages || componentData->previewImages().isEmpty()) {
                exportSuccess = true;
            }
        }

        // 更新预览图导出状态
        componentListViewModel->updateExportStatus(componentId, exportSuccess ? 1 : 0, -1);
        if (progressViewModel) {
            progressViewModel->updateComponentExportStatus(componentId, exportSuccess ? 1 : 0, -1);
        }
    }
    qDebug() << "Preview images export from ViewModel completed:" << successCount << "components";
    emit previewImagesExported(successCount);
}

void PipelineCompletionHandler::exportDatasheetsFromViewModel(ComponentListViewModel* componentListViewModel,
                                                              ExportProgressViewModel* progressViewModel,
                                                              const ExportOptions& options) {
    if (!options.exportDatasheet || !componentListViewModel) {
        return;
    }

    // 从 ViewModel 获取最新的数据
    QMap<QString, QSharedPointer<ComponentData>> allData = componentListViewModel->getAllPreloadedData();
    if (allData.isEmpty()) {
        qDebug() << "No preloaded data available from ViewModel for datasheet export";
        emit datasheetsExported(0);
        return;
    }

    int successCount = 0;
    for (auto it = allData.constBegin(); it != allData.constEnd(); ++it) {
        const QSharedPointer<ComponentData>& componentData = it.value();
        QString componentId = componentData->lcscId();

        QByteArray datasheetData = componentData->datasheetData();
        QString datasheetFormat = componentData->datasheetFormat();

        // 1. 如果内存中有数据，直接使用
        // 2. 如果内存中没有但有 URL，尝试从磁盘缓存加载
        if (datasheetData.isEmpty() && !componentData->datasheet().isEmpty()) {
            datasheetData = ComponentCacheService::instance()->loadDatasheet(componentId);
            if (!datasheetData.isEmpty()) {
                qDebug() << "Datasheet loaded from disk cache for component:" << componentId;
            } else {
                // 3. 磁盘缓存也没有，从 URL 下载并缓存
                qDebug() << "Datasheet not in cache, downloading for component:" << componentId;
                datasheetData = ComponentCacheService::instance()->downloadDatasheet(
                    componentId, componentData->datasheet(), &datasheetFormat);
            }
        }

        bool exportSuccess = false;
        if (!datasheetData.isEmpty()) {
            QString componentName = componentData->name().isEmpty() ? componentId : componentData->name();

            if (exportDatasheetFromMemory(datasheetData, options.outputPath, componentName, datasheetFormat)) {
                successCount++;
                exportSuccess = true;
            }
        } else {
            // 没有手册数据但不需要导出时，也标记为成功（没有手册不代表失败）
            if (!options.exportDatasheet || componentData->datasheet().isEmpty()) {
                exportSuccess = true;
            }
        }

        // 更新手册导出状态
        componentListViewModel->updateExportStatus(componentId, -1, exportSuccess ? 1 : 0);
        if (progressViewModel) {
            progressViewModel->updateComponentExportStatus(componentId, -1, exportSuccess ? 1 : 0);
        }
    }
    qDebug() << "Datasheets export from ViewModel completed:" << successCount << "components";
    emit datasheetsExported(successCount);
}

}  // namespace EasyKiConverter