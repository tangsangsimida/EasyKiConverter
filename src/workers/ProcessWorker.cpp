#include "ProcessWorker.h"
#include "src/core/easyeda/EasyedaImporter.h"
#include "src/core/easyeda/EasyedaApi.h"
#include "src/core/utils/NetworkUtils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace EasyKiConverter {

ProcessWorker::ProcessWorker(QSharedPointer<ComponentExportStatus> status, QObject *parent)
    : QObject(parent)
    , m_status(status)
{
}

ProcessWorker::~ProcessWorker()
{
}

void ProcessWorker::run()
{
    m_status->addDebugLog(QString("ProcessWorker started for component: %1").arg(m_status->componentId));

    // 创建导入器
    EasyedaImporter importer;

    // 解析组件信息
    if (!parseComponentInfo(*m_status)) {
        m_status->processSuccess = false;
        m_status->processMessage = "Failed to parse component info";
        m_status->addDebugLog("ERROR: Failed to parse component info");
        emit processCompleted(m_status);
        return;
    }

    // 解析CAD数据
    if (!parseCadData(*m_status)) {
        m_status->processSuccess = false;
        m_status->processMessage = "Failed to parse CAD data";
        m_status->addDebugLog("ERROR: Failed to parse CAD data");
        emit processCompleted(m_status);
        return;
    }

    // 解析3D模型数据（如果已下载）
    if (m_status->need3DModel && !m_status->model3DObjRaw.isEmpty()) {
        m_status->addDebugLog("Parsing 3D model data (already fetched by FetchWorker)");
        if (!parse3DModelData(*m_status)) {
            m_status->addDebugLog(QString("WARNING: Failed to parse 3D model data for: %1").arg(m_status->componentId));
            // 3D模型解析失败不影响整体流程
        }
    }

    m_status->processSuccess = true;
    m_status->processMessage = "Process completed successfully";
    m_status->addDebugLog(QString("ProcessWorker completed successfully for component: %1").arg(m_status->componentId));

    emit processCompleted(m_status);
}

bool ProcessWorker::parseComponentInfo(ComponentExportStatus &status)
{
    if (status.componentInfoRaw.isEmpty()) {
        status.componentData = QSharedPointer<ComponentData>::create();
        status.componentData->setLcscId(status.componentId);
        return true;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(status.componentInfoRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        status.addDebugLog(QString("ERROR: Failed to parse component info JSON: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject obj = doc.object();
    status.componentData = QSharedPointer<ComponentData>::create();
    status.componentData->setLcscId(status.componentId);

    if (obj.contains("name")) {
        status.componentData->setName(obj["name"].toString());
    }

    return true;
}

bool ProcessWorker::parseCadData(ComponentExportStatus &status)
{
    if (status.cadDataRaw.isEmpty()) {
        return true;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(status.cadDataRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        status.addDebugLog(QString("ERROR: Failed to parse CAD data JSON: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject rootObj = doc.object();

    // API 返回的数据在 result 字段中，需要提取出来
    QJsonObject obj;
    if (rootObj.contains("result") && rootObj["result"].isObject()) {
        obj = rootObj["result"].toObject();
        status.addDebugLog("Extracted CAD data from 'result' field");
    } else {
        // 如果没有 result 字段，直接使用根对象
        obj = rootObj;
    }

    // 使用EasyedaImporter导入符号和封装数据
    EasyedaImporter importer;

    // 导入符号数据
    status.symbolData = importer.importSymbolData(obj);

    // 导入封装数据
    status.footprintData = importer.importFootprintData(obj);

    return true;
}

bool ProcessWorker::parse3DModelData(ComponentExportStatus &status)
{
    if (status.model3DObjRaw.isEmpty()) {
        return true;
    }

    // 创建3D模型数据
    status.model3DData = QSharedPointer<Model3DData>::create();
    status.model3DData->setRawObj(QString::fromUtf8(status.model3DObjRaw));

    // 从 footprintData 的 model3D 中获取 UUID
    if (status.footprintData && !status.footprintData->model3D().uuid().isEmpty()) {
        status.model3DData->setUuid(status.footprintData->model3D().uuid());
        status.addDebugLog(QString("Using 3D model UUID from footprintData: %1").arg(status.model3DData->uuid()));
    }

    return true;
}

} // namespace EasyKiConverter