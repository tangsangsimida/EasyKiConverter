#include "CadDataLoader.h"

#include "ConfigService.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"
#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

namespace {
const QString EASYEDA_COMPONENT_API_ENDPOINT =
    QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51");
}  // namespace

CadParseResult CadDataLoader::parseCadPayload(const QString& componentId, const QJsonObject& rawData) {
    CadParseResult parsed;
    parsed.componentId = componentId;

    QJsonObject resultData = rawData;
    if (rawData.contains("result")) {
        resultData = rawData.value("result").toObject();
    }

    if (resultData.isEmpty()) {
        parsed.errorMessage = QStringLiteral("Empty CAD data");
        return parsed;
    }

    ComponentData componentData;
    componentData.setLcscId(componentId);
    componentData.setName(resultData.value("title").toString());
    componentData.setPackage(resultData.value("package").toString());
    componentData.setManufacturer(resultData.value("manufacturer").toString());
    componentData.setDatasheet(resultData.value("datasheet").toString());

    EasyedaSymbolImporter symbolImporter;
    QSharedPointer<SymbolData> symbolData = symbolImporter.importSymbolData(resultData);
    if (!symbolData) {
        parsed.errorMessage = QStringLiteral("Failed to parse Symbol data from EasyEDA JSON");
        return parsed;
    }
    componentData.setSymbolData(symbolData);

    if (!symbolData->info().thumb.isEmpty()) {
        const QString thumbUrl = UrlUtils::normalizePreviewImageUrl(symbolData->info().thumb);
        componentData.setPreviewImages(QStringList() << thumbUrl);
    }

    if (!symbolData->info().datasheet.isEmpty() && componentData.datasheet().isEmpty()) {
        const QString datasheetUrl = UrlUtils::normalizePreviewImageUrl(symbolData->info().datasheet);
        componentData.setDatasheet(datasheetUrl);
    }

    EasyedaFootprintImporter footprintImporter;
    QSharedPointer<FootprintData> footprintData = footprintImporter.importFootprintData(resultData);
    if (!footprintData) {
        parsed.errorMessage =
            QStringLiteral("Failed to parse Footprint data from EasyEDA JSON (Library might be corrupted)");
        return parsed;
    }
    componentData.setFootprintData(footprintData);

    QString modelUuid = footprintData->model3D().uuid();
    if (modelUuid.isEmpty() && resultData.contains("dataStr")) {
        const QJsonObject dataStr = resultData.value("dataStr").toObject();
        const QJsonArray shapes = dataStr.value("shape").toArray();
        for (const QJsonValue& shapeVal : shapes) {
            const QString shapeStr = shapeVal.toString();
            if (!shapeStr.startsWith(QStringLiteral("SVGNODE~"))) {
                continue;
            }

            const int tildeIndex = shapeStr.indexOf('~');
            if (tildeIndex < 0) {
                continue;
            }

            const QJsonDocument nodeDoc = QJsonDocument::fromJson(shapeStr.mid(tildeIndex + 1).toUtf8());
            const QJsonObject attrs = nodeDoc.object().value("attrs").toObject();
            if (attrs.value("c_etype").toString() == QLatin1String("outline3D")) {
                modelUuid = attrs.value("uuid").toString();
                if (!modelUuid.isEmpty()) {
                    break;
                }
            }
        }

        if (modelUuid.isEmpty()) {
            modelUuid = dataStr.value("head").toObject().value("uuid_3d").toString();
        }
    }

    if (!modelUuid.isEmpty()) {
        auto model3DData = QSharedPointer<Model3DData>::create();
        model3DData->setUuid(modelUuid);
        model3DData->setName(footprintData->model3D().name());
        model3DData->setTranslation(footprintData->model3D().translation());
        model3DData->setRotation(footprintData->model3D().rotation());
        componentData.setModel3DData(model3DData);
    }

    parsed.resultData = resultData;
    parsed.componentData = componentData;
    parsed.success = true;
    return parsed;
}

CadFetchTaskResult CadDataLoader::fetchAndParseCadData(const QString& componentId) {
    CadFetchTaskResult taskResult;
    taskResult.componentId = componentId;

    const RequestProfile profile = RequestProfiles::cadData();
    const RetryPolicy policy = RetryPolicy::fromProfile(profile, ConfigService::instance()->getWeakNetworkSupport());
    const QUrl url(EASYEDA_COMPONENT_API_ENDPOINT.arg(componentId));
    const NetworkResult networkResult = NetworkClient::instance().get(url, ResourceType::CadData, policy);
    if (!networkResult.success) {
        taskResult.errorMessage =
            networkResult.error.isEmpty() ? QStringLiteral("CAD request failed") : networkResult.error;
        return taskResult;
    }

    QJsonParseError parseError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(networkResult.data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        taskResult.errorMessage = QStringLiteral("Failed to parse CAD response JSON");
        return taskResult;
    }

    const QJsonObject root = jsonDoc.object();

    // 检查业务层错误：success == false
    if (root.contains("success") && !root.value("success").toBool()) {
        const int code = root.value("code").toInt();
        const QString message = root.value("message").toString();
        if (code == 404) {
            taskResult.errorMessage = QStringLiteral("元器件不存在（404）");
        } else {
            taskResult.errorMessage = message.isEmpty() ? QStringLiteral("API returned success=false") : message;
        }
        return taskResult;
    }

    // 检查 result 字段是否缺失或为 null
    if (!root.contains("result") || root.value("result").isNull()) {
        taskResult.errorMessage = QStringLiteral("API response missing result field");
        return taskResult;
    }

    // 检查 result 是否为空对象
    if (root.value("result").isObject() && root.value("result").toObject().isEmpty()) {
        taskResult.errorMessage = QStringLiteral("Empty CAD data");
        return taskResult;
    }

    taskResult.parsed = parseCadPayload(componentId, root);
    if (!taskResult.parsed.success) {
        taskResult.errorMessage = taskResult.parsed.errorMessage;
        return taskResult;
    }

    taskResult.success = true;
    return taskResult;
}

}  // namespace EasyKiConverter
