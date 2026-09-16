#include "ExportWorkerHelpers.h"

#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "models/ComponentData.h"
#include "models/Model3DData.h"
#include "services/ComponentCacheService.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

// ExportOptions is defined in ExportProgress.h which is included by the header

namespace EasyKiConverter {

namespace {

/**
 * @brief 在磁盘缓存缺少独立模型时，从封装模型恢复三维模型元数据。
 * @param data 待补齐的元器件数据。
 */
void restoreModel3DFromFootprint(ComponentData& data) {
    if (data.model3DData() && !data.model3DData()->uuid().isEmpty())
        return;
    if (!data.footprintData())
        return;

    const Model3DData footprintModel = data.footprintData()->model3D();
    if (footprintModel.uuid().isEmpty())
        return;

    auto model3DData = QSharedPointer<Model3DData>::create();
    model3DData->setUuid(footprintModel.uuid());
    model3DData->setName(footprintModel.name());
    model3DData->setTranslation(footprintModel.translation());
    model3DData->setRotation(footprintModel.rotation());
    data.setModel3DData(model3DData);
}

}  // namespace

/** @brief 返回指定导出类型的默认输出目录。 */
QString ExportWorkerHelpers::defaultOutputDir(const QString& subdir) {
    return QDir::currentPath() + QStringLiteral("/export/") + subdir;
}

/** @brief 创建并返回导出输出目录。 */
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

/** @brief 根据元器件编号构造导出文件路径。 */
QString ExportWorkerHelpers::buildFilePath(const QString& componentId,
                                           const QString& outputDir,
                                           const QString& fileExtension) {
    return outputDir + QStringLiteral("/") + componentId + fileExtension;
}

/** @brief 判断已有文件是否应因禁止覆盖而跳过。 */
bool ExportWorkerHelpers::shouldSkipExisting(const QString& filePath, const struct ExportOptions& options) {
    return QFile::exists(filePath) && !options.overwriteExistingFiles;
}

/** @brief 加载磁盘缓存中的元器件、CAD 数据和三维模型。 */
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
        // 保存原始 CAD JSON 数据用于 debug 导出
        cachedData->setCadJsonRaw(cadJsonData);
    }

    restoreModel3DFromFootprint(*cachedData);

    // 加载 3D 模型 OBJ 原始数据
    if (cachedData->model3DData() && !cachedData->model3DData()->uuid().isEmpty()) {
        const QByteArray model3DObjData = cache->loadModel3D(cachedData->model3DData()->uuid(), QStringLiteral("obj"));
        if (!model3DObjData.isEmpty()) {
            cachedData->setModel3DObjRaw(model3DObjData);
        }
    }

    const QByteArray datasheetData = cache->loadDatasheet(componentId);
    if (!datasheetData.isEmpty()) {
        cachedData->setDatasheetData(datasheetData);
    }

    return cachedData;
}

/** @brief 使用回退数据补齐目标元器件的缺失字段。 */
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

    // 合并 raw 数据用于 debug 导出
    if (target.cadJsonRaw().isEmpty() && !fallback->cadJsonRaw().isEmpty()) {
        target.setCadJsonRaw(fallback->cadJsonRaw());
    }
    if (target.cinfoJsonRaw().isEmpty() && !fallback->cinfoJsonRaw().isEmpty()) {
        target.setCinfoJsonRaw(fallback->cinfoJsonRaw());
    }
    if (target.model3DObjRaw().isEmpty() && !fallback->model3DObjRaw().isEmpty()) {
        target.setModel3DObjRaw(fallback->model3DObjRaw());
    }
}

/** @brief 根据各元器件状态重新计算导出阶段计数。 */
void ExportWorkerHelpers::recomputeTypeProgressCounts(ExportTypeProgress& progress) {
    progress.completedCount = 0;
    progress.successCount = 0;
    progress.failedCount = 0;
    progress.skippedCount = 0;
    progress.inProgressCount = 0;

    // 按每个元器件的最终状态累加阶段统计。
    for (auto it = progress.itemStatus.cbegin(); it != progress.itemStatus.cend(); ++it) {
        // 将当前状态映射到对应的完成、成功、失败或跳过计数。
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
