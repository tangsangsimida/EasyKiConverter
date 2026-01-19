#include "ProcessWorker.h"
#include "src/core/easyeda/EasyedaImporter.h"
#include "src/core/easyeda/EasyedaApi.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace EasyKiConverter {

ProcessWorker::ProcessWorker(const ComponentExportStatus &status, QObject *parent)
    : QObject(parent)
    , m_status(status)
{
}

ProcessWorker::~ProcessWorker()
{
}

void ProcessWorker::run()
{
    qDebug() << "ProcessWorker started for component:" << m_status.componentId;

    // 创建导入器
    EasyedaImporter importer;

    // 解析组件信息
    if (!parseComponentInfo(m_status)) {
        m_status.processSuccess = false;
        m_status.processMessage = "Failed to parse component info";
        emit processCompleted(m_status);
        return;
    }

    // 解析CAD数据
    if (!parseCadData(m_status)) {
        m_status.processSuccess = false;
        m_status.processMessage = "Failed to parse CAD data";
        emit processCompleted(m_status);
        return;
    }

    // 解析3D模型数据（如果需要）
    if (m_status.need3DModel && !m_status.model3DObjRaw.isEmpty()) {
        if (!parse3DModelData(m_status)) {
            m_status.processSuccess = false;
            m_status.processMessage = "Failed to parse 3D model data";
            emit processCompleted(m_status);
            return;
        }
    }

    m_status.processSuccess = true;
    m_status.processMessage = "Process completed successfully";
    qDebug() << "ProcessWorker completed successfully for component:" << m_status.componentId;

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
        qWarning() << "Failed to parse component info JSON:" << parseError.errorString();
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
        qWarning() << "Failed to parse CAD data JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject obj = doc.object();

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

    // 尝试从CAD数据中提取UUID
    if (!status.cadDataRaw.isEmpty()) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(status.cadDataRaw, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            QJsonObject obj = doc.object();
            if (obj.contains("c_para") && obj["c_para"].isObject()) {
                QJsonObject cPara = obj["c_para"].toObject();
                if (cPara.contains("uuid")) {
                    status.model3DData->setUuid(cPara["uuid"].toString());
                }
            }
        }
    }

    return true;
}

} // namespace EasyKiConverter