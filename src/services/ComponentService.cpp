#include "ComponentService.h"

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
    , m_parallelFetching(false) {
    // 连接 API 信号
    connect(m_api, &EasyedaApi::componentInfoFetched, this, &ComponentService::handleComponentInfoFetched);
    connect(m_api, &EasyedaApi::cadDataFetched, this, &ComponentService::handleCadDataFetched);
    connect(m_api, &EasyedaApi::model3DFetched, this, &ComponentService::handleModel3DFetched);
    connect(m_api, &EasyedaApi::fetchError, this, &ComponentService::handleFetchError);
}

ComponentService::~ComponentService() {}

void ComponentService::fetchComponentData(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data for:" << componentId << "Fetch 3D:" << fetch3DModel;

    // 暂时存储当前请求的元件ID?D模型标志
    m_currentComponentId = componentId;
    m_fetch3DModel = fetch3DModel;

    // 首先获取 CAD 数据（包含符号和封装信息?
    m_api->fetchCadData(componentId);
}

void ComponentService::fetchLcscPreviewImage(const QString& componentId) {
    fetchLcscPreviewImageWithRetry(componentId, 0);
}

void ComponentService::fetchLcscPreviewImageWithRetry(const QString& componentId, int retryCount) {
    qDebug() << "Fetching LCSC preview image for:" << componentId << "Retry:" << retryCount;
    QString url =
        QString("https://overseas.szlcsc.com/overseas/global/search?keyword=%1&pageNumber=1&noShowSelf=false&from=%1")
            .arg(componentId);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/114.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, retryCount]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "LCSC search request failed:" << reply->errorString() << "Retry:" << retryCount;
            if (retryCount < 3) {
                QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, retryCount]() {
                    fetchLcscPreviewImageWithRetry(componentId, retryCount + 1);
                });
            }
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        // Parse: result[2].products[0].selfBreviaryImageUrl
        if (!root.contains("result")) {
            qWarning() << "LCSC response missing 'result' field for:" << componentId << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;
        }
        QJsonArray resultArray = root["result"].toArray();
        if (resultArray.size() <= 2) {
            qWarning() << "LCSC response 'result' array size too small:" << resultArray.size() << "for:" << componentId
                       << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;  // Need at least index 2
        }

        QJsonObject result2 = resultArray[2].toObject();
        if (!result2.contains("products")) {
            qWarning() << "LCSC response missing 'products' in result[2] for:" << componentId
                       << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;
        }

        QJsonArray products = result2["products"].toArray();
        if (products.isEmpty()) {
            qWarning() << "LCSC products array is empty for:" << componentId << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;
        }

        QJsonObject product0 = products[0].toObject();
        if (!product0.contains("selfBreviaryImageUrl")) {
            qWarning() << "LCSC product missing 'selfBreviaryImageUrl' for:" << componentId
                       << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;
        }

        QString imageUrl = product0["selfBreviaryImageUrl"].toString();
        if (imageUrl.isEmpty()) {
            qWarning() << "LCSC image URL is empty for:" << componentId << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;
        }

        qDebug() << "Found LCSC preview image URL:" << imageUrl << "for:" << componentId;
        downloadLcscImage(componentId, imageUrl, 0);
    });
}

void ComponentService::fetchLcscPreviewImageFallback(const QString& componentId) {
    qDebug() << "Starting LCSC Fallback scraping for:" << componentId;
    QString url =
        QString(
            "https://so.szlcsc.com/"
            "global.html?k=%1&hot-key=KLM8G1GETF-B041&lcsc_vid="
            "RQQKVgVeQFdeVwUFFgcLX1UAFVEMUV1TQVkKVgFST1QxVlNRR1VbXlBfQ1ddVjtW&spm=sc.gbn.hd.ss___sc.gbn.hd.ss")
            .arg(componentId);

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/144.0.0.0 Safari/537.36");
    request.setRawHeader("accept",
                         "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/"
                         "*;q=0.8,application/signed-exchange;v=b3;q=0.7");
    request.setRawHeader("accept-language", "zh-CN,zh;q=0.9,en;q=0.8");
    request.setRawHeader("cache-control", "no-cache");
    request.setRawHeader(
        "cookie",
        "lcb_cid_pro=7c5c2031b1f74f61b1a374c6c150866c1767537144388; _lcsc_fid=2ed1a87600acafe81fb0b69568d1e485; "
        "lotteryKey=b6da0ab85f7f9d7d257c; noLoginCustomerFlag2=347162dfb4193385adc2; "
        "PRO_NEW_SID=c309f8b3-416b-4bb6-a4e4-ec6e9abd1b65; customer_info=30-5-0-1; "
        "currentCartVo={%22cartCode%22:%22%22%2C%22cartName%22:%22%E8%B4%AD%E7%89%A9%E8%BD%A6%EF%BC%88%E9%BB%98%E8%AE%"
        "A4%EF%BC%89%22%2C%22productCount%22:30}; uvCookie=2ed1a87600acafe81fb0b69568d1e485_1770459162862_T5Qboa; "
        "JSESSIONID=8287734B6C5B5FB19C8F3B642CDC9273; cookies_save_operation_code_mark=; isLoginCustomerFlag=6104950A; "
        "noLoginCustomerFlag=42e8f7115e25f0fb574e; "
        "acw_tc=76b20fb417704674515477679eb5b37ce59da5c291482c056a3064f85e4b21; "
        "searchHistoryRecord=C3018718%EF%BC%9AC12345%EF%BC%9ASM712_C7420375%EF%BC%9AC8734%EF%BC%9AUFQFPN-48_L7.0-W7.0-"
        "P0.50-BL-EP%EF%BC%9AC35556%EF%BC%9AC8323%EF%BC%9AC414042%EF%BC%9AC23345%EF%BC%9ASOT-23; soBuriedPointData=; "
        "_lcsc_asid=19c3819a4e7e2509cd4c04e8ded46ad0c7d; _uetsid=86bad040040d11f18c3cd1c583eb7e2e; "
        "_uetvid=dfdaa240e97911f0b6908526265f8f7c");
    request.setRawHeader("pragma", "no-cache");
    request.setRawHeader("priority", "u=0, i");
    request.setRawHeader("referer", QString("https://so.szlcsc.com/global.html?k=%1").arg(componentId).toUtf8());
    request.setRawHeader("sec-ch-ua", "\"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Google Chrome\";v=\"144\"");
    request.setRawHeader("sec-ch-ua-mobile", "?0");
    request.setRawHeader("sec-ch-ua-platform", "\"Windows\"");
    request.setRawHeader("sec-fetch-dest", "document");
    request.setRawHeader("sec-fetch-mode", "navigate");
    request.setRawHeader("sec-fetch-site", "same-origin");
    request.setRawHeader("sec-fetch-user", "?1");
    request.setRawHeader("upgrade-insecure-requests", "1");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "LCSC Fallback scraping failed:" << reply->errorString();
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());

        // Regex to find the image src.
        // Targeting the structure: <img ... src=".../product/breviary/..." ...> inside the product list
        // Since we know the image usually contains "breviary" or is a jpg/png link in the result list.
        // The user's xpath points to the first result.
        // Let's look for the first image URL that looks like a product image.

        // Strategy: Look for the specific image class or structure if possible, but generic product image URL regex is safer.
        // LCSC images often look like: https://alimg.szlcsc.com/upload/public/product/breviary/...

        QRegularExpression re("src=\"(https?://[^\"]+/upload/public/product/breviary/[^\"]+)\"");
        QRegularExpressionMatch match = re.match(html);

        if (match.hasMatch()) {
            QString imageUrl = match.captured(1);
            qDebug() << "Fallback: Found image URL:" << imageUrl;
            downloadLcscImage(componentId, imageUrl, 0);
        } else {
            qWarning() << "Fallback: Could not find image URL in HTML response for:" << componentId;
            // Debug: Print a small part of HTML to see if we got the right page
            // qDebug() << "HTML snippet:" << html.left(500);
        }
    });
}

void ComponentService::downloadLcscImage(const QString& componentId, const QString& imageUrl, int retryCount) {
    qDebug() << "Downloading LCSC image from:" << imageUrl << "Retry:" << retryCount;
    QNetworkRequest imgRequest{QUrl(imageUrl)};
    imgRequest.setHeader(QNetworkRequest::UserAgentHeader,
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/114.0.0.0 Safari/537.36");

    QNetworkReply* imgReply = m_networkManager->get(imgRequest);
    connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, componentId, imageUrl, retryCount]() {
        imgReply->deleteLater();
        if (imgReply->error() != QNetworkReply::NoError) {
            qWarning() << "LCSC image download failed:" << imgReply->errorString() << "Retry:" << retryCount;
            if (retryCount < 3) {
                QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, imageUrl, retryCount]() {
                    downloadLcscImage(componentId, imageUrl, retryCount + 1);
                });
            }
            return;
        }

        QByteArray data = imgReply->readAll();
        QImage image;
        if (image.loadFromData(data)) {
            qDebug() << "LCSC preview image downloaded successfully for:" << componentId << "Size:" << data.size();
            emit previewImageReady(componentId, image);
        } else {
            qWarning() << "Failed to load LCSC image from data for:" << componentId << "Size:" << data.size();
        }
    });
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
        // 如果没有 lcscId 字段，尝试从 lcsc.szlcsc.number 中提?
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

    // 临时保存当前的组?ID
    QString savedComponentId = m_currentComponentId;
    m_currentComponentId = lcscId;

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

    // 调试：打印resultData的结?
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

    // 检查是否需要获?3D 模型
    if (m_fetch3DModel && footprintData) {
        // 检查封装数据中是否包含 3D 模型 UUID
        QString modelUuid = footprintData->model3D().uuid();

        // 如果封装数据中没?UUID，尝试从 head.uuid_3d 字段中提?
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

            // 在并行模式下，使?m_fetchingComponents 存储待处理的组件数据
            if (m_parallelFetching) {
                FetchingComponent fetchingComponent;
                fetchingComponent.componentId = m_currentComponentId;
                fetchingComponent.data = componentData;
                fetchingComponent.hasComponentInfo = true;
                fetchingComponent.hasCadData = true;
                fetchingComponent.hasObjData = false;
                fetchingComponent.hasStepData = false;
                m_fetchingComponents[m_currentComponentId] = fetchingComponent;
            } else {
                // 串行模式下的处理
                m_pendingComponentData = componentData;
                m_pendingModelUuid = modelUuid;
                m_hasDownloadedWrl = false;
            }

            // 获取 WRL 格式?3D 模型
            m_api->fetch3DModelObj(modelUuid);
            return;  // 等待 3D 模型数据
        } else {
            qDebug() << "No 3D model UUID found for:" << m_currentComponentId;
        }
    }

    // 不需?3D 模型或没有找?UUID，直接发送完成信?
    emit cadDataReady(m_currentComponentId, componentData);

    // 如果在并行模式下，处理并行数据收?
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
        // 在并行模式下，查找对?UUID 的组?
        for (auto it = m_fetchingComponents.begin(); it != m_fetchingComponents.end(); ++it) {
            if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == uuid) {
                QString componentId = it.key();
                FetchingComponent& fetchingComponent = it.value();

                if (!fetchingComponent.hasObjData) {
                    // 这是 WRL 格式?3D 模型
                    fetchingComponent.data.model3DData()->setRawObj(QString::fromUtf8(data));
                    fetchingComponent.hasObjData = true;
                    qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                    // 继续下载 STEP 格式?3D 模型
                    qDebug() << "Fetching STEP model with UUID:" << uuid;
                    m_api->fetch3DModelStep(uuid);
                } else {
                    // 这是 STEP 格式?3D 模型
                    fetchingComponent.data.model3DData()->setStep(data);
                    fetchingComponent.hasStepData = true;
                    qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                    // 发送完成信?
                    emit cadDataReady(componentId, fetchingComponent.data);

                    // 处理并行数据收集
                    handleParallelDataCollected(componentId, fetchingComponent.data);

                    // 从待处理列表中移?
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
                // 这是 WRL 格式?3D 模型
                m_pendingComponentData.model3DData()->setRawObj(QString::fromUtf8(data));
                qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                // 标记已经下载?WRL 格式
                m_hasDownloadedWrl = true;

                // 继续下载 STEP 格式?3D 模型
                qDebug() << "Fetching STEP model with UUID:" << uuid;
                m_api->fetch3DModelStep(uuid);
            } else {
                // 这是 STEP 格式?3D 模型
                m_pendingComponentData.model3DData()->setStep(data);
                qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                // 发送完成信?
                emit cadDataReady(m_currentComponentId, m_pendingComponentData);

                // 清空待处理数?
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

void ComponentService::setOutputPath(const QString& path) {
    m_outputPath = path;
}

QString ComponentService::getOutputPath() const {
    return m_outputPath;
}

void ComponentService::fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel) {
    qDebug() << "Fetching data for" << componentIds.size() << "components in parallel";

    // 初始化并行数据收集状?
    m_parallelCollectedData.clear();
    m_parallelFetchingStatus.clear();
    m_parallelPendingComponents = componentIds;
    m_parallelTotalCount = componentIds.size();
    m_parallelCompletedCount = 0;
    m_parallelFetching = true;
    m_fetch3DModel = fetch3DModel;

    // 为每个元件启动数据收?
    for (const QString& componentId : componentIds) {
        m_parallelFetchingStatus[componentId] = true;
        fetchComponentData(componentId, fetch3DModel);
    }
}

void ComponentService::handleParallelDataCollected(const QString& componentId, const ComponentData& data) {
    qDebug() << "Parallel data collected for:" << componentId;

    // 保存收集到的数据
    m_parallelCollectedData[componentId] = data;
    m_parallelCompletedCount++;

    // 更新状?
    m_parallelFetchingStatus[componentId] = false;

    // 检查是否所有元件都已收集完?
    if (m_parallelCompletedCount >= m_parallelTotalCount) {
        qDebug() << "All components data collected in parallel:" << m_parallelCollectedData.size();

        // 发送完成信?
        QList<ComponentData> allData = m_parallelCollectedData.values();
        emit allComponentsDataCollected(allData);

        // 重置状?
        m_parallelFetching = false;
        m_parallelCollectedData.clear();
        m_parallelFetchingStatus.clear();
        m_parallelPendingComponents.clear();
    }
}

void ComponentService::handleParallelFetchError(const QString& componentId, const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    // 更新状?
    m_parallelFetchingStatus[componentId] = false;
    m_parallelCompletedCount++;

    // 检查是否所有元件都已处理完?
    if (m_parallelCompletedCount >= m_parallelTotalCount) {
        qDebug() << "All components data collected (with errors):" << m_parallelCollectedData.size();

        // 发送完成信?
        QList<ComponentData> allData = m_parallelCollectedData.values();
        emit allComponentsDataCollected(allData);

        // 重置状?
        m_parallelFetching = false;
        m_parallelCollectedData.clear();
        m_parallelFetchingStatus.clear();
        m_parallelPendingComponents.clear();
    }
}

bool ComponentService::validateComponentId(const QString& componentId) const {
    // LCSC 元件ID格式：以 'C' 开头，后面跟至少4位数字
    QRegularExpression re("^C\\d{4,}$");
    return re.match(componentId).hasMatch();
}

QStringList ComponentService::extractComponentIdFromText(const QString& text) const {
    QStringList extractedIds;

    // 匹配 LCSC 元件ID格式：以 'C' 开头，后面跟至少4位数字
    QRegularExpression re("C\\d{4,}");
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString id = match.captured();
        if (!extractedIds.contains(id)) {
            extractedIds.append(id);
        }
    }

    return extractedIds;
}

QStringList ComponentService::parseBomFile(const QString& filePath) {
    qDebug() << "Parsing BOM file:" << filePath;

    QStringList componentIds;
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        qWarning() << "BOM file does not exist:" << filePath;
        return componentIds;
    }

    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "csv" || suffix == "txt") {
        componentIds = parseCsvBomFile(filePath);
    } else if (suffix == "xlsx" || suffix == "xls") {
        componentIds = parseExcelBomFile(filePath);
    } else {
        qWarning() << "Unsupported BOM file format:" << suffix;
    }

    qDebug() << "Extracted" << componentIds.size() << "component IDs from BOM file";
    return componentIds;
}

QStringList ComponentService::parseCsvBomFile(const QString& filePath) {
    qDebug() << "Parsing CSV BOM file:" << filePath;

    QStringList componentIds;
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open CSV file:" << filePath << file.errorString();
        return componentIds;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // 匹配 LCSC 元件ID格式：以 'C' 开头，后面跟至少4位数字
    QRegularExpression re("C\\d{4,}");

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) {
            continue;
        }

        // 检索所有以 C 开头的单元格（按逗号分隔）
        QStringList cells = line.split(',', Qt::SkipEmptyParts);

        for (const QString& cell : cells) {
            QString trimmedCell = cell.trimmed();

            // 去除可能的引号
            if (trimmedCell.startsWith('"') && trimmedCell.endsWith('"')) {
                trimmedCell = trimmedCell.mid(1, trimmedCell.length() - 2);
            }

            // 检查是否匹配元件ID格式
            QRegularExpressionMatch match = re.match(trimmedCell);
            if (match.hasMatch()) {
                QString componentId = match.captured();
                if (!componentIds.contains(componentId)) {
                    componentIds.append(componentId);
                    qDebug() << "Found component ID:" << componentId;
                }
            }
        }
    }

    file.close();
    return componentIds;
}

QStringList ComponentService::parseExcelBomFile(const QString& filePath) {
    qDebug() << "Parsing Excel BOM file:" << filePath;

    QStringList componentIds;

// 尝试使用 QXlsx 库解析 Excel 文件
// 如果 QXlsx 不可用，则尝试使用其他方法
#ifdef QT_XLSX_LIB
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        qWarning() << "Failed to load Excel file:" << filePath;
        return componentIds;
    }

    // 匹配 LCSC 元件ID格式：以 'C' 开头，后面跟至少4位数字
    QRegularExpression re("C\\d{4,}");

    // 遍历所有工作表
    for (const QString& sheetName : xlsx.sheetNames()) {
        xlsx.selectSheet(sheetName);

        // 遍历所有单元格
        QXlsx::CellRange range = xlsx.dimension();
        for (int row = range.firstRow(); row <= range.lastRow(); ++row) {
            for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
                QXlsx::Cell* cell = xlsx.cellAt(row, col);
                if (!cell) {
                    continue;
                }

                QVariant value = cell->readValue();
                if (value.isNull() || !value.isValid()) {
                    continue;
                }

                QString cellText = value.toString().trimmed();

                // 检查是否匹配元件ID格式
                QRegularExpressionMatch match = re.match(cellText);
                if (match.hasMatch()) {
                    QString componentId = match.captured();
                    if (!componentIds.contains(componentId)) {
                        componentIds.append(componentId);
                        qDebug() << "Found component ID:" << componentId << "at" << sheetName << ":" << row << ","
                                 << col;
                    }
                }
            }
        }
    }
#else
    // 如果没有 QXlsx 库，尝试使用 Python 脚本解析
    qWarning() << "QXlsx library not available, trying alternative method";

    // 尝试将 Excel 转换为 CSV 格式
    QString pythonScript = QString(
                               "import pandas as pd\n"
                               "import sys\n"
                               "\n"
                               "try:\n"
                               "    # 读取 Excel 文件\n"
                               "    df = pd.read_excel(r'%1', sheet_name=None)\n"
                               "    \n"
                               "    # 合并所有工作表\n"
                               "    all_data = pd.concat(df.values(), ignore_index=True)\n"
                               "    \n"
                               "    # 保存为 CSV\n"
                               "    all_data.to_csv(r'%2', index=False, encoding='utf-8')\n"
                               "    print('SUCCESS')\n"
                               "except Exception as e:\n"
                               "    print('ERROR:', str(e), file=sys.stderr)\n"
                               "    sys.exit(1)\n")
                               .arg(filePath, filePath + ".temp.csv");

    // 创建临时 Python 脚本文件
    QString tempScriptPath =
        QDir::tempPath() + "/excel_to_csv_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".py";
    QFile tempScript(tempScriptPath);

    if (tempScript.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&tempScript);
        out.setEncoding(QStringConverter::Utf8);
        out << pythonScript;
        tempScript.close();

        // 执行 Python 脚本
        QProcess process;
        process.start("python", QStringList() << tempScriptPath);
        process.waitForFinished(30000);  // 30秒超时

        if (process.exitCode() == 0) {
            QString tempCsvPath = filePath + ".temp.csv";
            componentIds = parseCsvBomFile(tempCsvPath);

            // 删除临时文件
            QFile::remove(tempCsvPath);
        } else {
            qWarning() << "Failed to convert Excel to CSV:" << process.readAllStandardError();
        }

        QFile::remove(tempScriptPath);
    } else {
        qWarning() << "Failed to create temporary Python script";
    }
#endif

    return componentIds;
}

ComponentData ComponentService::getComponentData(const QString& componentId) const {
    return m_componentCache.value(componentId, ComponentData());
}

void ComponentService::clearCache() {
    m_componentCache.clear();
    qDebug() << "Component cache cleared";
}

}  // namespace EasyKiConverter