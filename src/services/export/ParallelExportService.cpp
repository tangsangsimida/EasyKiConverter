#include "ParallelExportService.h"

#include "ComponentService.h"
#include "DatasheetExportStage.h"
#include "FootprintExportStage.h"
#include "Model3DExportStage.h"
#include "PreviewImagesExportStage.h"
#include "SymbolExportStage.h"
#include "core/network/NetworkClient.h"
#include "services/ComponentCacheService.h"
#include "services/export/ExportReportGenerator.h"
#include "services/export/ExportWorkerHelpers.h"

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

namespace EasyKiConverter {

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
            if (stage->isRunning()) {
                stage->setParent(this);
            } else {
                stage->deleteLater();
            }
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

    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
    }

    cleanupExportStages();
    m_runningExportStages = 0;
    ++m_activeRunGeneration;
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
    ++m_activeRunGeneration;
    m_cancelRequested = true;
    m_nextPreloadIndex = m_componentIds.size();
    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
    }
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
    const quint64 runGeneration = m_activeRunGeneration;

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
                [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportItemStatusChanged(componentId, QStringLiteral("Symbol"), status);
                });
        connect(stage,
                &SymbolExportStage::progressChanged,
                this,
                [this, runGeneration](const ExportTypeProgress& progress) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportTypeProgressChanged(QStringLiteral("Symbol"), progress);
                });
        connect(stage,
                &SymbolExportStage::completed,
                this,
                [this, stage, runGeneration](int success, int failed, int skipped) {
                    if (runGeneration != m_activeRunGeneration) {
                        stage->deleteLater();
                        return;
                    }
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
                [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportItemStatusChanged(componentId, QStringLiteral("Footprint"), status);
                });
        connect(stage,
                &FootprintExportStage::progressChanged,
                this,
                [this, runGeneration](const ExportTypeProgress& progress) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportTypeProgressChanged(QStringLiteral("Footprint"), progress);
                });
        connect(stage,
                &FootprintExportStage::completed,
                this,
                [this, stage, runGeneration](int success, int failed, int skipped) {
                    if (runGeneration != m_activeRunGeneration) {
                        stage->deleteLater();
                        return;
                    }
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
                [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
                });
        connect(stage,
                &Model3DExportStage::progressChanged,
                this,
                [this, runGeneration](const ExportTypeProgress& progress) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportTypeProgressChanged(QStringLiteral("Model3D"), progress);
                });
        connect(stage,
                &Model3DExportStage::completed,
                this,
                [this, stage, runGeneration](int success, int failed, int skipped) {
                    if (runGeneration != m_activeRunGeneration) {
                        stage->deleteLater();
                        return;
                    }
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
                [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportItemStatusChanged(componentId, QStringLiteral("PreviewImages"), status);
                });
        connect(stage,
                &PreviewImagesExportStage::progressChanged,
                this,
                [this, runGeneration](const ExportTypeProgress& progress) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportTypeProgressChanged(QStringLiteral("PreviewImages"), progress);
                });
        connect(stage,
                &PreviewImagesExportStage::completed,
                this,
                [this, stage, runGeneration](int success, int failed, int skipped) {
                    if (runGeneration != m_activeRunGeneration) {
                        stage->deleteLater();
                        return;
                    }
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
                [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportItemStatusChanged(componentId, QStringLiteral("Datasheet"), status);
                });
        connect(stage,
                &DatasheetExportStage::progressChanged,
                this,
                [this, runGeneration](const ExportTypeProgress& progress) {
                    if (runGeneration != m_activeRunGeneration) {
                        return;
                    }
                    onExportTypeProgressChanged(QStringLiteral("Datasheet"), progress);
                });
        connect(stage,
                &DatasheetExportStage::completed,
                this,
                [this, stage, runGeneration](int success, int failed, int skipped) {
                    if (runGeneration != m_activeRunGeneration) {
                        stage->deleteLater();
                        return;
                    }
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
    ++m_activeRunGeneration;
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
    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        return;
    }

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
    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        return;
    }

    QMutexLocker locker(&m_progressMutex);

    if (!m_progress.exportTypeProgress.contains(typeName)) {
        m_progress.exportTypeProgress[typeName] = ExportTypeProgress();
        m_progress.exportTypeProgress[typeName].typeName = typeName;
        m_progress.exportTypeProgress[typeName].totalCount = m_componentIds.size();
    }

    ExportTypeProgress& typeProgress = m_progress.exportTypeProgress[typeName];
    typeProgress.itemStatus[componentId] = status;
    ExportWorkerHelpers::recomputeTypeProgressCounts(typeProgress);

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
        const QSharedPointer<ComponentData> diskCachedData =
            ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
        ExportWorkerHelpers::mergeComponentData(data, diskCachedData);

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
    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
    }

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
    ExportReportGenerator::logNetworkStats(context);
}

void ParallelExportService::writeExportDetailedReport(const QString& reason) const {
    // 获取进度快照（线程安全）
    ExportOverallProgress progressSnapshot;
    {
        QMutexLocker locker(&m_progressMutex);
        progressSnapshot = m_progress;
    }
    ExportReportGenerator::writeDetailedReport(reason, m_options, progressSnapshot);
}

}  // namespace EasyKiConverter
