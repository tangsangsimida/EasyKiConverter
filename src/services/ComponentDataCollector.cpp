#include "ComponentDataCollector.h"

#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaImporter.h"

#include <QDebug>

namespace EasyKiConverter {

ComponentDataCollector::ComponentDataCollector(const QString& componentId, QObject* parent)
    : QObject(parent)
    , m_componentId(componentId)
    , m_state(Idle)
    , m_export3DModel(true)
    , m_isCancelled(false)
    , m_api(new EasyedaApi(this))
    , m_importer(new EasyedaImporter(this)) {
    initializeApiConnections();
}

ComponentDataCollector::~ComponentDataCollector() {}

void ComponentDataCollector::start() {
    qDebug() << "Starting data collection for:" << m_componentId;

    if (m_state != Idle) {
        qWarning() << "Data collector already running for:" << m_componentId;
        return;
    }

    m_isCancelled = false;
    setState(FetchingCadData);

    // 直接获取 CAD 数据（包含符号和封装信息
    m_api->fetchCadData(m_componentId);
}

void ComponentDataCollector::cancel() {
    qDebug() << "Canceling data collection for:" << m_componentId;
    m_isCancelled = true;
    m_api->cancelRequest();
}

void ComponentDataCollector::setState(State state) {
    if (m_state != state) {
        m_state = state;
        qDebug() << "State changed to:" << state << "for:" << m_componentId;
        emit stateChanged(state);
    }
}

void ComponentDataCollector::handleComponentInfoFetched(const QJsonObject& data) {
    // 这个方法暂时不使用，因为我们直接获取 CAD 数据
    qDebug() << "Component info fetched (unused)";
}

void ComponentDataCollector::handleCadDataFetched(const QJsonObject& data) {
    qDebug() << "CAD data fetched for:" << m_componentId;

    if (m_isCancelled) {
        handleError("Data collection cancelled");
        return;
    }

    // 检查响应结
    QJsonObject resultData;
    if (data.contains("result")) {
        resultData = data["result"].toObject();
    } else {
        resultData = data;
    }

    if (resultData.isEmpty()) {
        handleError("Empty CAD data");
        return;
    }

    // 设置元件基本信息
    m_componentData.setLcscId(m_componentId);

    if (resultData.contains("title")) {
        m_componentData.setName(resultData["title"].toString());
    }
    if (resultData.contains("package")) {
        m_componentData.setPackage(resultData["package"].toString());
    }
    if (resultData.contains("manufacturer")) {
        m_componentData.setManufacturer(resultData["manufacturer"].toString());
    }
    if (resultData.contains("datasheet")) {
        m_componentData.setDatasheet(resultData["datasheet"].toString());
    }

    // 导入符号数据
    QSharedPointer<SymbolData> symbolData = m_importer->importSymbolData(resultData);
    if (symbolData) {
        m_componentData.setSymbolData(symbolData);
        qDebug() << "Symbol imported - Name:" << symbolData->info().name;
    } else {
        qWarning() << "Failed to import symbol data";
    }

    // 导入封装数据
    QSharedPointer<FootprintData> footprintData = m_importer->importFootprintData(resultData);
    if (footprintData) {
        m_componentData.setFootprintData(footprintData);
        qDebug() << "Footprint imported - Name:" << footprintData->info().name;
    } else {
        qWarning() << "Failed to import footprint data";
    }

    // 检查是否需要获3D 模型
    if (m_export3DModel && footprintData && footprintData->model3D().uuid().isEmpty()) {
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
                    m_componentData.setModel3DData(model3DData);

                    // 进入获取 OBJ 数据状
                    setState(FetchingObjData);
                    m_api->fetch3DModelObj(uuid);
                    return;
                }
            }
        }
    }

    // 不需3D 模型或没有找UUID，直接完
    complete();
}

void ComponentDataCollector::handleModel3DFetched(const QString& uuid, const QByteArray& data) {
    qDebug() << "3D model data fetched for UUID:" << uuid << "Size:" << data.size();

    if (m_isCancelled) {
        handleError("Data collection cancelled");
        return;
    }

    // 根据当前状态决定如何处
    if (m_state == FetchingObjData) {
        // 保存 OBJ 数据
        if (m_componentData.model3DData()) {
            m_componentData.model3DData()->setRawObj(QString::fromUtf8(data));
            qDebug() << "OBJ data saved for:" << uuid;
        }

        // 检查是否需要获STEP 数据
        if (m_export3DModel) {
            setState(FetchingStepData);
            m_api->fetch3DModelStep(uuid);
        } else {
            complete();
        }
    } else if (m_state == FetchingStepData) {
        // 保存 STEP 数据
        if (m_componentData.model3DData()) {
            m_componentData.model3DData()->setStep(data);
            qDebug() << "STEP data saved for:" << uuid;
        }

        complete();
    } else {
        qDebug() << "Unexpected 3D model data in state:" << m_state;
    }
}

void ComponentDataCollector::handleFetchError(const QString& errorMessage) {
    qDebug() << "Fetch error:" << errorMessage;
    handleError(errorMessage);
}

void ComponentDataCollector::complete() {
    qDebug() << "Data collection completed for:" << m_componentId;
    setState(Completed);
    emit dataCollected(m_componentId, m_componentData);
}

void ComponentDataCollector::handleError(const QString& error) {
    qDebug() << "Error occurred:" << error;
    m_errorMessage = error;
    setState(Failed);
    emit errorOccurred(m_componentId, error);
}

void ComponentDataCollector::initializeApiConnections() {
    connect(m_api, &EasyedaApi::componentInfoFetched, this, &ComponentDataCollector::handleComponentInfoFetched);
    connect(m_api, &EasyedaApi::cadDataFetched, this, &ComponentDataCollector::handleCadDataFetched);
    connect(m_api, &EasyedaApi::model3DFetched, this, &ComponentDataCollector::handleModel3DFetched);
    connect(m_api, qOverload<const QString&>(&EasyedaApi::fetchError), this, &ComponentDataCollector::handleFetchError);
    connect(m_api,
            qOverload<const QString&, const QString&>(&EasyedaApi::fetchError),
            this,
            [this](const QString&, const QString& errorMessage) { handleFetchError(errorMessage); });
}

}  // namespace EasyKiConverter
