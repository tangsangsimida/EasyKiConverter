#include "ComponentCacheLoadWorker.h"

#include "ComponentCacheService.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

/** @brief 从封装模型复制三维模型的完整元数据。 */
void ComponentCacheLoadWorker::restoreModel3DFromFootprint(ComponentData& component,
                                                           const QSharedPointer<FootprintData>& footprint) {
    if (!footprint) {
        return;
    }

    const Model3DData footprintModel = footprint->model3D();
    if (footprintModel.uuid().isEmpty()) {
        return;
    }
    if (component.model3DData() && !component.model3DData()->uuid().isEmpty()) {
        return;
    }

    auto model3DData = QSharedPointer<Model3DData>::create();
    model3DData->setUuid(footprintModel.uuid());
    model3DData->setName(footprintModel.name());
    model3DData->setTranslation(footprintModel.translation());
    model3DData->setRotation(footprintModel.rotation());
    component.setModel3DData(model3DData);
}

/** @brief 在后台线程读取缓存并完成可并行执行的解析工作。 */
ComponentCacheLoadResult ComponentCacheLoadWorker::load(const QString& componentId,
                                                        bool fetch3DModel,
                                                        ComponentCacheService* cache) {
    ComponentCacheLoadResult result;
    result.componentId = componentId;

    QSharedPointer<ComponentData> cachedData = cache->loadComponentData(componentId);
    if (!cachedData) {
        return result;
    }

    const QByteArray cadJsonData = cache->loadCadDataJson(componentId);
    if (!cadJsonData.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonData, &parseError);
        if (parseError.error == QJsonParseError::NoError && cadDoc.isObject()) {
            const QJsonObject cadDataObj = cadDoc.object();
            EasyedaSymbolImporter symbolImporter;
            result.symbolData = symbolImporter.importSymbolData(cadDataObj);
            EasyedaFootprintImporter footprintImporter;
            result.footprintData = footprintImporter.importFootprintData(cadDataObj);
        }
    }

    const QJsonObject metadata = cache->loadMetadata(componentId);
    if (metadata.contains(QStringLiteral("model3duuid"))) {
        const QString uuid = metadata.value(QStringLiteral("model3duuid")).toString();
        if (!uuid.isEmpty()) {
            if (!cachedData->model3DData()) {
                auto model3DData = QSharedPointer<Model3DData>::create();
                model3DData->setUuid(uuid);
                cachedData->setModel3DData(model3DData);
            } else if (cachedData->model3DData()->uuid().isEmpty()) {
                cachedData->model3DData()->setUuid(uuid);
            }
        }
    }

    restoreModel3DFromFootprint(*cachedData, result.footprintData);
    if (!result.symbolData || !result.footprintData) {
        return result;
    }

    result.cachedData = cachedData;
    result.success = true;
    result.encodedPreviewImages = QStringList(3);
    for (int i = 0; i < 3; ++i) {
        const QByteArray imageData = cache->loadPreviewImage(componentId, i);
        if (!imageData.isEmpty()) {
            result.previewImageData.append({i, imageData});
            result.encodedPreviewImages[i] = QString::fromLatin1(imageData.toBase64());
        }
    }

    result.datasheetData = cache->loadDatasheet(componentId);
    if (fetch3DModel && cachedData->model3DData() && !cachedData->model3DData()->uuid().isEmpty()) {
        const QByteArray objData = cache->loadModel3D(cachedData->model3DData()->uuid(), QStringLiteral("obj"));
        if (!objData.isEmpty()) {
            cachedData->setModel3DObjRaw(objData);
        }
    }

    return result;
}

}  // namespace EasyKiConverter
