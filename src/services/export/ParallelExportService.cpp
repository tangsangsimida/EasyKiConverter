#include "ParallelExportService.h"

#include "ComponentService.h"
#include "DatasheetExportStage.h"
#include "FootprintExportStage.h"
#include "Model3DExportStage.h"
#include "PreviewImagesExportStage.h"
#include "SymbolExportStage.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/network/NetworkClient.h"
#include "services/ComponentCacheService.h"

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

namespace EasyKiConverter {

namespace {

QSharedPointer<ComponentData> loadDiskCachedComponentData(const QString& componentId) {
    ComponentCacheService* cache = ComponentCacheService::instance();
    QSharedPointer<ComponentData> cachedData = cache->loadComponentData(componentId);
    if (!cachedData) {
        return nullptr;
    }

    const QByteArray cadJsonData = cache->loadCadDataJson(componentId);
    if (!cadJsonData.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonData, &parseError);
        if (parseError.error == QJsonParseError::NoError && cadDoc.isObject()) {
            const QJsonObject cadDataObject = cadDoc.object();
            EasyedaSymbolImporter symbolImporter;
            EasyedaFootprintImporter footprintImporter;
            cachedData->setSymbolData(symbolImporter.importSymbolData(cadDataObject));
            cachedData->setFootprintData(footprintImporter.importFootprintData(cadDataObject));
        }
    }

    const QByteArray datasheetData = cache->loadDatasheet(componentId);
    if (!datasheetData.isEmpty()) {
        cachedData->setDatasheetData(datasheetData);
    }

    return cachedData;
}

void mergeComponentData(ComponentData& target, const QSharedPointer<ComponentData>& fallback) {
    if (!fallback) {
        return;
    }

    if (target.lcscId().isEmpty()) {
        target = *fallback;
        return;
    }

    if (target.symbolData() == nullptr && fallback->symbolData() != nullptr) {
        target.setSymbolData(fallback->symbolData());
    }
    if (target.footprintData() == nullptr && fallback->footprintData() != nullptr) {
        target.setFootprintData(fallback->footprintData());
    }
    if ((target.model3DData() == nullptr || target.model3DData()->uuid().isEmpty()) &&
        fallback->model3DData() != nullptr) {
        target.setModel3DData(fallback->model3DData());
    }
    if (target.previewImages().isEmpty() && !fallback->previewImages().isEmpty()) {
        target.setPreviewImages(fallback->previewImages());
    }
    if (target.previewImageData().isEmpty() && !fallback->previewImageData().isEmpty()) {
        target.setPreviewImageData(fallback->previewImageData());
    }
    if (target.datasheet().isEmpty() && !fallback->datasheet().isEmpty()) {
        target.setDatasheet(fallback->datasheet());
    }
    if (target.datasheetFormat().isEmpty() && !fallback->datasheetFormat().isEmpty()) {
        target.setDatasheetFormat(fallback->datasheetFormat());
    }
    if (target.datasheetData().isEmpty() && !fallback->datasheetData().isEmpty()) {
        target.setDatasheetData(fallback->datasheetData());
    }
    if (target.name().isEmpty() && !fallback->name().isEmpty()) {
        target.setName(fallback->name());
    }
    if (target.prefix().isEmpty() && !fallback->prefix().isEmpty()) {
        target.setPrefix(fallback->prefix());
    }
    if (target.package().isEmpty() && !fallback->package().isEmpty()) {
        target.setPackage(fallback->package());
    }
    if (target.manufacturer().isEmpty() && !fallback->manufacturer().isEmpty()) {
        target.setManufacturer(fallback->manufacturer());
    }
    if (target.manufacturerPart().isEmpty() && !fallback->manufacturerPart().isEmpty()) {
        target.setManufacturerPart(fallback->manufacturerPart());
    }
}

void recomputeTypeProgressCounts(ExportTypeProgress& progress) {
    progress.completedCount = 0;
    progress.successCount = 0;
    progress.failedCount = 0;
    progress.skippedCount = 0;
    progress.inProgressCount = 0;

    for (auto it = progress.itemStatus.cbegin(); it != progress.itemStatus.cend(); ++it) {
        switch (it.value().status) {
            case ExportItemStatus::Status::Pending:
                break;
            case ExportItemStatus::Status::InProgress:
                progress.inProgressCount++;
                break;
            case ExportItemStatus::Status::Success:
                progress.completedCount++;
                progress.successCount++;
                break;
            case ExportItemStatus::Status::Failed:
                progress.completedCount++;
                progress.failedCount++;
                break;
            case ExportItemStatus::Status::Skipped:
                progress.completedCount++;
                progress.skippedCount++;
                break;
        }
    }
}

}  // namespace

ParallelExportService::ParallelExportService(QObject* parent) : QObject(parent), m_progress() {
    m_progress.currentStage = ExportOverallProgress::Stage::Idle;
}

ParallelExportService::~ParallelExportService() {
    // 取消所有导出阶段，防止worker在stage销毁后访问已释放对象
    for (auto* stage : m_exportStages) {
        stage->cancel();
        stage->deleteLater();  // 使用 deleteLater 确保线程安全清理
    }

    // 清理阶段列表
    m_exportStages.clear();
}

void ParallelExportService::cleanupExportStages() {
    for (auto* stage : m_exportStages) {
        if (stage) {
            stage->deleteLater();
        }
    }
    m_exportStages.clear();
}

void ParallelExportService::setOptions(const ExportOptions& options) {
    m_options = options;
}

void ParallelExportService::setOutputPath(const QString& path) {
    m_options.outputPath = path;
}

void ParallelExportService::setComponentService(ComponentService* componentService) {
    m_componentService = componentService;
}

void ParallelExportService::startPreload(const QStringList& componentIds) {
    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        qWarning() << "ParallelExportService: Preload already in progress";
        return;
    }

    m_cancelRequested = false;
    qDebug() << "ParallelExportService: Starting preload for" << componentIds.size() << "components";

    m_componentIds = componentIds;
    m_progress.currentStage = ExportOverallProgress::Stage::Preloading;
    m_progress.totalComponents = componentIds.size();
    m_progress.startTime = QDateTime::currentDateTime();
    m_progress.preloadProgress = PreloadProgress();
    m_progress.preloadProgress.totalCount = componentIds.size();
    m_cachedData.clear();
    m_preloadCompleted = false;
    m_nextPreloadIndex = 0;

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.preloadProgress.inProgressCount = 0;
    }

    emit preloadProgressChanged(m_progress.preloadProgress);
    updateOverallProgress();
    logNetworkRuntimeStats(QStringLiteral("preload-start"));

    // 使用 ComponentService 的并行获取能力来加载数据
    // 这样可以利用配置的并发数（默认10个并行请求）
    if (m_componentService) {
        // 连接到 ComponentService 的信号以接收数据
        connect(m_componentService,
                &ComponentService::allComponentsDataCollected,
                this,
                &ParallelExportService::onAllComponentDataCollected,
                Qt::UniqueConnection);

        // 启动并行获取
        m_componentService->fetchMultipleComponentsData(componentIds, m_options.exportModel3D);

        qDebug() << "ParallelExportService: Started parallel fetch for" << componentIds.size() << "components";
    } else {
        // 如果没有 ComponentService，直接从本地缓存读取
        qWarning() << "ParallelExportService: ComponentService not set, using sync cache fallback";
        processNextPreloadBatch();
    }
}

void ParallelExportService::cancelPreload() {
    qDebug() << "ParallelExportService: Cancelling preload";
    m_cancelRequested = true;
    m_nextPreloadIndex = m_componentIds.size();
    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.inProgressCount = 0;
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
        m_progress.endTime = QDateTime::currentDateTime();
    }
    updateOverallProgress();
    logNetworkRuntimeStats(QStringLiteral("preload-cancelled"));
    writeExportDetailedReport(QStringLiteral("preload-cancelled"));
    emit cancelled();
}

void ParallelExportService::startExport() {
    if (m_progress.currentStage == ExportOverallProgress::Stage::Exporting) {
        qWarning() << "ParallelExportService: Export already in progress";
        return;
    }

    if (!m_preloadCompleted) {
        qWarning() << "ParallelExportService: startExport called before preload completed";
        emit failed(QStringLiteral("Preload has not completed"));
        return;
    }

    if (m_componentIds.isEmpty()) {
        qWarning() << "ParallelExportService: No components to export";
        emit failed(QStringLiteral("No components to export"));
        return;
    }

    cleanupExportStages();
    m_cancelRequested = false;

    qDebug() << "ParallelExportService: Starting export for" << m_componentIds.size() << "components";

    m_progress.currentStage = ExportOverallProgress::Stage::Exporting;
    m_progress.startTime = QDateTime::currentDateTime();
    m_progress.exportTypeProgress.clear();
    m_runningExportStages = 0;

    const bool enableSymbol = m_options.exportSymbol;
    const bool enableFootprint = m_options.exportFootprint;
    const bool enableModel3D = m_options.exportModel3D;
    const bool enablePreview = m_options.exportPreviewImages;
    const bool enableDatasheet = m_options.exportDatasheet;
    QStringList exportableComponentIds;
    QStringList missingDataComponentIds;

    for (const QString& componentId : std::as_const(m_componentIds)) {
        const auto it = m_cachedData.constFind(componentId);
        if (it != m_cachedData.cend() && it.value()) {
            exportableComponentIds.append(componentId);
        } else {
            missingDataComponentIds.append(componentId);
        }
    }

    if (!missingDataComponentIds.isEmpty()) {
        qWarning() << "ParallelExportService: Missing preloaded component data for" << missingDataComponentIds.size()
                   << "components:" << missingDataComponentIds;
    }

    const auto initTypeProgress = [this](const QString& typeName) {
        ExportTypeProgress progress;
        progress.typeName = typeName;
        progress.totalCount = m_componentIds.size();
        m_progress.exportTypeProgress[typeName] = progress;
    };

    if (enableSymbol) {
        initTypeProgress(QStringLiteral("Symbol"));
    }
    if (enableFootprint) {
        initTypeProgress(QStringLiteral("Footprint"));
    }
    if (enableModel3D) {
        initTypeProgress(QStringLiteral("Model3D"));
    }
    if (enablePreview) {
        initTypeProgress(QStringLiteral("PreviewImages"));
    }
    if (enableDatasheet) {
        initTypeProgress(QStringLiteral("Datasheet"));
    }

    const auto markMissingDataFailures = [this, &missingDataComponentIds](const QString& typeName) {
        for (const QString& componentId : missingDataComponentIds) {
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = QStringLiteral("Component preload data missing");
            onExportItemStatusChanged(componentId, typeName, status);
        }
    };

    if (!missingDataComponentIds.isEmpty()) {
        if (enableSymbol) {
            markMissingDataFailures(QStringLiteral("Symbol"));
        }
        if (enableFootprint) {
            markMissingDataFailures(QStringLiteral("Footprint"));
        }
        if (enableModel3D) {
            markMissingDataFailures(QStringLiteral("Model3D"));
        }
        if (enablePreview) {
            markMissingDataFailures(QStringLiteral("PreviewImages"));
        }
        if (enableDatasheet) {
            markMissingDataFailures(QStringLiteral("Datasheet"));
        }
    }

    m_runningExportStages = (enableSymbol ? 1 : 0) + (enableFootprint ? 1 : 0) + (enableModel3D ? 1 : 0) +
                            (enablePreview ? 1 : 0) + (enableDatasheet ? 1 : 0);
    logNetworkRuntimeStats(QStringLiteral("export-start"));

    if (exportableComponentIds.isEmpty()) {
        qWarning() << "ParallelExportService: No exportable components after preload";
        m_progress.currentStage = ExportOverallProgress::Stage::Failed;
        m_progress.endTime = QDateTime::currentDateTime();
        writeExportDetailedReport(QStringLiteral("export-failed-no-exportable-components"));
        emit failed(QStringLiteral("No exportable components after preload"));
        cleanupExportStages();
        return;
    }

    // 创建并启动各导出类型的Stage
    if (enableSymbol) {
        auto* stage = new SymbolExportStage(this);
        stage->setOptions(m_options);
        m_exportStages[QStringLiteral("Symbol")] = stage;

        connect(stage,
                &SymbolExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Symbol"), status);
                });
        connect(stage, &SymbolExportStage::progressChanged, this, [this](const ExportTypeProgress& progress) {
            onExportTypeProgressChanged(QStringLiteral("Symbol"), progress);
        });
        connect(stage, &SymbolExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Symbol"), success, failed, skipped);
        });
        stage->start(exportableComponentIds, m_cachedData);
    }

    if (enableFootprint) {
        auto* stage = new FootprintExportStage(this);
        stage->setOptions(m_options);
        m_exportStages[QStringLiteral("Footprint")] = stage;

        connect(stage,
                &FootprintExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Footprint"), status);
                });
        connect(stage, &FootprintExportStage::progressChanged, this, [this](const ExportTypeProgress& progress) {
            onExportTypeProgressChanged(QStringLiteral("Footprint"), progress);
        });
        connect(stage, &FootprintExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Footprint"), success, failed, skipped);
        });
        stage->start(exportableComponentIds, m_cachedData);
    }

    if (enableModel3D) {
        auto* stage = new Model3DExportStage(this);
        stage->setOptions(m_options);
        m_exportStages[QStringLiteral("Model3D")] = stage;

        connect(stage,
                &Model3DExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
                });
        connect(stage, &Model3DExportStage::progressChanged, this, [this](const ExportTypeProgress& progress) {
            onExportTypeProgressChanged(QStringLiteral("Model3D"), progress);
        });
        connect(stage, &Model3DExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Model3D"), success, failed, skipped);
        });
        stage->start(exportableComponentIds, m_cachedData);
    }

    if (enablePreview) {
        auto* stage = new PreviewImagesExportStage(this);
        stage->setOptions(m_options);
        m_exportStages[QStringLiteral("PreviewImages")] = stage;

        connect(stage,
                &PreviewImagesExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("PreviewImages"), status);
                });
        connect(stage, &PreviewImagesExportStage::progressChanged, this, [this](const ExportTypeProgress& progress) {
            onExportTypeProgressChanged(QStringLiteral("PreviewImages"), progress);
        });
        connect(stage, &PreviewImagesExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("PreviewImages"), success, failed, skipped);
        });
        stage->start(exportableComponentIds, m_cachedData);
    }

    if (enableDatasheet) {
        auto* stage = new DatasheetExportStage(this);
        stage->setOptions(m_options);
        m_exportStages[QStringLiteral("Datasheet")] = stage;

        connect(stage,
                &DatasheetExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Datasheet"), status);
                });
        connect(stage, &DatasheetExportStage::progressChanged, this, [this](const ExportTypeProgress& progress) {
            onExportTypeProgressChanged(QStringLiteral("Datasheet"), progress);
        });
        connect(stage, &DatasheetExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Datasheet"), success, failed, skipped);
        });
        stage->start(exportableComponentIds, m_cachedData);
    }

    if (m_runningExportStages == 0) {
        qWarning() << "ParallelExportService: No export types enabled";
        m_progress.currentStage = ExportOverallProgress::Stage::Completed;
        m_progress.endTime = QDateTime::currentDateTime();
        logNetworkRuntimeStats(QStringLiteral("export-no-types-enabled"));
        writeExportDetailedReport(QStringLiteral("export-no-types-enabled"));
        emit completed(0, 0);
    }
}

void ParallelExportService::cancelExport() {
    qDebug() << "ParallelExportService: Cancelling export";
    m_cancelRequested = true;

    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        cancelPreload();
        return;
    }

    for (auto* stage : m_exportStages) {
        stage->cancel();
    }

    m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
    m_progress.endTime = QDateTime::currentDateTime();
    logNetworkRuntimeStats(QStringLiteral("export-cancelled"));
    writeExportDetailedReport(QStringLiteral("export-cancelled"));
    emit cancelled();
}

ExportOverallProgress ParallelExportService::getProgress() const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress;
}

ExportTypeProgress ParallelExportService::getTypeProgress(const QString& typeName) const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress.exportTypeProgress.value(typeName);
}

bool ParallelExportService::isRunning() const {
    return m_progress.currentStage == ExportOverallProgress::Stage::Preloading ||
           m_progress.currentStage == ExportOverallProgress::Stage::Exporting;
}

void ParallelExportService::onPreloadItemCompleted(const QString& componentId, bool success, const QString& error) {
    QMutexLocker locker(&m_progressMutex);

    m_progress.preloadProgress.inProgressCount--;

    if (success) {
        m_progress.preloadProgress.successCount++;
    } else {
        m_progress.preloadProgress.failedCount++;
        m_progress.preloadProgress.failedComponents[componentId] = error;
    }

    m_progress.preloadProgress.completedCount++;

    locker.unlock();
    emit preloadProgressChanged(m_progress.preloadProgress);

    if (m_progress.preloadProgress.completedCount >= m_progress.preloadProgress.totalCount) {
        m_preloadCompleted = true;
        m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        emit preloadCompleted(m_progress.preloadProgress.successCount, m_progress.preloadProgress.failedCount);
    }
}

void ParallelExportService::onExportTypeProgressChanged(const QString& typeName, const ExportTypeProgress& progress) {
    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.exportTypeProgress[typeName] = progress;
    }

    updateOverallProgress();
}

void ParallelExportService::onExportTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    QMutexLocker locker(&m_progressMutex);

    ExportTypeProgress typeProgress = m_progress.exportTypeProgress.value(typeName);
    typeProgress.typeName = typeName;
    typeProgress.totalCount = m_componentIds.size();
    typeProgress.successCount = successCount;
    typeProgress.failedCount = failedCount;
    typeProgress.skippedCount = skippedCount;
    typeProgress.completedCount = successCount + failedCount + skippedCount;
    typeProgress.inProgressCount = 0;

    m_progress.exportTypeProgress[typeName] = typeProgress;

    if (m_runningExportStages > 0) {
        m_runningExportStages--;
    }

    locker.unlock();
    emit typeCompleted(typeName, successCount, failedCount, skippedCount);

    if (m_exportStages.contains(typeName)) {
        auto* finishedStage = m_exportStages.take(typeName);
        if (finishedStage) {
            finishedStage->deleteLater();
        }
    }

    checkAllExportCompleted();
}

void ParallelExportService::onExportItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    QMutexLocker locker(&m_progressMutex);

    if (!m_progress.exportTypeProgress.contains(typeName)) {
        m_progress.exportTypeProgress[typeName] = ExportTypeProgress();
        m_progress.exportTypeProgress[typeName].typeName = typeName;
        m_progress.exportTypeProgress[typeName].totalCount = m_componentIds.size();
    }

    ExportTypeProgress& typeProgress = m_progress.exportTypeProgress[typeName];
    typeProgress.itemStatus[componentId] = status;
    recomputeTypeProgressCounts(typeProgress);

    locker.unlock();
    emit itemStatusChanged(componentId, typeName, status);

    // 更新整体进度
    updateOverallProgress();
}

void ParallelExportService::processNextPreloadBatch() {
    if (m_cancelRequested || m_progress.currentStage != ExportOverallProgress::Stage::Preloading) {
        return;
    }

    constexpr int BATCH_SIZE = 8;

    if (!m_componentService) {
        qWarning() << "ParallelExportService: ComponentService not set, cannot load data";
        {
            QMutexLocker locker(&m_progressMutex);
            m_progress.preloadProgress.completedCount = m_componentIds.size();
            m_progress.preloadProgress.failedCount = m_componentIds.size();
            m_progress.preloadProgress.inProgressCount = 0;
            m_progress.preloadProgress.currentComponentId.clear();
            m_progress.currentStage = ExportOverallProgress::Stage::Idle;
            m_progress.endTime = QDateTime::currentDateTime();
        }
        emit preloadProgressChanged(m_progress.preloadProgress);
        logNetworkRuntimeStats(QStringLiteral("preload-failed-no-component-service"));
        writeExportDetailedReport(QStringLiteral("preload-failed-no-component-service"));
        emit preloadCompleted(0, m_componentIds.size());
        return;
    }

    int processedInBatch = 0;
    while (processedInBatch < BATCH_SIZE && m_nextPreloadIndex < m_componentIds.size() && !m_cancelRequested) {
        const QString componentId = m_componentIds.at(m_nextPreloadIndex++);
        {
            QMutexLocker locker(&m_progressMutex);
            m_progress.preloadProgress.currentComponentId = componentId;
            m_progress.preloadProgress.inProgressCount = 1;
        }

        ComponentData data = m_componentService->getComponentData(componentId);
        const QSharedPointer<ComponentData> diskCachedData = loadDiskCachedComponentData(componentId);
        mergeComponentData(data, diskCachedData);

        {
            QMutexLocker locker(&m_progressMutex);
            if (!data.lcscId().isEmpty()) {
                auto sharedData = QSharedPointer<ComponentData>::create(data);
                m_cachedData[componentId] = sharedData;
                m_progress.preloadProgress.successCount++;
                qDebug() << "ParallelExportService: Loaded data for" << componentId;
            } else {
                m_progress.preloadProgress.failedCount++;
                m_progress.preloadProgress.failedComponents[componentId] = QStringLiteral("No component data found");
                qWarning() << "ParallelExportService: No data found for component:" << componentId;
            }

            m_progress.preloadProgress.completedCount++;
            m_progress.preloadProgress.inProgressCount = 0;
        }

        processedInBatch++;
    }

    const bool finished = m_nextPreloadIndex >= m_componentIds.size();
    if (finished) {
        {
            QMutexLocker locker(&m_progressMutex);
            m_progress.preloadProgress.currentComponentId.clear();
            m_progress.currentStage = ExportOverallProgress::Stage::Idle;
            m_progress.endTime = QDateTime::currentDateTime();
        }
        qDebug() << "ParallelExportService: Preload completed. Success:" << m_progress.preloadProgress.successCount
                 << "Failed:" << m_progress.preloadProgress.failedCount;
        m_preloadCompleted = true;
    }

    emit preloadProgressChanged(m_progress.preloadProgress);
    updateOverallProgress();

    if (finished) {
        emit preloadCompleted(m_progress.preloadProgress.successCount, m_progress.preloadProgress.failedCount);
        return;
    }

    QTimer::singleShot(0, this, &ParallelExportService::processNextPreloadBatch);
}

void ParallelExportService::onAllComponentDataCollected(const QList<ComponentData>& componentDataList) {
    // 断开连接，避免重复处理
    disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);

    // 检查是否已请求取消 - 如果是则直接返回，避免覆盖取消状态
    {
        QMutexLocker locker(&m_progressMutex);
        if (m_cancelRequested) {
            qDebug() << "ParallelExportService: Ignoring late callback after cancel requested";
            return;
        }
    }

    qDebug() << "ParallelExportService: Received" << componentDataList.size() << "component data from parallel fetch";

    // 统计成功和失败数量
    int successCount = 0;
    int failedCount = 0;

    // 处理每个组件数据
    for (const ComponentData& data : componentDataList) {
        const QString componentId = data.lcscId();

        if (componentId.isEmpty()) {
            failedCount++;
            continue;
        }

        // 检查数据是否有效（至少需要有 symbol 或 footprint 数据）
        bool hasValidData = (data.symbolData() != nullptr || data.footprintData() != nullptr);

        if (hasValidData) {
            auto sharedData = QSharedPointer<ComponentData>::create(data);
            m_cachedData[componentId] = sharedData;
            successCount++;
            qDebug() << "ParallelExportService: Cached data for" << componentId;
        } else {
            failedCount++;
            m_progress.preloadProgress.failedComponents[componentId] = QStringLiteral("No valid data found");
            qWarning() << "ParallelExportService: No valid data for component:" << componentId;
        }
    }

    // 更新进度
    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.successCount = successCount;
        m_progress.preloadProgress.failedCount = failedCount;
        m_progress.preloadProgress.completedCount = componentDataList.size();
        m_progress.preloadProgress.inProgressCount = 0;
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        m_progress.endTime = QDateTime::currentDateTime();
    }

    qDebug() << "ParallelExportService: Parallel preload completed. Success:" << successCount
             << "Failed:" << failedCount;
    logNetworkRuntimeStats(QStringLiteral("preload-completed"));
    writeExportDetailedReport(QStringLiteral("preload-completed"));

    m_preloadCompleted = true;
    emit preloadProgressChanged(m_progress.preloadProgress);
    emit preloadCompleted(successCount, failedCount);
}

void ParallelExportService::updateOverallProgress() {
    ExportOverallProgress progressSnapshot;
    {
        QMutexLocker locker(&m_progressMutex);
        progressSnapshot = m_progress;
    }

    emit progressChanged(progressSnapshot);
}

void ParallelExportService::checkAllExportCompleted() {
    if (m_runningExportStages > 0) {
        return;
    }

    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        qDebug() << "ParallelExportService: Ignoring completion because export was cancelled";
        cleanupExportStages();
        return;
    }

    qDebug() << "ParallelExportService: All export stages completed";

    m_progress.currentStage = ExportOverallProgress::Stage::Completed;
    m_progress.endTime = QDateTime::currentDateTime();

    int totalSuccess = 0;
    int totalFailed = 0;
    for (const QString& componentId : m_componentIds) {
        bool anyFailed = false;
        bool allDone = true;

        for (auto it = m_progress.exportTypeProgress.cbegin(); it != m_progress.exportTypeProgress.cend(); ++it) {
            const ExportItemStatus itemStatus =
                it.value().itemStatus.value(componentId, ExportItemStatus{ExportItemStatus::Status::Pending});
            if (itemStatus.status == ExportItemStatus::Status::Failed) {
                anyFailed = true;
            }
            if (itemStatus.status != ExportItemStatus::Status::Success &&
                itemStatus.status != ExportItemStatus::Status::Failed &&
                itemStatus.status != ExportItemStatus::Status::Skipped) {
                allDone = false;
            }
        }

        if (!allDone) {
            continue;
        }
        if (anyFailed) {
            totalFailed++;
        } else {
            totalSuccess++;
        }
    }

    logNetworkRuntimeStats(QStringLiteral("export-completed"));
    writeExportDetailedReport(QStringLiteral("export-completed"));
    emit completed(totalSuccess, totalFailed);
    cleanupExportStages();
}

void ParallelExportService::logNetworkRuntimeStats(const QString& context) const {
    const QString snapshot = NetworkClient::instance().formatRuntimeStats();
    qInfo().noquote() << QStringLiteral("ParallelExportService network runtime stats [%1]\n%2").arg(context, snapshot);
}

void ParallelExportService::writeExportDetailedReport(const QString& reason) const {
    if (m_options.outputPath.trimmed().isEmpty()) {
        return;
    }

    QDir outputDir(m_options.outputPath);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        qWarning() << "ParallelExportService: Failed to create diagnostics report directory:" << outputDir.path();
        return;
    }

    const QString reportPath = outputDir.filePath(QStringLiteral("easykiconverter_export_detailed_report.md"));
    QSaveFile file(reportPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ParallelExportService: Failed to open export detailed report for writing:" << reportPath;
        return;
    }

    ExportOverallProgress progressSnapshot;
    {
        QMutexLocker locker(&m_progressMutex);
        progressSnapshot = m_progress;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "# EasyKiConverter Export Detailed Report\n\n";
    out << "- Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "- Reason: " << reason << "\n";
    out << "- Weak network support: " << (m_options.weakNetworkSupport ? "enabled" : "disabled") << "\n";
    out << "- Output path: " << m_options.outputPath << "\n";
    out << "- Current stage: " << static_cast<int>(progressSnapshot.currentStage) << "\n";
    out << "- Start time: " << progressSnapshot.startTime.toString(Qt::ISODate) << "\n";
    out << "- End time: " << progressSnapshot.endTime.toString(Qt::ISODate) << "\n";
    out << "- Total components: " << progressSnapshot.totalComponents << "\n\n";

    out << "## Export Options\n\n";
    out << "- Library name: " << m_options.libName << "\n";
    out << "- Export symbol: " << (m_options.exportSymbol ? "yes" : "no") << "\n";
    out << "- Export footprint: " << (m_options.exportFootprint ? "yes" : "no") << "\n";
    out << "- Export 3D model: " << (m_options.exportModel3D ? "yes" : "no") << "\n";
    out << "- Export preview images: " << (m_options.exportPreviewImages ? "yes" : "no") << "\n";
    out << "- Export datasheet: " << (m_options.exportDatasheet ? "yes" : "no") << "\n";
    out << "- Overwrite existing files: " << (m_options.overwriteExistingFiles ? "yes" : "no") << "\n";
    out << "- Update mode: " << (m_options.updateMode ? "yes" : "no") << "\n";
    out << "- Debug mode: " << (m_options.debugMode ? "yes" : "no") << "\n\n";

    out << "## Preload\n\n";
    out << "- Completed: " << progressSnapshot.preloadProgress.completedCount << "/"
        << progressSnapshot.preloadProgress.totalCount << "\n";
    out << "- Success: " << progressSnapshot.preloadProgress.successCount << "\n";
    out << "- Failed: " << progressSnapshot.preloadProgress.failedCount << "\n";
    out << "- In progress: " << progressSnapshot.preloadProgress.inProgressCount << "\n";
    if (!progressSnapshot.preloadProgress.failedComponents.isEmpty()) {
        out << "\n### Preload Failures\n\n";
        for (auto it = progressSnapshot.preloadProgress.failedComponents.cbegin();
             it != progressSnapshot.preloadProgress.failedComponents.cend();
             ++it) {
            out << "- `" << it.key() << "`: " << it.value() << "\n";
        }
    }

    out << "\n## Export Type Progress\n\n";
    for (auto it = progressSnapshot.exportTypeProgress.cbegin(); it != progressSnapshot.exportTypeProgress.cend();
         ++it) {
        const ExportTypeProgress& typeProgress = it.value();
        out << "### " << it.key() << "\n\n";
        out << "- Completed: " << typeProgress.completedCount << "/" << typeProgress.totalCount << "\n";
        out << "- Success: " << typeProgress.successCount << "\n";
        out << "- Failed: " << typeProgress.failedCount << "\n";
        out << "- Skipped: " << typeProgress.skippedCount << "\n";
        out << "- In progress: " << typeProgress.inProgressCount << "\n";
    }

    out << "\n## Weak Network Diagnostics\n\n";
    out << "### Network Runtime Stats\n\n```\n";
    out << NetworkClient::instance().formatRuntimeStats() << "\n";
    out << "```\n";

    if (!file.commit()) {
        qWarning() << "ParallelExportService: Failed to commit export detailed report:" << reportPath;
        return;
    }

    qInfo() << "ParallelExportService: Export detailed report written to" << reportPath;
}

}  // namespace EasyKiConverter
