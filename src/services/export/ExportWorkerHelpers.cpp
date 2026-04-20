#include "ExportWorkerHelpers.h"

#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

// ExportOptions is defined in ExportProgress.h which is included by the header

namespace EasyKiConverter {

QString ExportWorkerHelpers::defaultOutputDir(const QString& subdir) {
    return QDir::currentPath() + QStringLiteral("/export/") + subdir;
}

QString ExportWorkerHelpers::ensureOutputDir(const struct ExportOptions& options, const QString& subdir) {
    QString outputDir = options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = defaultOutputDir(subdir);
    }
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        return QString();
    }
    return outputDir;
}

QString ExportWorkerHelpers::buildFilePath(const QString& componentId,
                                           const QString& outputDir,
                                           const QString& fileExtension) {
    return outputDir + QStringLiteral("/") + componentId + fileExtension;
}

bool ExportWorkerHelpers::shouldSkipExisting(const QString& filePath, const struct ExportOptions& options) {
    return QFile::exists(filePath) && !options.overwriteExistingFiles;
}

QSharedPointer<ComponentData> ExportWorkerHelpers::loadDiskCachedComponentData(const QString& componentId) {
    ComponentCacheService* cache = ComponentCacheService::instance();
    QSharedPointer<ComponentData> cachedData = cache->loadComponentData(componentId);
    if (!cachedData) {
        return nullptr;
    }

    const QByteArray cadJsonData = cache->loadCadDataJson(componentId);
    if (!cadJsonData.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonData, &parseError);
        if (parseError.error == QJsonParseError::NoError && cadDoc.isObject()) {
            const QJsonObject cadDataObject = cadDoc.object();
            EasyedaSymbolImporter symbolImporter;
            EasyedaFootprintImporter footprintImporter;
            cachedData->setSymbolData(symbolImporter.importSymbolData(cadDataObject));
            cachedData->setFootprintData(footprintImporter.importFootprintData(cadDataObject));
        }
    }

    const QByteArray datasheetData = cache->loadDatasheet(componentId);
    if (!datasheetData.isEmpty()) {
        cachedData->setDatasheetData(datasheetData);
    }

    return cachedData;
}

void ExportWorkerHelpers::mergeComponentData(ComponentData& target, const QSharedPointer<ComponentData>& fallback) {
    if (!fallback) {
        return;
    }

    if (target.lcscId().isEmpty()) {
        target = *fallback;
        return;
    }

    if (target.symbolData() == nullptr && fallback->symbolData() != nullptr) {
        target.setSymbolData(fallback->symbolData());
    }
    if (target.footprintData() == nullptr && fallback->footprintData() != nullptr) {
        target.setFootprintData(fallback->footprintData());
    }
    if ((target.model3DData() == nullptr || target.model3DData()->uuid().isEmpty()) &&
        fallback->model3DData() != nullptr) {
        target.setModel3DData(fallback->model3DData());
    }
    if (target.previewImages().isEmpty() && !fallback->previewImages().isEmpty()) {
        target.setPreviewImages(fallback->previewImages());
    }
    if (target.previewImageData().isEmpty() && !fallback->previewImageData().isEmpty()) {
        target.setPreviewImageData(fallback->previewImageData());
    }
    if (target.datasheet().isEmpty() && !fallback->datasheet().isEmpty()) {
        target.setDatasheet(fallback->datasheet());
    }
    if (target.datasheetFormat().isEmpty() && !fallback->datasheetFormat().isEmpty()) {
        target.setDatasheetFormat(fallback->datasheetFormat());
    }
    if (target.datasheetData().isEmpty() && !fallback->datasheetData().isEmpty()) {
        target.setDatasheetData(fallback->datasheetData());
    }
    if (target.name().isEmpty() && !fallback->name().isEmpty()) {
        target.setName(fallback->name());
    }
    if (target.prefix().isEmpty() && !fallback->prefix().isEmpty()) {
        target.setPrefix(fallback->prefix());
    }
    if (target.package().isEmpty() && !fallback->package().isEmpty()) {
        target.setPackage(fallback->package());
    }
    if (target.manufacturer().isEmpty() && !fallback->manufacturer().isEmpty()) {
        target.setManufacturer(fallback->manufacturer());
    }
    if (target.manufacturerPart().isEmpty() && !fallback->manufacturerPart().isEmpty()) {
        target.setManufacturerPart(fallback->manufacturerPart());
    }
}

void ExportWorkerHelpers::recomputeTypeProgressCounts(ExportTypeProgress& progress) {
    progress.completedCount = 0;
    progress.successCount = 0;
    progress.failedCount = 0;
    progress.skippedCount = 0;
    progress.inProgressCount = 0;

    for (auto it = progress.itemStatus.cbegin(); it != progress.itemStatus.cend(); ++it) {
        switch (it.value().status) {
            case ExportItemStatus::Status::Pending:
                break;
            case ExportItemStatus::Status::InProgress:
                progress.inProgressCount++;
                break;
            case ExportItemStatus::Status::Success:
                progress.completedCount++;
                progress.successCount++;
                break;
            case ExportItemStatus::Status::Failed:
                progress.completedCount++;
                progress.failedCount++;
                break;
            case ExportItemStatus::Status::Skipped:
                progress.completedCount++;
                progress.skippedCount++;
                break;
        }
    }
}

}  // namespace EasyKiConverter
