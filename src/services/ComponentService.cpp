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
    , m_parallelFetching(false)
    , m_activeRequests(0)
    , m_activeImageRequests(0) {
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
    qDebug() << "Enqueuing request for:" << componentId << "Fetch 3D:" << fetch3DModel;
    m_requestQueue.enqueue(qMakePair(componentId, fetch3DModel));
    processNextRequest();
}

void ComponentService::fetchComponentDataInternal(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data (internal) for:" << componentId << "Fetch 3D:" << fetch3DModel;

    // 暂时存储当前请求的元件ID?D模型标志
    m_currentComponentId = componentId;

    // 初始化或更新 m_fetchingComponents 中的条目，确保存储 fetch3DModel 配置
    FetchingComponent& fetchingComponent = m_fetchingComponents[componentId];
    fetchingComponent.componentId = componentId;
    fetchingComponent.fetch3DModel = fetch3DModel;
    // 重置其他标志，如果是新请求
    if (!m_parallelFetching) {
        // 非并行模式下也利用这个结构来存储配置，保持一致性
        // 但注意并行模式下 m_fetchingComponents 用于追踪进度，非并行模式下可能只用于存储 fetch3DModel
        // 为了安全起见，非并行模式下我们不重置 has... 标志，或者由调用者负责清理
        // 简单起见，这里只更新 fetch3DModel
    } else {
        // 并行模式下，这些已经在 fetchMultipleComponentsData 中初始化了，但为了保险再次确认
        fetchingComponent.hasComponentInfo = false;
        fetchingComponent.hasCadData = false;
        fetchingComponent.hasObjData = false;
        fetchingComponent.hasStepData = false;
    }

    // 首先获取 CAD 数据（包含符号和封装信息?
    m_api->fetchCadData(componentId);
}

void ComponentService::fetchLcscPreviewImage(const QString& componentId) {
    qDebug() << "Enqueuing LCSC preview image request for:" << componentId;
    m_imageRequestQueue.enqueue(componentId);
    processNextImageRequest();
}

void ComponentService::performFetchLcscPreviewImage(const QString& componentId, int retryCount) {
    qDebug() << "Executing LCSC preview image request for:" << componentId << "Retry:" << retryCount;

    QString url =
        QString("https://overseas.szlcsc.com/overseas/global/search?keyword=%1&pageNumber=1&noShowSelf=false&from=%1")
            .arg(componentId);
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "Chrome/114.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, componentId, retryCount]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "LCSC search request failed:" << reply->errorString() << "Retry:" << retryCount;
            if (retryCount < 3) {
                QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, retryCount]() {
                    performFetchLcscPreviewImage(componentId, retryCount + 1);
                });
            } else {
                // 放弃重试，释放资源
                advanceImageQueue();
            }
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        // Parse: result[2].products[0].selfBreviaryImageUrl
        // 为了提高鲁棒性，放宽解析条件

        QString imageUrl;
        bool found = false;

        if (root.contains("result")) {
            QJsonArray resultArray = root["result"].toArray();
            // 遍历 result 数组寻找包含 products 的对象
            for (const auto& resVal : resultArray) {
                QJsonObject resObj = resVal.toObject();
                if (resObj.contains("products")) {
                    QJsonArray products = resObj["products"].toArray();
                    if (!products.isEmpty()) {
                        QJsonObject product0 = products[0].toObject();
                        if (product0.contains("selfBreviaryImageUrl")) {
                            imageUrl = product0["selfBreviaryImageUrl"].toString();
                            if (!imageUrl.isEmpty()) {
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (!found) {
            qWarning() << "LCSC response parsing failed for:" << componentId << "- Switching to Fallback";
            fetchLcscPreviewImageFallback(componentId);
            return;
        }

        qDebug() << "Found LCSC preview image URL:" << imageUrl << "for:" << componentId;
        downloadLcscImage(componentId, imageUrl, 0);
    });
}

// 保持兼容性，如果有旧代码调用 fetchLcscPreviewImageWithRetry，将其转发到 performFetchLcscPreviewImage
void ComponentService::fetchLcscPreviewImageWithRetry(const QString& componentId, int retryCount) {
    if (retryCount == 0) {
        fetchLcscPreviewImage(componentId);
    } else {
        performFetchLcscPreviewImage(componentId, retryCount);
    }
}

void ComponentService::processNextImageRequest() {
    qDebug() << "Processing next image request. Active:" << m_activeImageRequests
             << "Queue size:" << m_imageRequestQueue.size();

    while (m_activeImageRequests < MAX_CONCURRENT_IMAGE_REQUESTS && !m_imageRequestQueue.isEmpty()) {
        QString componentId = m_imageRequestQueue.dequeue();
        m_activeImageRequests++;

        // 实际发起请求
        performFetchLcscPreviewImage(componentId, 0);
    }
}

void ComponentService::advanceImageQueue() {
    if (m_activeImageRequests > 0) {
        m_activeImageRequests--;
    }
    qDebug() << "Image queue advanced. Active:" << m_activeImageRequests << "Queue:" << m_imageRequestQueue.size();
    processNextImageRequest();
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
    request.setTransferTimeout(15000);
    // request.setRawHeader(
    //     "cookie",
    //     "lcb_cid_pro=7c5c2031b1f74f61b1a374c6c150866c1767537144388; _lcsc_fid=2ed1a87600acafe81fb0b69568d1e485; "
    //     "lotteryKey=b6da0ab85f7f9d7d257c; noLoginCustomerFlag2=347162dfb4193385adc2; "
    //     "PRO_NEW_SID=c309f8b3-416b-4bb6-a4e4-ec6e9abd1b65; customer_info=30-5-0-1; "
    //     "currentCartVo={%22cartCode%22:%22%22%2C%22cartName%22:%22%E8%B4%AD%E7%89%A9%E8%BD%A6%EF%BC%88%E9%BB%98%E8%AE%"
    //     "A4%EF%BC%89%22%2C%22productCount%22:30}; uvCookie=2ed1a87600acafe81fb0b69568d1e485_1770459162862_T5Qboa; "
    //     "JSESSIONID=8287734B6C5B5FB19C8F3B642CDC9273; cookies_save_operation_code_mark=; isLoginCustomerFlag=6104950A; "
    //     "noLoginCustomerFlag=42e8f7115e25f0fb574e; "
    //     "acw_tc=76b20fb417704674515477679eb5b37ce59da5c291482c056a3064f85e4b21; "
    //     "searchHistoryRecord=C3018718%EF%BC%9AC12345%EF%BC%9ASM712_C7420375%EF%BC%9AC8734%EF%BC%9AUFQFPN-48_L7.0-W7.0-"
    //     "P0.50-BL-EP%EF%BC%9AC35556%EF%BC%9AC8323%EF%BC%9AC414042%EF%BC%9AC23345%EF%BC%9ASOT-23; soBuriedPointData=; "
    //     "_lcsc_asid=19c3819a4e7e2509cd4c04e8ded46ad0c7d; _uetsid=86bad040040d11f18c3cd1c583eb7e2e; "
    //     "_uetvid=dfdaa240e97911f0b6908526265f8f7c");
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
            advanceImageQueue();  // 失败也要释放资源
            return;
        }

        QString html = QString::fromUtf8(reply->readAll());

        // 放宽正则，匹配更多可能的图片 URL
        // 原始: src="(https?://[^"]+/upload/public/product/breviary/[^"]+)"
        // 新: src="(https?://[^"]+/upload/public/[^"]+\.(?:jpg|png|jpeg|webp))" 且包含 product

        QRegularExpression re("src=\"(https?://[^\"]+/upload/public/[^\"]+)\"");
        QRegularExpressionMatchIterator it = re.globalMatch(html);

        QString imageUrl;
        bool found = false;

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString url = match.captured(1);
            // 简单过滤，通常预览图包含 breviary 或 product
            if (url.contains("product") && (url.endsWith(".jpg") || url.endsWith(".png") || url.endsWith(".jpeg"))) {
                imageUrl = url;
                found = true;
                break;  // 取第一个匹配的
            }
        }

        if (found) {
            qDebug() << "Fallback: Found image URL:" << imageUrl;
            downloadLcscImage(componentId, imageUrl, 0);
        } else {
            qWarning() << "Fallback: Could not find image URL in HTML response for:" << componentId;
            advanceImageQueue();  // 失败也要释放资源
        }
    });
}

void ComponentService::downloadLcscImage(const QString& componentId, const QString& imageUrl, int retryCount) {
    qDebug() << "Downloading LCSC image from:" << imageUrl << "Retry:" << retryCount;
    QNetworkRequest imgRequest{QUrl(imageUrl)};
    imgRequest.setHeader(QNetworkRequest::UserAgentHeader,
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/114.0.0.0 Safari/537.36");
    imgRequest.setTransferTimeout(15000);

    QNetworkReply* imgReply = m_networkManager->get(imgRequest);
    connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, componentId, imageUrl, retryCount]() {
        imgReply->deleteLater();
        if (imgReply->error() != QNetworkReply::NoError) {
            qWarning() << "LCSC image download failed:" << imgReply->errorString() << "Retry:" << retryCount;
            if (retryCount < 3) {
                QTimer::singleShot(1000 * (retryCount + 1), this, [this, componentId, imageUrl, retryCount]() {
                    downloadLcscImage(componentId, imageUrl, retryCount + 1);
                });
            } else {
                advanceImageQueue();  // 放弃
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

        // 成功完成
        advanceImageQueue();
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
    // 使用本地变量 need3DModel 而不是成员变量 m_fetch3DModel
    if (need3DModel && footprintData) {
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

    // 推进请求队?
    advanceQueue();

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

                    // 推进请求队?
                    advanceQueue();
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

                // 推进请求队?
                advanceQueue();
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

    // 推进请求队?
    advanceQueue();
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

    // 初始化请求队?
    m_requestQueue.clear();
    m_activeRequests = 0;
    for (const QString& componentId : componentIds) {
        m_requestQueue.enqueue(qMakePair(componentId, fetch3DModel));
        m_parallelFetchingStatus[componentId] = true;
    }

    // 开始处理请?
    processNextRequest();
}

void ComponentService::processNextRequest() {
    qDebug() << "Processing next request. Active:" << m_activeRequests << "Queue size:" << m_requestQueue.size();

    while (m_activeRequests < MAX_CONCURRENT_REQUESTS && !m_requestQueue.isEmpty()) {
        QPair<QString, bool> request = m_requestQueue.dequeue();
        QString componentId = request.first;
        bool fetch3DModel = request.second;

        m_activeRequests++;

        qDebug() << "Starting request for:" << componentId;
        fetchComponentDataInternal(componentId, fetch3DModel);
    }
}

void ComponentService::advanceQueue() {
    if (m_activeRequests > 0) {
        m_activeRequests--;
    }
    processNextRequest();
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
        m_requestQueue.clear();
        m_activeRequests = 0;
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
        m_requestQueue.clear();
        m_activeRequests = 0;
    }
}

bool ComponentService::validateComponentId(const QString& componentId) const {
    // LCSC 元件ID格式：以 'C' 或 'c' 开头，后面跟至少4位数字
    QRegularExpression re("^[Cc]\\d{4,}$");
    return re.match(componentId).hasMatch();
}

QStringList ComponentService::extractComponentIdFromText(const QString& text) const {
    QStringList extractedIds;

    // 匹配 LCSC 元件ID格式：以 'C' 或 'c' 开头，后面跟至少4位数字
    QRegularExpression re("[Cc]\\d{4,}");
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString id = match.captured();
        // 统一转换为大写
        id = id.toUpper();
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

    // 匹配 LCSC 元件ID格式：以 'C' 或 'c' 开头，后面跟至少4位数字
    QRegularExpression re("[Cc]\\d{4,}");

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) {
            continue;
        }

        // 检索所有以 C 或 c 开头的单元格（按逗号分隔）
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
                // 统一转换为大写
                componentId = componentId.toUpper();
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

    // 匹配 LCSC 元件ID格式：以 'C' 或 'c' 开头，后面跟至少4位数字
    QRegularExpression re("[Cc]\\d{4,}");

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
                    // 统一转换为大写
                    componentId = componentId.toUpper();
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
