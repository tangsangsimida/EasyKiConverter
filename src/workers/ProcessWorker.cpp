#include "ProcessWorker.h"

#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaImporter.h"
#include "core/utils/NetworkUtils.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

ProcessWorker::ProcessWorker(QSharedPointer<ComponentExportStatus> status, QObject* parent)
    : QObject(parent), m_status(status), m_isAborted(0) {}

ProcessWorker::~ProcessWorker() {}

void ProcessWorker::run() {
    // 检查是否已被取消 (v3.0.5+)
    if (m_isAborted.loadRelaxed()) {
        m_status->processSuccess = false;
        m_status->processMessage = "Export cancelled";
        m_status->isCancelled = true;
        m_status->addDebugLog(QString("ProcessWorker cancelled for component: %1").arg(m_status->componentId));
        emit processCompleted(m_status);
        return;
    }

    // 启动计时器
    QElapsedTimer processTimer;
    processTimer.start();

    m_status->addDebugLog(QString("ProcessWorker started for component: %1").arg(m_status->componentId));

    // 创建导入器
    EasyedaImporter importer;

    // 解析组件信息
    if (!parseComponentInfo(*m_status)) {
        m_status->processDurationMs = processTimer.elapsed();
        m_status->processSuccess = false;
        m_status->processMessage = "Failed to parse component info";
        m_status->addDebugLog(
            QString("ERROR: Failed to parse component info, Duration: %1ms").arg(m_status->processDurationMs));
        emit processCompleted(m_status);
        return;
    }

    if (m_isAborted.loadRelaxed()) {
        m_status->processDurationMs = processTimer.elapsed();
        m_status->processSuccess = false;
        m_status->processMessage = "Export cancelled";
        m_status->isCancelled = true;
        m_status->addDebugLog(
            QString("ProcessWorker cancelled during processing for component: %1").arg(m_status->componentId));
        emit processCompleted(m_status);
        return;
    }

    // 解析CAD数据
    if (!parseCadData(*m_status)) {
        m_status->processDurationMs = processTimer.elapsed();
        m_status->processSuccess = false;
        m_status->processMessage = "Failed to parse CAD data";
        m_status->addDebugLog(
            QString("ERROR: Failed to parse CAD data, Duration: %1ms").arg(m_status->processDurationMs));
        emit processCompleted(m_status);
        return;
    }

    if (m_isAborted.loadRelaxed()) {
        m_status->processDurationMs = processTimer.elapsed();
        m_status->processSuccess = false;
        m_status->processMessage = "Export cancelled";
        m_status->isCancelled = true;
        m_status->addDebugLog(
            QString("ProcessWorker cancelled during processing for component: %1").arg(m_status->componentId));
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

    m_status->processDurationMs = processTimer.elapsed();
    m_status->processSuccess = true;
    m_status->processMessage = "Process completed successfully";
    m_status->addDebugLog(QString("ProcessWorker completed successfully for component: %1, Duration: %2ms")
                              .arg(m_status->componentId)
                              .arg(m_status->processDurationMs));

    // 内存优化：处理完成后立即清理中间数据（JSON缓冲、OBJ Raw等）
    // STEP 数据保留给 WriteWorker 使用
    m_status->clearIntermediateData();

    emit processCompleted(m_status);
}

bool ProcessWorker::parseComponentInfo(ComponentExportStatus& status) {
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

bool ProcessWorker::parseCadData(ComponentExportStatus& status) {
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

    // 调试：打印obj的结构
    status.addDebugLog("=== ProcessWorker CAD Data Structure ===");
    status.addDebugLog(QString("Top-level keys: %1").arg(obj.keys().join(", ")));
    if (obj.contains("dataStr")) {
        QJsonObject dataStr = obj["dataStr"].toObject();
        status.addDebugLog(QString("dataStr keys: %1").arg(dataStr.keys().join(", ")));
        if (dataStr.contains("shape")) {
            QJsonArray shapes = dataStr["shape"].toArray();
            status.addDebugLog(QString("dataStr.shape size: %1").arg(shapes.size()));
            if (!shapes.isEmpty()) {
                status.addDebugLog(QString("First shape: %1").arg(shapes[0].toString().left(100)));
            }
        } else {
            status.addDebugLog("WARNING: dataStr does NOT contain 'shape' field!");
        }
    } else {
        status.addDebugLog("WARNING: obj does NOT contain 'dataStr' field!");
    }
    status.addDebugLog("==========================================");

    // 使用EasyedaImporter导入符号和封装数据
    EasyedaImporter importer;

    // 导入符号数据
    status.symbolData = importer.importSymbolData(obj);

    // 严格检查: 如果启用了符号导出但导入失败
    if (!status.symbolData) {
        status.addDebugLog("ERROR: Symbol data import failed or missing in CAD data.");
        // 如果 symbolData 为空，说明解析失败。
        return false;
    }

    // 调试：打印符号数据的统计信息
    status.addDebugLog(QString("Symbol data imported successfully. Pins: %1").arg(status.symbolData->pins().size()));

    // 导入封装数据
    status.footprintData = importer.importFootprintData(obj);

    // 严格检查: 如果启用了封装导出但导入失败
    if (!status.footprintData || status.footprintData->info().name.isEmpty()) {
        status.addDebugLog("ERROR: Footprint data import failed or footprint name is empty.");
        return false;
    }

    status.addDebugLog(
        QString("Footprint data imported successfully - Name: %1").arg(status.footprintData->info().name));

    return true;
}

bool ProcessWorker::parse3DModelData(ComponentExportStatus& status) {
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

void ProcessWorker::abort() {
    m_isAborted.storeRelaxed(1);
    m_status->addDebugLog(QString("ProcessWorker abort requested for component: %1").arg(m_status->componentId));
}

}  // namespace EasyKiConverter
