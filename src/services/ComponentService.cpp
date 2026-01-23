#include "ComponentService.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaImporter.h"

namespace EasyKiConverter
{

    ComponentService::ComponentService(QObject *parent)
        : QObject(parent), m_api(new EasyedaApi(this)), m_importer(new EasyedaImporter(this)), m_currentComponentId(), m_hasDownloadedWrl(false), m_parallelTotalCount(0), m_parallelCompletedCount(0), m_parallelFetching(false)
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

        // 暂时存储当前请求的元件ID�?D模型标志
        m_currentComponentId = componentId;
        m_fetch3DModel = fetch3DModel;

        // 首先获取 CAD 数据（包含符号和封装信息�?
        m_api->fetchCadData(componentId);
    }

    void ComponentService::handleComponentInfoFetched(const QJsonObject &data)
    {
        qDebug() << "Component info fetched:" << data.keys();

        // 解析组件信息
        ComponentData componentData;
        componentData.setLcscId(m_currentComponentId);

        // 从响应中提取基本信息
        if (data.contains("result"))
        {
            QJsonObject result = data["result"].toObject();

            if (result.contains("title"))
            {
                componentData.setName(result["title"].toString());
            }
            if (result.contains("package"))
            {
                componentData.setPackage(result["package"].toString());
            }
            if (result.contains("manufacturer"))
            {
                componentData.setManufacturer(result["manufacturer"].toString());
            }
            if (result.contains("datasheet"))
            {
                componentData.setDatasheet(result["datasheet"].toString());
            }
        }

        emit componentInfoReady(m_currentComponentId, componentData);
    }

    void ComponentService::handleCadDataFetched(const QJsonObject &data)
    {
        // 从数据中提取 LCSC ID
        QString lcscId;
        if (data.contains("lcscId"))
        {
            lcscId = data["lcscId"].toString();
        }
        else
        {
            // 如果没有 lcscId 字段，尝试从 lcsc.szlcsc.number 中提�?
            if (data.contains("lcsc"))
            {
                QJsonObject lcsc = data["lcsc"].toObject();
                if (lcsc.contains("number"))
                {
                    lcscId = lcsc["number"].toString();
                }
            }
        }

        if (lcscId.isEmpty())
        {
            qWarning() << "Cannot extract LCSC ID from CAD data";
            return;
        }

        qDebug() << "CAD data fetched for:" << lcscId;

        // 临时保存当前的组�?ID
        QString savedComponentId = m_currentComponentId;
        m_currentComponentId = lcscId;

        // 提取 result 数据
        QJsonObject resultData;
        if (data.contains("result"))
        {
            resultData = data["result"].toObject();
        }
        else
        {
            // 直接使用 data
            resultData = data;
        }

        if (resultData.isEmpty())
        {
            emit fetchError(lcscId, "Empty CAD data");
            m_currentComponentId = savedComponentId;
            return;
        }

        // 调试：打印resultData的结�?
        qDebug() << "=== CAD Data Structure ===";
        qDebug() << "Top-level keys:" << resultData.keys();
        if (resultData.contains("dataStr"))
        {
            QJsonObject dataStr = resultData["dataStr"].toObject();
            qDebug() << "dataStr keys:" << dataStr.keys();
            if (dataStr.contains("shape"))
            {
                QJsonArray shapes = dataStr["shape"].toArray();
                qDebug() << "dataStr.shape size:" << shapes.size();
                if (!shapes.isEmpty())
                {
                    qDebug() << "First shape:" << shapes[0].toString().left(100);
                }
            }
            else
            {
                qDebug() << "WARNING: dataStr does NOT contain 'shape' field!";
            }
        }
        else
        {
            qDebug() << "WARNING: resultData does NOT contain 'dataStr' field!";
        }
        qDebug() << "===========================";

        // 创建 ComponentData 对象
        ComponentData componentData;
        componentData.setLcscId(lcscId);

        // 提取基本信息
        if (resultData.contains("title"))
        {
            componentData.setName(resultData["title"].toString());
        }
        if (resultData.contains("package"))
        {
            componentData.setPackage(resultData["package"].toString());
        }
        if (resultData.contains("manufacturer"))
        {
            componentData.setManufacturer(resultData["manufacturer"].toString());
        }
        if (resultData.contains("datasheet"))
        {
            componentData.setDatasheet(resultData["datasheet"].toString());
        }

        // 导入符号数据
        QSharedPointer<SymbolData> symbolData = m_importer->importSymbolData(resultData);
        if (symbolData)
        {
            componentData.setSymbolData(symbolData);
            qDebug() << "Symbol imported successfully - Name:" << symbolData->info().name;
        }
        else
        {
            qWarning() << "Failed to import symbol data for:" << m_currentComponentId;
        }

        // 导入封装数据
        QSharedPointer<FootprintData> footprintData = m_importer->importFootprintData(resultData);
        if (footprintData)
        {
            componentData.setFootprintData(footprintData);
            qDebug() << "Footprint imported successfully - Name:" << footprintData->info().name;
        }
        else
        {
            qWarning() << "Failed to import footprint data for:" << m_currentComponentId;
        }

        // 检查是否需要获�?3D 模型
        if (m_fetch3DModel && footprintData)
        {
            // 检查封装数据中是否包含 3D 模型 UUID
            QString modelUuid = footprintData->model3D().uuid();

            // 如果封装数据中没�?UUID，尝试从 head.uuid_3d 字段中提�?
            if (modelUuid.isEmpty() && resultData.contains("head"))
            {
                QJsonObject head = resultData["head"].toObject();
                if (head.contains("uuid_3d"))
                {
                    modelUuid = head["uuid_3d"].toString();
                }
            }

            if (!modelUuid.isEmpty())
            {
                qDebug() << "Fetching 3D model with UUID:" << modelUuid;

                // 创建 Model3DData 对象
                QSharedPointer<Model3DData> model3DData(new Model3DData());
                model3DData->setUuid(modelUuid);

                // 如果封装数据中有 3D 模型信息，复制平移和旋转
                if (!footprintData->model3D().uuid().isEmpty())
                {
                    model3DData->setName(footprintData->model3D().name());
                    model3DData->setTranslation(footprintData->model3D().translation());
                    model3DData->setRotation(footprintData->model3D().rotation());
                }

                componentData.setModel3DData(model3DData);

                // 在并行模式下，使�?m_fetchingComponents 存储待处理的组件数据
                if (m_parallelFetching)
                {
                    FetchingComponent fetchingComponent;
                    fetchingComponent.componentId = m_currentComponentId;
                    fetchingComponent.data = componentData;
                    fetchingComponent.hasComponentInfo = true;
                    fetchingComponent.hasCadData = true;
                    fetchingComponent.hasObjData = false;
                    fetchingComponent.hasStepData = false;
                    m_fetchingComponents[m_currentComponentId] = fetchingComponent;
                }
                else
                {
                    // 串行模式下的处理
                    m_pendingComponentData = componentData;
                    m_pendingModelUuid = modelUuid;
                    m_hasDownloadedWrl = false;
                }

                // 获取 WRL 格式�?3D 模型
                m_api->fetch3DModelObj(modelUuid);
                return; // 等待 3D 模型数据
            }
            else
            {
                qDebug() << "No 3D model UUID found for:" << m_currentComponentId;
            }
        }

        // 不需�?3D 模型或没有找�?UUID，直接发送完成信�?
        emit cadDataReady(m_currentComponentId, componentData);

        // 如果在并行模式下，处理并行数据收�?
        if (m_parallelFetching)
        {
            handleParallelDataCollected(m_currentComponentId, componentData);
        }

        // 恢复组件 ID
        m_currentComponentId = savedComponentId;
    }

    void ComponentService::handleModel3DFetched(const QString &uuid, const QByteArray &data)
    {
        qDebug() << "3D model data fetched for UUID:" << uuid << "Size:" << data.size();

        // 在并行模式下，查找对应的组件
        if (m_parallelFetching)
        {
            // 在并行模式下，查找对�?UUID 的组�?
            for (auto it = m_fetchingComponents.begin(); it != m_fetchingComponents.end(); ++it)
            {
                if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == uuid)
                {
                    QString componentId = it.key();
                    FetchingComponent &fetchingComponent = it.value();

                    if (!fetchingComponent.hasObjData)
                    {
                        // 这是 WRL 格式�?3D 模型
                        fetchingComponent.data.model3DData()->setRawObj(QString::fromUtf8(data));
                        fetchingComponent.hasObjData = true;
                        qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                        // 继续下载 STEP 格式�?3D 模型
                        qDebug() << "Fetching STEP model with UUID:" << uuid;
                        m_api->fetch3DModelStep(uuid);
                    }
                    else
                    {
                        // 这是 STEP 格式�?3D 模型
                        fetchingComponent.data.model3DData()->setStep(data);
                        fetchingComponent.hasStepData = true;
                        qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                        // 发送完成信�?
                        emit cadDataReady(componentId, fetchingComponent.data);

                        // 处理并行数据收集
                        handleParallelDataCollected(componentId, fetchingComponent.data);

                        // 从待处理列表中移�?
                        m_fetchingComponents.remove(componentId);
                    }
                    return;
                }
            }
            qWarning() << "Received 3D model data for unexpected UUID:" << uuid;
        }
        else
        {
            // 串行模式下的处理
            if (m_pendingComponentData.model3DData() && m_pendingComponentData.model3DData()->uuid() == uuid)
            {
                if (!m_hasDownloadedWrl)
                {
                    // 这是 WRL 格式�?3D 模型
                    m_pendingComponentData.model3DData()->setRawObj(QString::fromUtf8(data));
                    qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                    // 标记已经下载�?WRL 格式
                    m_hasDownloadedWrl = true;

                    // 继续下载 STEP 格式�?3D 模型
                    qDebug() << "Fetching STEP model with UUID:" << uuid;
                    m_api->fetch3DModelStep(uuid);
                }
                else
                {
                    // 这是 STEP 格式�?3D 模型
                    m_pendingComponentData.model3DData()->setStep(data);
                    qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                    // 发送完成信�?
                    emit cadDataReady(m_currentComponentId, m_pendingComponentData);

                    // 清空待处理数�?
                    m_pendingComponentData = ComponentData();
                    m_pendingModelUuid.clear();
                    m_hasDownloadedWrl = false;
                }
            }
            else
            {
                qWarning() << "Received 3D model data for unexpected UUID:" << uuid;
            }
        }
    }

    void ComponentService::handleFetchError(const QString &errorMessage)
    {
        qDebug() << "Fetch error:" << errorMessage;
        emit fetchError(m_currentComponentId, errorMessage);

        // 如果在并行模式下，处理并行错�?
        if (m_parallelFetching)
        {
            handleParallelFetchError(m_currentComponentId, errorMessage);
        }
    }

    void ComponentService::setOutputPath(const QString &path)
    {
        m_outputPath = path;
    }

    QString ComponentService::getOutputPath() const
    {
        return m_outputPath;
    }

    void ComponentService::fetchMultipleComponentsData(const QStringList &componentIds, bool fetch3DModel)
    {
        qDebug() << "Fetching data for" << componentIds.size() << "components in parallel";

        // 初始化并行数据收集状�?
        m_parallelCollectedData.clear();
        m_parallelFetchingStatus.clear();
        m_parallelPendingComponents = componentIds;
        m_parallelTotalCount = componentIds.size();
        m_parallelCompletedCount = 0;
        m_parallelFetching = true;
        m_fetch3DModel = fetch3DModel;

        // 为每个元件启动数据收�?
        for (const QString &componentId : componentIds)
        {
            m_parallelFetchingStatus[componentId] = true;
            fetchComponentData(componentId, fetch3DModel);
        }
    }

    void ComponentService::handleParallelDataCollected(const QString &componentId, const ComponentData &data)
    {
        qDebug() << "Parallel data collected for:" << componentId;

        // 保存收集到的数据
        m_parallelCollectedData[componentId] = data;
        m_parallelCompletedCount++;

        // 更新状�?
        m_parallelFetchingStatus[componentId] = false;

        // 检查是否所有元件都已收集完�?
        if (m_parallelCompletedCount >= m_parallelTotalCount)
        {
            qDebug() << "All components data collected in parallel:" << m_parallelCollectedData.size();

            // 发送完成信�?
            QList<ComponentData> allData = m_parallelCollectedData.values();
            emit allComponentsDataCollected(allData);

            // 重置状�?
            m_parallelFetching = false;
            m_parallelCollectedData.clear();
            m_parallelFetchingStatus.clear();
            m_parallelPendingComponents.clear();
        }
    }

    void ComponentService::handleParallelFetchError(const QString &componentId, const QString &error)
    {
        qDebug() << "Parallel fetch error for:" << componentId << error;

        // 更新状�?
        m_parallelFetchingStatus[componentId] = false;
        m_parallelCompletedCount++;

        // 检查是否所有元件都已处理完�?
        if (m_parallelCompletedCount >= m_parallelTotalCount)
        {
            qDebug() << "All components data collected (with errors):" << m_parallelCollectedData.size();

            // 发送完成信�?
            QList<ComponentData> allData = m_parallelCollectedData.values();
            emit allComponentsDataCollected(allData);

            // 重置状�?
            m_parallelFetching = false;
            m_parallelCollectedData.clear();
            m_parallelFetchingStatus.clear();
            m_parallelPendingComponents.clear();
        }
    }

} // namespace EasyKiConverter
