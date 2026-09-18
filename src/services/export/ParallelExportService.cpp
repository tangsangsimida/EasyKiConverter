#include "ParallelExportService.h"

#include "ComponentService.h"
#include "DatasheetExportStage.h"
#include "ExportRunPlan.h"
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
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QTimer>

namespace EasyKiConverter {

ParallelExportService::ParallelExportService(QObject* parent) : QObject(parent), m_progress() {
    m_progress.currentStage = ExportOverallProgress::Stage::Idle;
}

ParallelExportService::~ParallelExportService() {
    m_cancelRequested = true;
    ++m_activeRunGeneration;
    NetworkClient::instance().cancelAllRequests();

    QList<ExportTypeStage*> stagesToStop;
    const auto appendUniqueStage = [&stagesToStop](ExportTypeStage* stage) {
        if (stage && !stagesToStop.contains(stage)) {
            stagesToStop.append(stage);
        }
    };

    for (auto* stage : std::as_const(m_exportStages)) {
        appendUniqueStage(stage);
    }
    for (const QPointer<ExportTypeStage>& stage : std::as_const(m_retiredExportStages)) {
        appendUniqueStage(stage.data());
    }

    m_exportStages.clear();
    m_retiredExportStages.clear();

    for (ExportTypeStage* stage : std::as_const(stagesToStop)) {
        stage->cancel();
    }
    for (ExportTypeStage* stage : std::as_const(stagesToStop)) {
        stage->waitForFinished(30000);
        delete stage;
    }
}

// 清理已经完成或需要退出的导出阶段。
void ParallelExportService::cleanupExportStages() {
    for (auto* stage : m_exportStages) {
        if (stage) {
            if (stage->isRunning() || stage->hasActiveWorkers()) {
                retireExportStage(stage);
            } else {
                stage->deleteLater();
            }
        }
    }
    m_exportStages.clear();
    cleanupRetiredExportStages();
}

// 从阶段表中移除指定阶段并安排安全销毁。
void ParallelExportService::discardExportStage(const QString& typeName, ExportTypeStage* stage) {
    if (m_exportStages.value(typeName) == stage) {
        m_exportStages.remove(typeName);
    }
    if (stage) {
        if (stage->isRunning() || stage->hasActiveWorkers()) {
            retireExportStage(stage);
        } else {
            for (int i = m_retiredExportStages.size() - 1; i >= 0; --i) {
                if (m_retiredExportStages.at(i).data() == stage || m_retiredExportStages.at(i).isNull()) {
                    m_retiredExportStages.removeAt(i);
                }
            }
            stage->deleteLater();
        }
    }
    cleanupRetiredExportStages();
}

// 将仍有活动任务的阶段转入延迟回收列表。
void ParallelExportService::retireExportStage(ExportTypeStage* stage) {
    if (!stage) {
        return;
    }

    stage->setParent(this);
    for (const QPointer<ExportTypeStage>& retiredStage : std::as_const(m_retiredExportStages)) {
        if (retiredStage.data() == stage) {
            return;
        }
    }
    m_retiredExportStages.append(QPointer<ExportTypeStage>(stage));

    QPointer<ExportTypeStage> stagePtr(stage);
    connect(stage, &ExportTypeStage::completed, this, [this, stagePtr](int, int, int) {
        if (!stagePtr) {
            cleanupRetiredExportStages();
            return;
        }
        discardExportStage(stagePtr->typeName(), stagePtr.data());
    });
}

// 回收已经停止且没有活动任务的阶段。
void ParallelExportService::cleanupRetiredExportStages() {
    for (int i = m_retiredExportStages.size() - 1; i >= 0; --i) {
        ExportTypeStage* stage = m_retiredExportStages.at(i).data();
        if (!stage) {
            m_retiredExportStages.removeAt(i);
            continue;
        }
        if (stage->isRunning() || stage->hasActiveWorkers()) {
            continue;
        }
        m_retiredExportStages.removeAt(i);
        stage->deleteLater();
    }
}

// 设置后续预加载和导出流程使用的选项。
void ParallelExportService::setOptions(const ExportOptions& options) {
    m_options = options;
}

// 更新导出输出目录。
void ParallelExportService::setOutputPath(const QString& path) {
    m_options.outputPath = path;
}

// 注入负责网络和组件数据获取的服务。
void ParallelExportService::setComponentService(ComponentService* componentService) {
    m_componentService = componentService;
}

// 启动组件预加载，并在没有网络服务时使用磁盘缓存回退。
void ParallelExportService::startPreload(const QStringList& componentIds) {
    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        qWarning() << "ParallelExportService: Preload already in progress";
        return;
    }

    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
        disconnect(m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, this, nullptr);
    }

    QStringList normalizedComponentIds;
    QSet<QString> seenComponentIds;
    for (const QString& componentId : componentIds) {
        const QString normalizedId = componentId.trimmed().toUpper();
        if (!normalizedId.isEmpty() && !seenComponentIds.contains(normalizedId)) {
            seenComponentIds.insert(normalizedId);
            normalizedComponentIds.append(normalizedId);
        }
    }

    cleanupExportStages();
    m_runningExportStages = 0;
    ++m_activeRunGeneration;
    m_cancelRequested = false;
    // 新一轮验证/导出开始，解除全局 tombstone
    // 旧回调靠 generation token 被拒绝，新请求携带新 token 可正常写入
    ComponentCacheService::instance()->clearGlobalTombstone();
    qDebug() << "ParallelExportService: Starting preload for" << normalizedComponentIds.size() << "components";

    m_componentIds = normalizedComponentIds;
    m_progress.currentStage = ExportOverallProgress::Stage::Preloading;
    m_progress.totalComponents = m_componentIds.size();
    m_progress.startTime = QDateTime::currentDateTime();
    m_progress.preloadProgress = PreloadProgress();
    m_progress.preloadProgress.totalCount = m_componentIds.size();
    m_cachedData.clear();
    m_preloadCompleted = false;
    m_nextPreloadIndex = 0;

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.preloadProgress.inProgressCount = 0;
    }

    if (m_componentIds.isEmpty()) {
        m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        m_progress.endTime = QDateTime::currentDateTime();
        m_preloadCompleted = true;
        emit preloadProgressChanged(m_progress.preloadProgress);
        updateOverallProgress();
        emit preloadCompleted(0, 0);
        return;
    }

    emit preloadProgressChanged(m_progress.preloadProgress);
    updateOverallProgress();
    logNetworkRuntimeStats(QStringLiteral("preload-start"));

    // 使用 ComponentService 的并行获取能力来加载数据
    // 这样可以利用配置的并发数（默认10个并行请求）
    if (m_componentService) {
        // 连接到 ComponentService 的信号以接收数据
        connect(m_componentService,
                &ComponentService::allComponentsDataCollectedWithErrors,
                this,
                &ParallelExportService::onAllComponentDataCollectedWithErrors,
                Qt::UniqueConnection);

        // 启动并行获取
        m_componentService->fetchMultipleComponentsData(m_componentIds, m_options.exportModel3D);

        qDebug() << "ParallelExportService: Started parallel fetch for" << m_componentIds.size() << "components";
    } else {
        // 如果没有 ComponentService，直接从本地缓存读取
        qWarning() << "ParallelExportService: ComponentService not set, using sync cache fallback";
        processNextPreloadBatch();
    }
}

// 取消当前预加载并收敛其进度状态。
void ParallelExportService::cancelPreload() {
    qDebug() << "ParallelExportService: Cancelling preload";
    ++m_activeRunGeneration;
    m_cancelRequested = true;
    m_nextPreloadIndex = m_componentIds.size();
    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollected, this, nullptr);
        disconnect(m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, this, nullptr);
        m_componentService->abortBatchFetch();
    }
    {
        QMutexLocker locker(&m_progressMutex);
        m_progress.preloadProgress.inProgressCount = 0;
        m_progress.preloadProgress.currentComponentId.clear();
        m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
        m_progress.endTime = QDateTime::currentDateTime();
    }
    updateOverallProgress();
    emit cancelled();
    logNetworkRuntimeStats(QStringLiteral("preload-cancelled"));
    writeExportDetailedReport(QStringLiteral("preload-cancelled"));
}

void ParallelExportService::registerStageAndStart(ExportTypeStage* stage,
                                                  const QString& typeName,
                                                  const QStringList& componentIds,
                                                  quint64 runGeneration) {
    QPointer<ExportTypeStage> stagePtr(stage);
    m_exportStages[typeName] = stage;

    connect(stage,
            &ExportTypeStage::itemStatusChanged,
            this,
            [this, typeName, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                if (runGeneration != m_activeRunGeneration) {
                    return;
                }
                onExportItemStatusChanged(componentId, typeName, status);
            });
    connect(stage,
            &ExportTypeStage::progressChanged,
            this,
            [this, typeName, runGeneration](const ExportTypeProgress& progress) {
                if (runGeneration != m_activeRunGeneration) {
                    return;
                }
                onExportTypeProgressChanged(typeName, progress);
            });
    connect(stage,
            &ExportTypeStage::completed,
            this,
            [this, stagePtr, typeName, runGeneration](int success, int failed, int skipped) {
                if (runGeneration != m_activeRunGeneration) {
                    if (stagePtr) {
                        discardExportStage(typeName, stagePtr.data());
                    }
                    return;
                }
                onExportTypeCompleted(typeName, success, failed, skipped);
            });

    if (typeName == QStringLiteral("Footprint") && m_options.exportModel3D &&
        m_options.targetFormat == TargetEdaFormat::Altium) {
        if (auto* footprintStage = qobject_cast<FootprintExportStage*>(stage)) {
            connect(footprintStage,
                    &FootprintExportStage::embeddedModel3DStatusChanged,
                    this,
                    [this, runGeneration](const QString& componentId, const ExportItemStatus& status) {
                        if (runGeneration == m_activeRunGeneration) {
                            onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
                        }
                    });
        }
    }
    stage->start(componentIds, m_cachedData);
}

// 根据预加载结果启动各导出阶段。
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
    // 预加载完成，旧回调已全部处理，解除全局 tombstone
    ComponentCacheService::instance()->clearGlobalTombstone();
    // 从 ComponentService 刷新最新的组件数据（确保用户在 UI 中的编辑生效）
    if (m_componentService) {
        for (auto it = m_cachedData.begin(); it != m_cachedData.end(); ++it) {
            ComponentData freshData = m_componentService->getComponentData(it.key());
            if (freshData.symbolData() && it.value() && it.value()->symbolData()) {
                it.value()->symbolData()->setInfo(freshData.symbolData()->info());
            }
            if (freshData.footprintData() && it.value() && it.value()->footprintData()) {
                it.value()->footprintData()->setInfo(freshData.footprintData()->info());
            }
        }
    }
    const quint64 runGeneration = m_activeRunGeneration;

    qDebug() << "ParallelExportService: Starting export for" << m_componentIds.size() << "components"
             << (m_options.retryMode ? "(retry mode)" : "");

    m_progress.currentStage = ExportOverallProgress::Stage::Exporting;
    m_progress.startTime = QDateTime::currentDateTime();
    m_progress.exportTypeProgress.clear();
    m_runningExportStages = 0;

    // Altium PcbLib 将 STEP 直接嵌入 Library/Models，不生成 KiCad 风格的
    // 外部 .3dmodels 目录；封装阶段仍会按需获取 STEP 并交给写入器。
    // Model3D 统计项仍需保留，Altium 下由封装阶段镜像完成状态。
    const ExportRunPlan plan = buildExportRunPlan(m_options, m_componentIds, m_cachedData);
    const bool enableSymbol = plan.enableSymbol;
    const bool enableFootprint = plan.enableFootprint;
    const bool enableModel3D = plan.enableModel3D;
    const bool runExternalModel3DStage = plan.runExternalModel3DStage;
    const bool enablePreview = plan.enablePreview;
    const bool enableDatasheet = plan.enableDatasheet;
    if (m_options.exportModel3D && m_options.targetFormat == TargetEdaFormat::Xpedition) {
        qWarning() << "ParallelExportService: Xpedition 目标当前不支持 3D 模型关联，已跳过 3D 导出阶段";
    }
    if (!plan.missingDataComponentIds.isEmpty()) {
        qWarning() << "ParallelExportService: Missing preloaded component data for"
                   << plan.missingDataComponentIds.size() << "components:" << plan.missingDataComponentIds;
    }

    const auto initTypeProgress = [this](const QString& typeName) {
        ExportTypeProgress progress;
        progress.typeName = typeName;
        progress.totalCount = m_componentIds.size();
        m_progress.exportTypeProgress[typeName] = progress;
    };

    for (const QString& typeName : plan.progressTypeNames()) {
        initTypeProgress(typeName);
    }

    const auto markMissingDataFailures = [this, &plan](const QString& typeName) {
        for (const QString& componentId : plan.missingDataComponentIds) {
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = QStringLiteral("Component preload data missing");
            onExportItemStatusChanged(componentId, typeName, status);
        }
    };

    if (!plan.missingDataComponentIds.isEmpty()) {
        for (const QString& typeName : plan.progressTypeNames()) {
            markMissingDataFailures(typeName);
        }
    }

    m_runningExportStages = plan.runningStageCount();
    logNetworkRuntimeStats(QStringLiteral("export-start"));

    if (plan.exportableComponentIds.isEmpty()) {
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
        registerStageAndStart(stage, QStringLiteral("Symbol"), plan.exportableComponentIds, runGeneration);
    }

    if (enableFootprint) {
        auto* stage = new FootprintExportStage(this);
        stage->setOptions(m_options);
        registerStageAndStart(stage, QStringLiteral("Footprint"), plan.exportableComponentIds, runGeneration);
    }

    if (runExternalModel3DStage) {
        auto* stage = new Model3DExportStage(this);
        stage->setOptions(m_options);
        registerStageAndStart(stage, QStringLiteral("Model3D"), plan.exportableComponentIds, runGeneration);
    }

    if (enablePreview) {
        auto* stage = new PreviewImagesExportStage(this);
        stage->setOptions(m_options);
        registerStageAndStart(stage, QStringLiteral("PreviewImages"), plan.exportableComponentIds, runGeneration);
    }

    if (enableDatasheet) {
        auto* stage = new DatasheetExportStage(this);
        stage->setOptions(m_options);
        registerStageAndStart(stage, QStringLiteral("Datasheet"), plan.exportableComponentIds, runGeneration);
    }

    // 重试模式仅对本次导出生效，避免影响后续正常导出
    m_options.retryMode = false;

    if (m_runningExportStages == 0) {
        qWarning() << "ParallelExportService: No export types enabled";
        m_progress.currentStage = ExportOverallProgress::Stage::Completed;
        m_progress.endTime = QDateTime::currentDateTime();
        logNetworkRuntimeStats(QStringLiteral("export-no-types-enabled"));
        writeExportDetailedReport(QStringLiteral("export-no-types-enabled"));
        emit completed(0, 0);
    }
}

// 取消预加载或导出，并回收仍在运行的阶段。
void ParallelExportService::cancelExport() {
    qDebug() << "ParallelExportService: Cancelling export";
    ++m_activeRunGeneration;
    m_cancelRequested = true;

    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        cancelPreload();
        return;
    }

    // 重置 ComponentService 批处理上下文
    if (m_componentService) {
        m_componentService->abortBatchFetch();
    }

    m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
    m_progress.endTime = QDateTime::currentDateTime();
    emit cancelled();

    QList<QPointer<ExportTypeStage>> stagesToCancel;
    for (auto* stage : m_exportStages) {
        if (stage) {
            disconnect(stage, nullptr, this, nullptr);
            retireExportStage(stage);
            stagesToCancel.append(QPointer<ExportTypeStage>(stage));
        }
    }
    m_exportStages.clear();
    m_runningExportStages = 0;

    NetworkClient::instance().cancelAllRequests();
    for (const QPointer<ExportTypeStage>& stage : std::as_const(stagesToCancel)) {
        if (stage) {
            stage->cancel();
        }
    }
    cleanupRetiredExportStages();

    QTimer::singleShot(0, this, [this]() {
        logNetworkRuntimeStats(QStringLiteral("export-cancelled"));
        writeExportDetailedReport(QStringLiteral("export-cancelled"));
    });
}

// 获取线程安全的整体进度快照。
ExportOverallProgress ParallelExportService::getProgress() const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress;
}

// 获取指定导出类型的线程安全进度快照。
ExportTypeProgress ParallelExportService::getTypeProgress(const QString& typeName) const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress.exportTypeProgress.value(typeName);
}

// 判断预加载或导出流程是否仍在运行。
bool ParallelExportService::isRunning() const {
    return m_progress.currentStage == ExportOverallProgress::Stage::Preloading ||
           m_progress.currentStage == ExportOverallProgress::Stage::Exporting;
}

// 接收单个预加载项的完成结果并更新统计。
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

// 接收单个导出阶段的进度快照。
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

// 接收单个导出阶段的完成统计并触发总流程检查。
void ParallelExportService::onExportTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    Q_UNUSED(skippedCount);
    {
        QMutexLocker locker(&m_progressMutex);

        ExportTypeProgress typeProgress = m_progress.exportTypeProgress.value(typeName);
        typeProgress.typeName = typeName;
        typeProgress.totalCount = m_componentIds.size();
        typeProgress.inProgressCount = 0;

        // 完成信号的阶段计数只覆盖实际启动的任务，必须保留预加载失败项的逐项状态。
        // 以 itemStatus 重新计算，确保统计与界面逐项结果保持一致。
        ExportWorkerHelpers::recomputeTypeProgressCounts(typeProgress);

        m_progress.exportTypeProgress[typeName] = typeProgress;

        if (m_runningExportStages > 0) {
            m_runningExportStages--;
        }
    }

    emit typeCompleted(typeName, successCount, failedCount, skippedCount);

    if (auto* finishedStage = m_exportStages.value(typeName)) {
        discardExportStage(typeName, finishedStage);
    }

    checkAllExportCompleted();
}

// 合并单个元器件的导出状态并同步整体进度。
void ParallelExportService::onExportItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    if (m_cancelRequested || m_progress.currentStage == ExportOverallProgress::Stage::Cancelled) {
        return;
    }

    bool shouldMirrorAltiumModel3DStatus = false;
    QMutexLocker locker(&m_progressMutex);

    if (!m_progress.exportTypeProgress.contains(typeName)) {
        m_progress.exportTypeProgress[typeName] = ExportTypeProgress();
        m_progress.exportTypeProgress[typeName].typeName = typeName;
        m_progress.exportTypeProgress[typeName].totalCount = m_componentIds.size();
    }

    ExportTypeProgress& typeProgress = m_progress.exportTypeProgress[typeName];
    ExportItemStatus mergedStatus = status;
    const auto previousStatus = typeProgress.itemStatus.constFind(componentId);
    if (previousStatus != typeProgress.itemStatus.cend()) {
        for (const QString& diagnostic : previousStatus->diagnostics) {
            if (!mergedStatus.diagnostics.contains(diagnostic))
                mergedStatus.diagnostics.append(diagnostic);
        }
    }
    typeProgress.itemStatus[componentId] = mergedStatus;
    ExportWorkerHelpers::recomputeTypeProgressCounts(typeProgress);

    // FootprintExportStage 会先发出真实的嵌入结果，再发出封装结果。
    // 只有在 3D 尚未收到独立结果时才使用封装状态兜底，避免封装成功覆盖
    // STEP 缺失等真实的 Model3D 失败状态。
    if (typeName == QStringLiteral("Footprint") && m_options.exportModel3D &&
        m_options.targetFormat == TargetEdaFormat::Altium) {
        const auto modelProgress = m_progress.exportTypeProgress.constFind(QStringLiteral("Model3D"));
        shouldMirrorAltiumModel3DStatus =
            modelProgress == m_progress.exportTypeProgress.cend() || !modelProgress->itemStatus.contains(componentId);
    }

    locker.unlock();
    emit itemStatusChanged(componentId, typeName, status);

    // Altium 的 STEP 在 PcbLib 封装阶段写入并嵌入库中，没有独立的 Model3D stage。
    // 将封装结果镜像到 Model3D，确保 UI 状态、成功率和总体完成判定一致。
    if (shouldMirrorAltiumModel3DStatus) {
        onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
    }

    // 更新整体进度
    updateOverallProgress();
}

// 处理一批预加载任务，网络服务缺失时直接读取磁盘缓存。
void ParallelExportService::processNextPreloadBatch() {
    if (m_cancelRequested || m_progress.currentStage != ExportOverallProgress::Stage::Preloading) {
        return;
    }

    constexpr int BATCH_SIZE = 8;

    int processedInBatch = 0;
    while (processedInBatch < BATCH_SIZE && m_nextPreloadIndex < m_componentIds.size() && !m_cancelRequested) {
        const QString componentId = m_componentIds.at(m_nextPreloadIndex++);
        {
            QMutexLocker locker(&m_progressMutex);
            m_progress.preloadProgress.currentComponentId = componentId;
            m_progress.preloadProgress.inProgressCount = 1;
        }

        ComponentData data;
        if (m_componentService) {
            data = m_componentService->getComponentData(componentId);
            const QSharedPointer<ComponentData> diskCachedData =
                ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
            ExportWorkerHelpers::mergeComponentData(data, diskCachedData);
        } else {
            const QSharedPointer<ComponentData> diskCachedData =
                ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
            if (diskCachedData)
                data = *diskCachedData;
        }

        {
            QMutexLocker locker(&m_progressMutex);
            // 严格校验：必须有 lcscId + symbolData + footprintData + isValid 才算有效
            bool hasValidData = !data.lcscId().isEmpty() && data.isValid() && data.symbolData() != nullptr &&
                                data.footprintData() != nullptr;
            if (hasValidData) {
                auto sharedData = QSharedPointer<ComponentData>::create(data);
                m_cachedData[componentId] = sharedData;
                m_progress.preloadProgress.successCount++;
                qDebug() << "ParallelExportService: Loaded data for" << componentId;
            } else {
                m_progress.preloadProgress.failedCount++;
                m_progress.preloadProgress.failedComponents[componentId] = QStringLiteral("Incomplete component data");
                qWarning() << "ParallelExportService: No valid data found for component:" << componentId;
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

// 接收网络批量获取结果并完成预加载统计。
void ParallelExportService::onAllComponentDataCollected(const QList<ComponentData>& componentDataList) {
    onAllComponentDataCollectedWithErrors(componentDataList, {});
}

// 接收网络失败原因并完成预加载统计。
void ParallelExportService::onAllComponentDataCollectedWithErrors(const QList<ComponentData>& componentDataList,
                                                                  const QMap<QString, QString>& failedComponents) {
    // 断开连接，避免重复处理
    if (m_componentService) {
        disconnect(m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, this, nullptr);
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
    int failedCount = failedComponents.size();

    for (auto it = failedComponents.cbegin(); it != failedComponents.cend(); ++it) {
        m_progress.preloadProgress.failedComponents[it.key()] = it.value();
    }

    // 处理每个组件数据
    for (const ComponentData& data : componentDataList) {
        const QString componentId = data.lcscId();

        if (componentId.isEmpty()) {
            failedCount++;
            continue;
        }

        // 严格校验：必须同时有 symbol 和 footprint 数据且通过验证
        bool hasValidData = data.isValid() && data.symbolData() != nullptr && data.footprintData() != nullptr;

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
        m_progress.preloadProgress.completedCount = successCount + failedCount;
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

// 发布当前导出总进度快照。
void ParallelExportService::updateOverallProgress() {
    ExportOverallProgress progressSnapshot;
    {
        QMutexLocker locker(&m_progressMutex);
        progressSnapshot = m_progress;
    }

    emit progressChanged(progressSnapshot);
}

// 检查所有导出阶段和元器件是否已经完成。
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

// 记录指定阶段的网络运行诊断。
void ParallelExportService::logNetworkRuntimeStats(const QString& context) const {
    ExportReportGenerator::logNetworkStats(context);
}

// 生成包含状态、诊断和网络信息的详细导出报告。
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
