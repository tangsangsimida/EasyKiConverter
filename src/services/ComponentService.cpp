#include "ComponentService.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "src/core/easyeda/EasyedaApi.h"
#include "src/core/easyeda/EasyedaImporter.h"

namespace EasyKiConverter
{

ComponentService::ComponentService(QObject *parent)
    : QObject(parent)
    , m_api(new EasyedaApi(this))
    , m_importer(new EasyedaImporter(this))
    , m_currentComponentId()
{
    // 连接 API 信号
    connect(m_api, &EasyedaApi::componentInfoFetched, this, &ComponentService::handleComponentInfoFetched);
    connect(m_api, &EasyedaApi::cadDataFetched, this, &ComponentService::handleCadDataFetched);
    connect(m_api, &EasyedaApi::model3DFetched, this, &ComponentService::handleModel3DFetched);
    connect(m_api, &EasyedaApi::fetchError, this, &ComponentService::handleFetchError);
}

ComponentService::~ComponentService()
{
}

void ComponentService::fetchComponentData(const QString &componentId, bool fetch3DModel)
{
    qDebug() << "Fetching component data for:" << componentId << "Fetch 3D:" << fetch3DModel;
    
    m_currentComponentId = componentId;
    m_fetch3DModel = fetch3DModel;
    
    // 首先获取 CAD 数据（包含符号和封装信息）
    m_api->fetchCadData(componentId);
}

void ComponentService::handleComponentInfoFetched(const QJsonObject &data)
{
    qDebug() << "Component info fetched:" << data.keys();
    
    // 解析组件信息
    ComponentData componentData;
    componentData.setLcscId(m_currentComponentId);
    
    // 从响应中提取基本信息
    if (data.contains("result")) {
        QJsonObject result = data["result"].toObject();
        
        if (result.contains("title")) {
            componentData.setName(result["title"].toString());
        }
        if (result.contains("package")) {
            componentData.setPackage(result["package"].toString());
        }
        if (result.contains("manufacturer")) {
            componentData.setManufacturer(result["manufacturer"].toString());
        }
        if (result.contains("datasheet")) {
            componentData.setDatasheet(result["datasheet"].toString());
        }
    }
    
    emit componentInfoReady(m_currentComponentId, componentData);
}

void ComponentService::handleCadDataFetched(const QJsonObject &data)
{
    qDebug() << "CAD data fetched for:" << m_currentComponentId;
    
    // 检查响应结果
    QJsonObject resultData;
    if (data.contains("result")) {
        resultData = data["result"].toObject();
    } else {
        // 直接使用 data
        resultData = data;
    }
    
    if (resultData.isEmpty()) {
        emit fetchError(m_currentComponentId, "Empty CAD data");
        return;
    }
    
    // 创建 ComponentData 对象
    ComponentData componentData;
    componentData.setLcscId(m_currentComponentId);
    
    // 提取基本信息
    if (resultData.contains("title")) {
        componentData.setName(resultData["title"].toString());
    }
    if (resultData.contains("package")) {
        componentData.setPackage(resultData["package"].toString());
    }
    if (resultData.contains("manufacturer")) {
        componentData.setManufacturer(resultData["manufacturer"].toString());
    }
    if (resultData.contains("datasheet")) {
        componentData.setDatasheet(resultData["datasheet"].toString());
    }
    
    // 导入符号数据
    QSharedPointer<SymbolData> symbolData = m_importer->importSymbolData(resultData);
    if (symbolData) {
        componentData.setSymbolData(symbolData);
        qDebug() << "Symbol imported successfully - Name:" << symbolData->info().name;
    } else {
        qWarning() << "Failed to import symbol data for:" << m_currentComponentId;
    }
    
    // 导入封装数据
    QSharedPointer<FootprintData> footprintData = m_importer->importFootprintData(resultData);
    if (footprintData) {
        componentData.setFootprintData(footprintData);
        qDebug() << "Footprint imported successfully - Name:" << footprintData->info().name;
    } else {
        qWarning() << "Failed to import footprint data for:" << m_currentComponentId;
    }
    
    // 检查是否需要获取 3D 模型
    if (m_fetch3DModel && footprintData && footprintData->model3D().uuid().isEmpty()) {
        // 从 CAD 数据中提取 3D 模型 UUID
        if (resultData.contains("head")) {
            QJsonObject head = resultData["head"].toObject();
            if (head.contains("uuid_3d")) {
                QString uuid = head["uuid_3d"].toString();
                if (!uuid.isEmpty()) {
                    qDebug() << "Fetching 3D model with UUID:" << uuid;
                    // 更新 Model3DData 的 UUID
                    QSharedPointer<Model3DData> model3DData(new Model3DData());
                    model3DData->setUuid(uuid);
                    componentData.setModel3DData(model3DData);
                    
                    // 保存当前组件数据，等待 3D 模型数据
                    m_pendingComponentData = componentData;
                    
                    // 获取 OBJ 格式的 3D 模型
                    m_api->fetch3DModelObj(uuid);
                    return; // 等待 3D 模型数据
                }
            }
        }
    }
    
    // 不需要 3D 模型或没有找到 UUID，直接发送完成信号
    emit cadDataReady(m_currentComponentId, componentData);
}

void ComponentService::handleModel3DFetched(const QString &uuid, const QByteArray &data)
{
    qDebug() << "3D model data fetched for UUID:" << uuid << "Size:" << data.size();
    
    // 更新待处理的组件数据
    if (m_pendingComponentData.model3DData() && m_pendingComponentData.model3DData()->uuid() == uuid) {
        // 保存 OBJ 数据
        m_pendingComponentData.model3DData()->setRawObj(QString::fromUtf8(data));
        qDebug() << "OBJ data saved for:" << uuid;
        
        // 发送完成信号
        emit cadDataReady(m_currentComponentId, m_pendingComponentData);
        
        // 清空待处理数据
        m_pendingComponentData = ComponentData();
    } else {
        qWarning() << "Received 3D model data for unexpected UUID:" << uuid;
    }
}

void ComponentService::handleFetchError(const QString &errorMessage)
{
    qDebug() << "Fetch error:" << errorMessage;
    emit fetchError(m_currentComponentId, errorMessage);
}

void ComponentService::setOutputPath(const QString &path)
{
    m_outputPath = path;
}

QString ComponentService::getOutputPath() const
{
    return m_outputPath;
}

} // namespace EasyKiConverter