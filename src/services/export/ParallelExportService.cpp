#include "ParallelExportService.h"

#include "ComponentService.h"
#include "DatasheetExportStage.h"
#include "FootprintExportStage.h"
#include "Model3DExportStage.h"
#include "PreviewImagesExportStage.h"
#include "SymbolExportStage.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "services/ComponentCacheService.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMutexLocker>

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
    // cancel()设置原子标志后立即返回，不会等待worker完成（waitForDone会导致崩溃）
    for (auto* stage : m_exportStages) {
        stage->cancel();
        delete stage;  // 释放内存
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

    // 从ComponentService获取已验证的元器件数据
    if (m_componentService) {
        for (const QString& componentId : componentIds) {
            ComponentData data = m_componentService->getComponentData(componentId);
            const QSharedPointer<ComponentData> diskCachedData = loadDiskCachedComponentData(componentId);
            mergeComponentData(data, diskCachedData);

            if (!data.lcscId().isEmpty()) {
                auto sharedData = QSharedPointer<ComponentData>::create(data);
                m_cachedData[componentId] = sharedData;
                m_progress.preloadProgress.successCount++;
                qDebug() << "ParallelExportService: Loaded data for" << componentId;
            } else {
                m_progress.preloadProgress.failedCount++;
                qWarning() << "ParallelExportService: No data found for component:" << componentId;
            }
            m_progress.preloadProgress.completedCount++;
        }
    } else {
        qWarning() << "ParallelExportService: ComponentService not set, cannot load data";
        // 模拟完成但所有都失败
        for (const QString& componentId : componentIds) {
            m_progress.preloadProgress.completedCount++;
            m_progress.preloadProgress.failedCount++;
        }
    }

    qDebug() << "ParallelExportService: Preload completed. Success:" << m_progress.preloadProgress.successCount
             << "Failed:" << m_progress.preloadProgress.failedCount;

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

    m_runningExportStages = (enableSymbol ? 1 : 0) + (enableFootprint ? 1 : 0) + (enableModel3D ? 1 : 0) +
                            (enablePreview ? 1 : 0) + (enableDatasheet ? 1 : 0);

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
        stage->start(m_componentIds, m_cachedData);
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
        stage->start(m_componentIds, m_cachedData);
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
        stage->start(m_componentIds, m_cachedData);
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
        stage->start(m_componentIds, m_cachedData);
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
    m_cancelRequested = true;

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

    emit completed(totalSuccess, totalFailed);
    cleanupExportStages();
}

}  // namespace EasyKiConverter
