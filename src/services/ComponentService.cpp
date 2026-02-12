#include "ComponentService.h"

#include "BomParser.h"
#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaImporter.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QQueue>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

namespace EasyKiConverter {

ComponentService::ComponentService(QObject* parent)
    : QObject(parent)
    , m_api(new EasyedaApi(this))
    , m_importer(new EasyedaImporter(this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentComponentId()
    , m_hasDownloadedWrl(false)
    , m_parallelTotalCount(0)
    , m_parallelCompletedCount(0)
    , m_parallelFetching(false)
    , m_imageService(new LcscImageService(this)) {
    // 连接图片服务信号
    connect(m_imageService, &LcscImageService::imageReady, this, &ComponentService::handleImageReady);
    // 连接 API 信号
    connect(m_api, &EasyedaApi::componentInfoFetched, this, &ComponentService::handleComponentInfoFetched);
    connect(m_api, &EasyedaApi::cadDataFetched, this, &ComponentService::handleCadDataFetched);
    connect(m_api, &EasyedaApi::model3DFetched, this, &ComponentService::handleModel3DFetched);
    connect(m_api, qOverload<const QString&>(&EasyedaApi::fetchError), this, &ComponentService::handleFetchError);
    connect(m_api,
            qOverload<const QString&, const QString&>(&EasyedaApi::fetchError),
            this,
            &ComponentService::handleFetchErrorWithId);
}

ComponentService::~ComponentService() {}

void ComponentService::fetchComponentData(const QString& componentId, bool fetch3DModel) {
    fetchComponentDataInternal(componentId, fetch3DModel);
}

void ComponentService::fetchComponentDataInternal(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data (internal) for:" << componentId << "Fetch 3D:" << fetch3DModel;

    // 确保 componentId 格式统一（大写）
    QString normalizedId = componentId.toUpper();
    m_currentComponentId = normalizedId;

    // 获取或创建 FetchingComponent 条目
    FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
    fetchingComponent.componentId = normalizedId;
    fetchingComponent.fetch3DModel = fetch3DModel;

    // 初始化/重置状态标志
    fetchingComponent.hasComponentInfo = false;
    fetchingComponent.hasCadData = false;
    fetchingComponent.hasObjData = false;
    fetchingComponent.hasStepData = false;
    fetchingComponent.errorMessage.clear();

    // 首先获取 CAD 数据（包含符号和封装信息）
    // CAD 数据是后续流程的基础
    m_api->fetchCadData(normalizedId);
}

void ComponentService::fetchLcscPreviewImage(const QString& componentId) {
    m_imageService->fetchPreviewImage(componentId);
}

void ComponentService::handleImageReady(const QString& componentId, const QString& imagePath) {
    QImage image(imagePath);
    if (!image.isNull()) {
        emit previewImageReady(componentId, image);
    }
}

void ComponentService::handleComponentInfoFetched(const QJsonObject& data) {
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

void ComponentService::handleCadDataFetched(const QJsonObject& data) {
    // 从数据中提取 LCSC ID
    QString lcscId;
    if (data.contains("lcscId")) {
        lcscId = data["lcscId"].toString();
    } else {
        // 如果没有 lcscId 字段，尝试从 lcsc.szlcsc.number 中提取
        if (data.contains("lcsc")) {
            QJsonObject lcsc = data["lcsc"].toObject();
            if (lcsc.contains("number")) {
                lcscId = lcsc["number"].toString();
            }
        }
    }

    if (lcscId.isEmpty()) {
        qWarning() << "Cannot extract LCSC ID from CAD data";
        return;
    }

    qDebug() << "CAD data fetched for:" << lcscId;

    // 临时保存当前的组件ID
    QString savedComponentId = m_currentComponentId;
    m_currentComponentId = lcscId;

    // 获取 fetch3DModel 配置
    bool need3DModel = true;  // 默认值
    if (m_fetchingComponents.contains(lcscId)) {
        need3DModel = m_fetchingComponents[lcscId].fetch3DModel;
    } else {
        // 如果找不到配置（可能是旧的串行逻辑遗留，或者 m_fetchingComponents 被清除），
        // 尝试回退到 m_fetch3DModel，但这在并行下不可靠。
        // 不过由于我们在 fetchComponentDataInternal 中强制设置了 map，这里应该能找到。
        qWarning() << "Fetching component info not found in map for:" << lcscId << "Using default fetch3DModel=true";
    }

    // 提取 result 数据
    QJsonObject resultData;
    if (data.contains("result")) {
        resultData = data["result"].toObject();
    } else {
        // 直接使用 data
        resultData = data;
    }

    if (resultData.isEmpty()) {
        emit fetchError(lcscId, "Empty CAD data");
        m_currentComponentId = savedComponentId;
        return;
    }

    // 调试：打印resultData的结构
    qDebug() << "=== CAD Data Structure ===";
    qDebug() << "Top-level keys:" << resultData.keys();
    if (resultData.contains("dataStr")) {
        QJsonObject dataStr = resultData["dataStr"].toObject();
        qDebug() << "dataStr keys:" << dataStr.keys();
        if (dataStr.contains("shape")) {
            QJsonArray shapes = dataStr["shape"].toArray();
            qDebug() << "dataStr.shape size:" << shapes.size();
            if (!shapes.isEmpty()) {
                qDebug() << "First shape:" << shapes[0].toString().left(100);
            }
        } else {
            qDebug() << "WARNING: dataStr does NOT contain 'shape' field!";
        }
    } else {
        qDebug() << "WARNING: resultData does NOT contain 'dataStr' field!";
    }
    qDebug() << "===========================";

    // 创建 ComponentData 对象
    ComponentData componentData;
    componentData.setLcscId(lcscId);

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

    // 检查是否需要获取3D 模型
    // 使用本地变量 need3DModel 而不是成员变量 m_fetch3DModel
    if (need3DModel && footprintData) {
        // 检查封装数据中是否包含 3D 模型 UUID
        QString modelUuid = footprintData->model3D().uuid();

        // 如果封装数据中没有UUID，尝试从 head.uuid_3d 字段中提取
        if (modelUuid.isEmpty() && resultData.contains("head")) {
            QJsonObject head = resultData["head"].toObject();
            if (head.contains("uuid_3d")) {
                modelUuid = head["uuid_3d"].toString();
            }
        }

        if (!modelUuid.isEmpty()) {
            qDebug() << "Fetching 3D model with UUID:" << modelUuid;

            // 创建 Model3DData 对象
            QSharedPointer<Model3DData> model3DData(new Model3DData());
            model3DData->setUuid(modelUuid);

            // 如果封装数据中有 3D 模型信息，复制平移和旋转
            if (!footprintData->model3D().uuid().isEmpty()) {
                model3DData->setName(footprintData->model3D().name());
                model3DData->setTranslation(footprintData->model3D().translation());
                model3DData->setRotation(footprintData->model3D().rotation());
            }

            componentData.setModel3DData(model3DData);

            // 在并行模式下，使用 m_fetchingComponents 存储待处理的组件数据
            if (m_parallelFetching) {
                // 更新已有的 entry
                FetchingComponent& fetchingComponent = m_fetchingComponents[m_currentComponentId];
                fetchingComponent.data = componentData;
                fetchingComponent.hasComponentInfo = true;
                fetchingComponent.hasCadData = true;
                fetchingComponent.hasObjData = false;
                fetchingComponent.hasStepData = false;
                // fetch3DModel 已经在 internal 调用时设置了
            } else {
                // 串行模式下的处理
                m_pendingComponentData = componentData;
                m_pendingModelUuid = modelUuid;
                m_hasDownloadedWrl = false;
            }

            // 获取 WRL 格式的3D 模型
            m_api->fetch3DModelObj(modelUuid);
            return;  // 等待 3D 模型数据
        } else {
            qDebug() << "No 3D model UUID found for:" << m_currentComponentId;
        }
    }

    // 不需要3D 模型或没有找到UUID，直接发送完成信号
    emit cadDataReady(m_currentComponentId, componentData);

    // 如果在并行模式下，处理并行数据收集
    if (m_parallelFetching) {
        handleParallelDataCollected(m_currentComponentId, componentData);
    }

    // 恢复组件 ID
    m_currentComponentId = savedComponentId;
}

void ComponentService::handleModel3DFetched(const QString& uuid, const QByteArray& data) {
    qDebug() << "3D model data fetched for UUID:" << uuid << "Size:" << data.size();

    // 在并行模式下，查找对应的组件
    if (m_parallelFetching) {
        // 在并行模式下，查找对应UUID的组件
        for (auto it = m_fetchingComponents.begin(); it != m_fetchingComponents.end(); ++it) {
            if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == uuid) {
                QString componentId = it.key();
                FetchingComponent& fetchingComponent = it.value();

                if (!fetchingComponent.hasObjData) {
                    // 这是 WRL 格式的3D 模型
                    fetchingComponent.data.model3DData()->setRawObj(QString::fromUtf8(data));
                    fetchingComponent.hasObjData = true;
                    qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                    // 继续下载 STEP 格式的3D 模型
                    qDebug() << "Fetching STEP model with UUID:" << uuid;
                    m_api->fetch3DModelStep(uuid);
                } else {
                    // 这是 STEP 格式的3D 模型
                    fetchingComponent.data.model3DData()->setStep(data);
                    fetchingComponent.hasStepData = true;
                    qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                    // 发送完成信号
                    emit cadDataReady(componentId, fetchingComponent.data);

                    // 处理并行数据收集
                    handleParallelDataCollected(componentId, fetchingComponent.data);

                    // 从待处理列表中移除
                    m_fetchingComponents.remove(componentId);
                }
                return;
            }
        }
        qWarning() << "Received 3D model data for unexpected UUID:" << uuid;
    } else {
        // 串行模式下的处理
        if (m_pendingComponentData.model3DData() && m_pendingComponentData.model3DData()->uuid() == uuid) {
            if (!m_hasDownloadedWrl) {
                // 这是 WRL 格式的3D 模型
                m_pendingComponentData.model3DData()->setRawObj(QString::fromUtf8(data));
                qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                // 标记已经下载了WRL 格式
                m_hasDownloadedWrl = true;

                // 继续下载 STEP 格式的3D 模型
                qDebug() << "Fetching STEP model with UUID:" << uuid;
                m_api->fetch3DModelStep(uuid);
            } else {
                // 这是 STEP 格式的3D 模型
                m_pendingComponentData.model3DData()->setStep(data);
                qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                // 发送完成信号
                emit cadDataReady(m_currentComponentId, m_pendingComponentData);

                // 清空待处理数据
                m_pendingComponentData = ComponentData();
                m_pendingModelUuid.clear();
                m_hasDownloadedWrl = false;
            }
        } else {
            qWarning() << "Received 3D model data for unexpected UUID:" << uuid;
        }
    }
}

void ComponentService::handleFetchError(const QString& errorMessage) {
    qDebug() << "Fetch error:" << errorMessage;

    // 如果在并行模式下，处理并行错误
    if (m_parallelFetching) {
        handleParallelFetchError(m_currentComponentId, errorMessage);
    }

    // 最后发送信号，防止信号连接的槽函数删除了本对象导致后续访问成员变量崩溃
    emit fetchError(m_currentComponentId, errorMessage);
}

void ComponentService::handleFetchErrorWithId(const QString& idOrUuid, const QString& error) {
    QString componentId = idOrUuid;

    // 如果在并行模式下，尝试解析 UUID 为组件 ID
    if (m_parallelFetching && !m_parallelFetchingStatus.contains(idOrUuid)) {
        for (auto it = m_fetchingComponents.begin(); it != m_fetchingComponents.end(); ++it) {
            if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == idOrUuid) {
                componentId = it.key();
                qDebug() << "Resolved UUID" << idOrUuid << "to component ID" << componentId;
                break;
            }
        }
    }

    qDebug() << "Fetch error for component:" << componentId << "Error:" << error;

    // 如果在并行模式下，处理并行错误
    if (m_parallelFetching) {
        handleParallelFetchError(componentId, error);
    }

    emit fetchError(componentId, error);
}

void ComponentService::setOutputPath(const QString& path) {
    m_outputPath = path;
}

QString ComponentService::getOutputPath() const {
    return m_outputPath;
}

void ComponentService::fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel) {
    qDebug() << "Fetching data for" << componentIds.size() << "components in parallel";

    // 初始化请求
    m_parallelFetchingStatus.clear();
    for (const QString& id : componentIds) {
        m_parallelFetchingStatus[id] = true;
        fetchComponentDataInternal(id, fetch3DModel);
    }
}

void ComponentService::handleParallelDataCollected(const QString& componentId, const ComponentData& data) {
    qDebug() << "Parallel data collected for:" << componentId;

    // 保存收集到的数据
    m_parallelCollectedData[componentId] = data;
    m_parallelCompletedCount++;

    // 更新状态
    m_parallelFetchingStatus[componentId] = false;

    // 检查是否所有元件都已收集完毕
    if (m_parallelCompletedCount >= m_parallelTotalCount) {
        qDebug() << "All components data collected in parallel:" << m_parallelCollectedData.size();

        // 发送完成信号
        QList<ComponentData> allData = m_parallelCollectedData.values();
        emit allComponentsDataCollected(allData);

        // 重置状态
        m_parallelFetching = false;
        m_parallelCollectedData.clear();
        m_parallelFetchingStatus.clear();
        m_parallelPendingComponents.clear();
    }
}

void ComponentService::handleParallelFetchError(const QString& componentId, const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    // 更新状态
    m_parallelFetchingStatus[componentId] = false;
    m_parallelCompletedCount++;

    // 检查是否所有元件都已处理完毕
    if (m_parallelCompletedCount >= m_parallelTotalCount) {
        qDebug() << "All components data collected (with errors):" << m_parallelCollectedData.size();

        // 发送完成信号
        QList<ComponentData> allData = m_parallelCollectedData.values();
        emit allComponentsDataCollected(allData);

        // 重置状态
        m_parallelFetching = false;
        m_parallelCollectedData.clear();
        m_parallelFetchingStatus.clear();
        m_parallelPendingComponents.clear();
    }
}

bool ComponentService::validateComponentId(const QString& componentId) const {
    return BomParser::validateId(componentId);
}

QStringList ComponentService::extractComponentIdFromText(const QString& text) const {
    // 依然保留简单的提取逻辑，或者可以进一步整合进 BomParser
    QStringList extractedIds;
    QRegularExpression re("[Cc]\\d{4,}");
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString id = match.captured().toUpper();
        if (!BomParser::getExcludedIds().contains(id) && !extractedIds.contains(id)) {
            extractedIds.append(id);
        }
    }
    return extractedIds;
}

QStringList ComponentService::parseBomFile(const QString& filePath) {
    BomParser parser;
    return parser.parse(filePath);
}

ComponentData ComponentService::getComponentData(const QString& componentId) const {
    return m_componentCache.value(componentId, ComponentData());
}

void ComponentService::clearCache() {
    m_componentCache.clear();
    qDebug() << "Component cache cleared";
}

}  // namespace EasyKiConverter
