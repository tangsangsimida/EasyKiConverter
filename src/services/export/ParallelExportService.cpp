#include "ParallelExportService.h"

#include "DatasheetExportStage.h"
#include "FootprintExportStage.h"
#include "Model3DExportStage.h"
#include "PreviewImagesExportStage.h"
#include "SymbolExportStage.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

ParallelExportService::ParallelExportService(QObject* parent) : QObject(parent), m_progress() {
    m_progress.currentStage = ExportOverallProgress::Stage::Idle;
}

ParallelExportService::~ParallelExportService() {
    cancelExport();
}

void ParallelExportService::setOptions(const ExportOptions& options) {
    m_options = options;
}

void ParallelExportService::setOutputPath(const QString& path) {
    m_options.outputPath = path;
}

void ParallelExportService::startPreload(const QStringList& componentIds) {
    if (m_progress.currentStage == ExportOverallProgress::Stage::Preloading) {
        qWarning() << "ParallelExportService: Preload already in progress";
        return;
    }

    qDebug() << "ParallelExportService: Starting preload for" << componentIds.size() << "components";

    m_componentIds = componentIds;
    m_progress.currentStage = ExportOverallProgress::Stage::Preloading;
    m_progress.totalComponents = componentIds.size();
    m_progress.startTime = QDateTime::currentDateTime();
    m_progress.preloadProgress = PreloadProgress();
    m_progress.preloadProgress.totalCount = componentIds.size();

    // TODO: 实现预加载逻辑
    // 1. 使用网络客户端获取元器件信息
    // 2. 缓存符号JSON、封装JSON、3D模型UUID、预览图URL、数据手册URL
    // 3. 对于大文件（3D模型、数据手册），只缓存URL/UUID，实际下载在导出时进行

    // 临时：模拟预加载完成
    for (const QString& componentId : componentIds) {
        m_progress.preloadProgress.completedCount++;
        m_progress.preloadProgress.successCount++;
    }

    m_preloadCompleted = true;
    m_progress.currentStage = ExportOverallProgress::Stage::Idle;
    emit preloadCompleted(m_progress.preloadProgress.successCount, m_progress.preloadProgress.failedCount);
}

void ParallelExportService::cancelPreload() {
    qDebug() << "ParallelExportService: Cancelling preload";
    m_progress.preloadProgress.inProgressCount = 0;
    m_progress.currentStage = ExportOverallProgress::Stage::Idle;
    emit cancelled();
}

void ParallelExportService::startExport() {
    if (m_progress.currentStage == ExportOverallProgress::Stage::Exporting) {
        qWarning() << "ParallelExportService: Export already in progress";
        return;
    }

    if (m_componentIds.isEmpty()) {
        qWarning() << "ParallelExportService: No components to export";
        emit failed(QStringLiteral("No components to export"));
        return;
    }

    qDebug() << "ParallelExportService: Starting export for" << m_componentIds.size() << "components";

    m_progress.currentStage = ExportOverallProgress::Stage::Exporting;
    m_progress.startTime = QDateTime::currentDateTime();
    m_runningExportStages = 0;

    // 创建并启动各导出类型的Stage
    if (m_options.exportSymbol) {
        auto* stage = new SymbolExportStage(this);
        m_exportStages[QStringLiteral("Symbol")] = stage;

        connect(stage,
                &SymbolExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Symbol"), status);
                });
        connect(stage, &SymbolExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Symbol"), success, failed, skipped);
        });

        m_runningExportStages++;
        stage->start(m_componentIds, m_cachedData);
    }

    if (m_options.exportFootprint) {
        auto* stage = new FootprintExportStage(this);
        m_exportStages[QStringLiteral("Footprint")] = stage;

        connect(stage,
                &FootprintExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Footprint"), status);
                });
        connect(stage, &FootprintExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Footprint"), success, failed, skipped);
        });

        m_runningExportStages++;
        stage->start(m_componentIds, m_cachedData);
    }

    if (m_options.exportModel3D) {
        auto* stage = new Model3DExportStage(this);
        m_exportStages[QStringLiteral("Model3D")] = stage;

        connect(stage,
                &Model3DExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Model3D"), status);
                });
        connect(stage, &Model3DExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Model3D"), success, failed, skipped);
        });

        m_runningExportStages++;
        stage->start(m_componentIds, m_cachedData);
    }

    if (m_options.exportPreviewImages) {
        auto* stage = new PreviewImagesExportStage(this);
        m_exportStages[QStringLiteral("PreviewImages")] = stage;

        connect(stage,
                &PreviewImagesExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("PreviewImages"), status);
                });
        connect(stage, &PreviewImagesExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("PreviewImages"), success, failed, skipped);
        });

        m_runningExportStages++;
        stage->start(m_componentIds, m_cachedData);
    }

    if (m_options.exportDatasheet) {
        auto* stage = new DatasheetExportStage(this);
        m_exportStages[QStringLiteral("Datasheet")] = stage;

        connect(stage,
                &DatasheetExportStage::itemStatusChanged,
                this,
                [this](const QString& componentId, const ExportItemStatus& status) {
                    onExportItemStatusChanged(componentId, QStringLiteral("Datasheet"), status);
                });
        connect(stage, &DatasheetExportStage::completed, this, [this](int success, int failed, int skipped) {
            onExportTypeCompleted(QStringLiteral("Datasheet"), success, failed, skipped);
        });

        m_runningExportStages++;
        stage->start(m_componentIds, m_cachedData);
    }

    if (m_runningExportStages == 0) {
        qWarning() << "ParallelExportService: No export types enabled";
        m_progress.currentStage = ExportOverallProgress::Stage::Completed;
        m_progress.endTime = QDateTime::currentDateTime();
        emit completed(0, 0);
    }
}

void ParallelExportService::cancelExport() {
    qDebug() << "ParallelExportService: Cancelling export";

    for (auto* stage : m_exportStages) {
        stage->cancel();
    }

    m_progress.currentStage = ExportOverallProgress::Stage::Cancelled;
    m_progress.endTime = QDateTime::currentDateTime();
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

void ParallelExportService::onExportTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    QMutexLocker locker(&m_progressMutex);

    ExportTypeProgress typeProgress;
    typeProgress.typeName = typeName;
    typeProgress.successCount = successCount;
    typeProgress.failedCount = failedCount;
    typeProgress.skippedCount = skippedCount;
    typeProgress.completedCount = successCount + failedCount + skippedCount;

    m_progress.exportTypeProgress[typeName] = typeProgress;

    m_runningExportStages--;

    locker.unlock();
    emit typeCompleted(typeName, successCount, failedCount, skippedCount);

    checkAllExportCompleted();
}

void ParallelExportService::onExportItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    QMutexLocker locker(&m_progressMutex);

    if (!m_progress.exportTypeProgress.contains(typeName)) {
        m_progress.exportTypeProgress[typeName] = ExportTypeProgress();
        m_progress.exportTypeProgress[typeName].typeName = typeName;
    }

    m_progress.exportTypeProgress[typeName].itemStatus[componentId] = status;

    locker.unlock();
    emit itemStatusChanged(componentId, typeName, status);

    // 更新整体进度
    updateOverallProgress();
}

void ParallelExportService::updateOverallProgress() {
    QMutexLocker locker(&m_progressMutex);

    int total = 0, completed = 0;
    for (const auto& p : m_progress.exportTypeProgress) {
        total += p.totalCount;
        completed += p.completedCount;
    }

    // 发出整体进度更新信号
    emit progressChanged(m_progress);
}

void ParallelExportService::checkAllExportCompleted() {
    if (m_runningExportStages > 0) {
        return;
    }

    qDebug() << "ParallelExportService: All export stages completed";

    m_progress.currentStage = ExportOverallProgress::Stage::Completed;
    m_progress.endTime = QDateTime::currentDateTime();

    int totalSuccess = 0;
    int totalFailed = 0;
    for (const auto& p : m_progress.exportTypeProgress) {
        totalSuccess += p.successCount;
        totalFailed += p.failedCount;
    }

    emit completed(totalSuccess, totalFailed);
}

}  // namespace EasyKiConverter
