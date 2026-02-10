#include "ExportProgressViewModel.h"

#include "services/ExportService_Pipeline.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWindow>

namespace EasyKiConverter {

ExportProgressViewModel::ExportProgressViewModel(ExportService* exportService,
                                                 ComponentService* componentService,
                                                 ComponentListViewModel* componentListViewModel,
                                                 QObject* parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_componentService(componentService)
    , m_componentListViewModel(componentListViewModel)
    , m_status("Ready")
    , m_progress(0)
    , m_isExporting(false)
    , m_isStopping(false)
    , m_successCount(0)
    , m_successSymbolCount(0)
    , m_successFootprintCount(0)
    , m_successModel3DCount(0)
    , m_failureCount(0)
    , m_fetchedCount(0)
    , m_fetchProgress(0)
    , m_processProgress(0)
    , m_writeProgress(0)
    , m_usePipelineMode(false)
    , m_pendingUpdate(false)
    , m_hasStatistics(false)
    , m_systemTrayIcon(nullptr) {
    // 初始化节流定时器
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setInterval(100);
    m_throttleTimer->setSingleShot(true);
    connect(m_throttleTimer, &QTimer::timeout, this, &ExportProgressViewModel::flushPendingUpdates);

    // 连接 ExportService 信号
    if (m_exportService) {
        connect(m_exportService, &ExportService::exportProgress, this, &ExportProgressViewModel::handleExportProgress);
        connect(m_exportService,
                &ExportService::componentExported,
                this,
                &ExportProgressViewModel::handleComponentExported);
        connect(
            m_exportService, &ExportService::exportCompleted, this, &ExportProgressViewModel::handleExportCompleted);
        connect(m_exportService, &ExportService::exportFailed, this, &ExportProgressViewModel::handleExportFailed);

        if (auto* pipelineService = qobject_cast<ExportServicePipeline*>(m_exportService)) {
            connect(pipelineService,
                    &ExportServicePipeline::pipelineProgressUpdated,
                    this,
                    &ExportProgressViewModel::handlePipelineProgressUpdated);
            connect(pipelineService,
                    &ExportServicePipeline::statisticsReportGenerated,
                    this,
                    &ExportProgressViewModel::handleStatisticsReportGenerated);
            m_usePipelineMode = true;
        }
    }

    if (m_componentService) {
        connect(m_componentService,
                &ComponentService::cadDataReady,
                this,
                &ExportProgressViewModel::handleComponentDataFetched);
        connect(m_componentService,
                &ComponentService::allComponentsDataCollected,
                this,
                &ExportProgressViewModel::handleAllComponentsDataCollected);
    }

    // 初始化系统托盘图标
    initializeSystemTrayIcon();
}

void ExportProgressViewModel::setUsePipelineMode(bool usePipeline) {
    m_usePipelineMode = usePipeline;
}

ExportProgressViewModel::~ExportProgressViewModel() {
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }
    if (m_systemTrayIcon) {
        m_systemTrayIcon->hide();
        m_systemTrayIcon->deleteLater();
    }
}

void ExportProgressViewModel::startExport(const QStringList& componentIds,
                                          const QString& outputPath,
                                          const QString& libName,
                                          bool exportSymbol,
                                          bool exportFootprint,
                                          bool exportModel3D,
                                          bool overwriteExistingFiles,
                                          bool updateMode,
                                          bool debugMode) {
    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }

    // 保存导出选项
    m_exportOptions.outputPath = outputPath;
    m_exportOptions.libName = libName;
    m_exportOptions.exportSymbol = exportSymbol;
    m_exportOptions.exportFootprint = exportFootprint;
    m_exportOptions.exportModel3D = exportModel3D;
    m_exportOptions.overwriteExistingFiles = overwriteExistingFiles;
    m_exportOptions.updateMode = updateMode;
    m_exportOptions.debugMode = debugMode;

    startExportInternal(componentIds, false);
}

void ExportProgressViewModel::cancelExport() {
    if (!m_exportService)
        return;

    qDebug() << "ExportProgressViewModel::cancelExport() called";

    // 1. 瞬时响应：立即通知后端中断（原子操作，<1ms）
    m_exportService->cancelExport();

    // 2. 立即更新按钮状态（同步执行，<1ms）
    m_isStopping = true;
    emit isStoppingChanged();
    m_status = "Stopping export...";
    emit statusChanged();

    qDebug() << "Cancel operation initiated in <1ms";

    // 3. 异步处理 UI 列表更新（不阻塞取消操作）
    QTimer::singleShot(0, this, [this]() {
        bool updated = false;
        for (int i = 0; i < m_resultsList.size(); ++i) {
            QVariantMap item = m_resultsList[i].toMap();
            QString status = item["status"].toString();

            // 更新所有未完成的状态 (v3.0.5+ 改进)
            if (status == "pending" || status == "fetching" || status == "processing" || status == "writing") {
                item["status"] = "failed";
                item["message"] = "Export cancelled";
                // CRITICAL FIX: 在取消时，将所有分项成功标志也重置为 false
                item["symbolSuccess"] = false;
                item["footprintSuccess"] = false;
                item["model3DSuccess"] = false;
                m_resultsList[i] = item;
                updated = true;
            }
        }

        if (updated) {
            emit resultsListChanged();
            updateStatistics();
        }
        qDebug() << "UI update completed after cancellation";
    });
}

void ExportProgressViewModel::resetExport() {
    qDebug() << "ExportProgressViewModel::resetExport() called";

    // 重置所有导出状态
    m_progress = 0;
    m_status = "";
    m_isExporting = false;
    m_isStopping = false;
    m_successCount = 0;
    m_successSymbolCount = 0;
    m_successFootprintCount = 0;
    m_successModel3DCount = 0;
    m_failureCount = 0;
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;

    // 清空结果列表
    m_resultsList.clear();
    m_idToIndexMap.clear();

    // 清空统计信息
    m_hasStatistics = false;
    m_statisticsReportPath = "";
    m_statisticsSummary = "";
    m_statistics = ExportStatistics();

    // 触发所有相关的信号更新
    emit progressChanged();
    emit statusChanged();
    emit isExportingChanged();
    emit isStoppingChanged();
    emit successCountChanged();
    emit failureCountChanged();
    emit resultsListChanged();
    emit statisticsChanged();
    emit fetchProgressChanged();
    emit processProgressChanged();
    emit writeProgressChanged();

    qDebug() << "Export status reset completed";
}

void ExportProgressViewModel::handleExportProgress(int current, int total) {
    int newProgress = total > 0 ? (current * 100 / total) : 0;
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

QString ExportProgressViewModel::getStatusString(int stage,
                                                 bool success,
                                                 bool symbolSuccess,
                                                 bool footprintSuccess,
                                                 bool model3DSuccess) const {
    if (!success) {
        return "failed";
    }

    switch (stage) {
        case 0:
            return "fetch_completed";
        case 1:
            return "processing";
        case 2: {
            // 严格成功判定：检查用户请求的所有项是否都已成功写入
            bool allRequestedItemsDone = true;
            if (m_exportOptions.exportSymbol && !symbolSuccess)
                allRequestedItemsDone = false;
            if (m_exportOptions.exportFootprint && !footprintSuccess)
                allRequestedItemsDone = false;
            if (m_exportOptions.exportModel3D && !model3DSuccess)
                allRequestedItemsDone = false;

            return allRequestedItemsDone ? "success" : "failed";
        }
        default:
            return "success";
    }
}

void ExportProgressViewModel::handleComponentExported(const QString& componentId,
                                                      bool success,
                                                      const QString& message,
                                                      int stage,
                                                      bool symbolSuccess,
                                                      bool footprintSuccess,
                                                      bool model3DSuccess) {
    int index = m_idToIndexMap.value(componentId, -1);
    // 使用增强后的判定逻辑
    QString statusStr = getStatusString(stage, success, symbolSuccess, footprintSuccess, model3DSuccess);

    if (index >= 0 && index < m_resultsList.size()) {
        QVariantMap result = m_resultsList[index].toMap();
        result["status"] = statusStr;
        result["message"] = statusStr == "failed" && success ? "Partial export failed (missing parts)" : message;
        // 保存分项状态
        result["symbolSuccess"] = symbolSuccess;
        result["footprintSuccess"] = footprintSuccess;
        result["model3DSuccess"] = model3DSuccess;
        m_resultsList[index] = result;
    } else {
        QVariantMap result;
        result["componentId"] = componentId;
        result["status"] = statusStr;
        result["message"] = message;
        result["symbolSuccess"] = symbolSuccess;
        result["footprintSuccess"] = footprintSuccess;
        result["model3DSuccess"] = model3DSuccess;
        m_resultsList.append(result);
        m_idToIndexMap[componentId] = m_resultsList.size() - 1;
    }

    if (!m_pendingUpdate) {
        m_pendingUpdate = true;
        m_throttleTimer->start();
    }

    updateStatistics();
    emit componentExported(componentId, success, message);
}

void ExportProgressViewModel::updateStatistics() {
    int globalSuccess = 0;
    int globalFailed = 0;
    int globalSymbol = 0;
    int globalFootprint = 0;
    int globalModel3D = 0;

    for (const auto& var : m_resultsList) {
        QVariantMap item = var.toMap();
        QString status = item["status"].toString();

        if (status == "success") {
            globalSuccess++;
        } else if (status == "failed") {
            globalFailed++;
        }

        // 累加分项成功数（只有当该类型被启用时才计数）
        if (m_exportOptions.exportSymbol && item["symbolSuccess"].toBool())
            globalSymbol++;
        if (m_exportOptions.exportFootprint && item["footprintSuccess"].toBool())
            globalFootprint++;
        if (m_exportOptions.exportModel3D && item["model3DSuccess"].toBool())
            globalModel3D++;
    }

    if (m_successCount != globalSuccess) {
        m_successCount = globalSuccess;
        emit successCountChanged();
    }

    if (m_failureCount != globalFailed) {
        m_failureCount = globalFailed;
        emit failureCountChanged();
    }

    // 更新分项计数
    if (m_successSymbolCount != globalSymbol || m_successFootprintCount != globalFootprint ||
        m_successModel3DCount != globalModel3D) {
        m_successSymbolCount = globalSymbol;
        m_successFootprintCount = globalFootprint;
        m_successModel3DCount = globalModel3D;
        emit successCountChanged();  // 触发 UI 刷新
    }
}

void ExportProgressViewModel::flushPendingUpdates() {
    if (m_pendingUpdate) {
        m_pendingUpdate = false;
        emit resultsListChanged();
    }
}

void ExportProgressViewModel::prepopulateResultsList(const QStringList& componentIds) {
    m_resultsList.clear();
    m_idToIndexMap.clear();

    for (int i = 0; i < componentIds.size(); ++i) {
        QVariantMap result;
        result["componentId"] = componentIds[i];
        result["status"] = "pending";
        result["message"] = "Waiting to start...";
        m_resultsList.append(result);
        m_idToIndexMap[componentIds[i]] = i;
    }
}

void ExportProgressViewModel::handleComponentDataFetched(const QString& componentId, const ComponentData& data) {
    Q_UNUSED(componentId);
    Q_UNUSED(data);
}

void ExportProgressViewModel::handleAllComponentsDataCollected(const QList<ComponentData>& componentDataList) {
    m_status = "Exporting components in parallel...";
    emit statusChanged();
    m_exportService->executeExportPipelineWithDataParallel(componentDataList, m_exportOptions);
}

void ExportProgressViewModel::handleExportCompleted(int totalCount, int successCount) {
    Q_UNUSED(totalCount);
    Q_UNUSED(successCount);

    // 扫尾：确保 pending 项标记为取消
    for (int i = 0; i < m_resultsList.size(); ++i) {
        QVariantMap item = m_resultsList[i].toMap();
        if (item["status"].toString() == "pending") {
            item["status"] = "failed";
            item["message"] = "Export cancelled";
            m_resultsList[i] = item;
        }
    }

    // 全量刷新统计
    updateStatistics();

    if (m_pendingUpdate) {
        m_throttleTimer->stop();
        flushPendingUpdates();
    }

    m_isStopping = false;
    emit isStoppingChanged();
    m_isExporting = false;
    emit isExportingChanged();
    m_status = "Export completed";
    emit statusChanged();
    emit exportCompleted(m_successCount, m_failureCount);

    // 显示系统通知
    showExportCompleteNotification();
}

void ExportProgressViewModel::handleExportFailed(const QString& error) {
    if (m_pendingUpdate) {
        m_throttleTimer->stop();
        flushPendingUpdates();
    }

    m_isStopping = false;
    m_isExporting = false;
    emit isStoppingChanged();
    emit isExportingChanged();
    m_status = "Export failed: " + error;
    emit statusChanged();
    updateStatistics();
}

void ExportProgressViewModel::handlePipelineProgressUpdated(const PipelineProgress& progress) {
    if (m_fetchProgress != progress.fetchProgress()) {
        m_fetchProgress = progress.fetchProgress();
        emit fetchProgressChanged();
    }
    if (m_processProgress != progress.processProgress()) {
        m_processProgress = progress.processProgress();
        emit processProgressChanged();
    }
    if (m_writeProgress != progress.writeProgress()) {
        m_writeProgress = progress.writeProgress();
        emit writeProgressChanged();
    }

    int newProgress = progress.overallProgress();
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

void ExportProgressViewModel::handleStatisticsReportGenerated(const QString& reportPath,
                                                              const ExportStatistics& statistics) {
    m_hasStatistics = true;
    m_statisticsReportPath = reportPath;
    m_statistics = statistics;
    m_statisticsSummary = statistics.getSummary();
    emit statisticsChanged();
}

void ExportProgressViewModel::retryFailedComponents() {
    if (m_isExporting)
        return;

    QStringList failedIds;
    for (int i = 0; i < m_resultsList.size(); ++i) {
        if (m_resultsList[i].toMap()["status"].toString() != "success") {
            failedIds << m_resultsList[i].toMap()["componentId"].toString();
        }
    }

    if (!failedIds.isEmpty()) {
        startExportInternal(failedIds, true);
    }
}

void ExportProgressViewModel::retryComponent(const QString& componentId) {
    if (m_isExporting)
        return;
    startExportInternal(QStringList() << componentId, true);
}

void ExportProgressViewModel::startExportInternal(const QStringList& componentIds, bool isRetry) {
    if (!m_exportService || !m_componentService)
        return;

    m_isExporting = true;
    m_progress = 0;
    m_fetchedCount = 0;
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    m_componentIds = componentIds;
    m_collectedData.clear();
    m_status = isRetry ? "Retrying..." : "Starting export...";

    if (!isRetry) {
        m_successCount = 0;
        m_failureCount = 0;
        prepopulateResultsList(componentIds);
    } else {
        // 重试模式：仅更新被重试项的状态
        for (const QString& id : componentIds) {
            int index = m_idToIndexMap.value(id, -1);
            if (index >= 0) {
                QVariantMap item = m_resultsList[index].toMap();
                item["status"] = "pending";
                item["message"] = "Waiting to retry...";
                m_resultsList[index] = item;
            }
        }
    }

    // 无论是新开始还是重试，都先执行一次全量统计同步
    updateStatistics();
    emit resultsListChanged();
    emit isExportingChanged();
    emit progressChanged();
    emit statusChanged();

    if (isRetry) {
        m_exportService->retryExport(componentIds, m_exportOptions);
    } else {
        if (m_usePipelineMode && qobject_cast<ExportServicePipeline*>(m_exportService)) {
            auto* pipelineService = qobject_cast<ExportServicePipeline*>(m_exportService);

            // Gather preloaded data
            if (m_componentListViewModel) {
                QMap<QString, QSharedPointer<ComponentData>> preloadedData;
                for (const QString& id : componentIds) {
                    auto data = m_componentListViewModel->getPreloadedData(id);
                    if (data) {
                        preloadedData.insert(id, data);
                    }
                }
                if (!preloadedData.isEmpty()) {
                    qDebug() << "Passing" << preloadedData.size() << "preloaded components to pipeline";
                    pipelineService->setPreloadedData(preloadedData);
                }
            }

            pipelineService->executeExportPipelineWithStages(componentIds, m_exportOptions);
        } else {
            m_componentService->setOutputPath(m_exportOptions.outputPath);
            m_componentService->fetchMultipleComponentsData(componentIds, m_exportOptions.exportModel3D);
        }
    }
}

void ExportProgressViewModel::showExportCompleteNotification() {
    if (!m_systemTrayIcon) {
        qDebug() << "System tray icon not initialized";
        return;
    }

    // 检查是否支持消息
    if (!QSystemTrayIcon::supportsMessages()) {
        qDebug() << "System tray does not support messages on this platform";
        return;
    }

    // 准备通知内容
    QString title;
    QString message;
    QSystemTrayIcon::MessageIcon iconType = QSystemTrayIcon::Information;

    if (m_failureCount > 0) {
        // 有失败的情况
        title = QObject::tr("导出完成");

        // 计算成功率
        double successRate = m_resultsList.size() > 0 ? (m_successCount * 100.0 / m_resultsList.size()) : 0.0;

        if (m_successCount == 0) {
            // 全部失败
            iconType = QSystemTrayIcon::Critical;
            message = QObject::tr("导出失败：%1 个元器件全部失败").arg(m_resultsList.size());
        } else if (successRate < 50.0) {
            // 失败较多
            iconType = QSystemTrayIcon::Warning;
            message = QObject::tr("成功 %1 个，失败 %2 个").arg(m_successCount).arg(m_failureCount);
        } else {
            // 部分失败
            message = QObject::tr("成功 %1 个，失败 %2 个").arg(m_successCount).arg(m_failureCount);
        }

        // 添加详细的输出统计
        if (m_successCount > 0) {
            message += QObject::tr("\n输出：符号 %1 · 封装 %2 · 3D %3")
                           .arg(m_successSymbolCount)
                           .arg(m_successFootprintCount)
                           .arg(m_successModel3DCount);
        }
    } else {
        // 全部成功
        iconType = QSystemTrayIcon::Information;
        title = QObject::tr("导出完成");

        // 根据成功数量显示不同的成功消息
        if (m_successCount == 1) {
            message = QObject::tr("成功导出 1 个元器件");
        } else {
            message = QObject::tr("成功导出 %1 个元器件").arg(m_successCount);
        }

        // 添加输出统计和耗时信息
        QStringList stats;
        stats << QObject::tr("输出：符号 %1 · 封装 %2 · 3D %3")
                     .arg(m_successSymbolCount)
                     .arg(m_successFootprintCount)
                     .arg(m_successModel3DCount);

        // 添加耗时信息
        if (m_statistics.totalDurationMs > 0) {
            double durationSec = m_statistics.totalDurationMs / 1000.0;
            QString timeStr;
            if (durationSec < 60.0) {
                timeStr = QObject::tr("%1 秒").arg(QString::number(durationSec, 'f', 1));
            } else {
                int minutes = static_cast<int>(durationSec / 60.0);
                double seconds = durationSec - (minutes * 60.0);
                timeStr = QObject::tr("%1 分 %2 秒").arg(minutes).arg(QString::number(seconds, 'f', 0));
            }
            stats << QObject::tr("耗时：%1").arg(timeStr);
        }

        message += "\n" + stats.join("\n");
    }

    qDebug() << "Attempting to show notification:" << title << message;

    // 显示通知（延长显示时间到 12 秒，让用户有足够时间阅读）
    m_systemTrayIcon->showMessage(title, message, iconType, 12000);

    qDebug() << "Notification shown successfully";
}

void ExportProgressViewModel::initializeSystemTrayIcon() {
    // 检查是否支持系统托盘
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "System tray is not available on this platform";
        return;
    }

    qDebug() << "Initializing system tray icon...";

    // 创建系统托盘图标
    m_systemTrayIcon = new QSystemTrayIcon(this);

    // 设置图标 - 尝试多个路径
    QIcon appIcon;

    // 尝试资源文件路径（使用 qt_add_qml_module 的路径格式）
    QStringList iconPaths = {":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.ico",
                             ":/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png",
                             ":/resources/icons/app_icon.ico",
                             ":/resources/icons/app_icon.png",
                             ":/icons/app_icon.ico",
                             ":/icons/app_icon.png"};

    for (const QString& path : iconPaths) {
        appIcon = QIcon(path);
        if (!appIcon.isNull()) {
            qDebug() << "Icon loaded successfully from:" << path;
            break;
        } else {
            qDebug() << "Failed to load icon from:" << path;
        }
    }

    // 如果所有路径都失败，使用 Qt 内置图标
    if (appIcon.isNull()) {
        qWarning() << "Failed to load icon from all paths, using default icon";
        // 使用 Qt 样式标准图标
        appIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        if (appIcon.isNull()) {
            qCritical() << "Failed to load even default icon";
            return;
        }
    }

    m_systemTrayIcon->setIcon(appIcon);
    qDebug() << "System tray icon set";

    // 设置工具提示
    m_systemTrayIcon->setToolTip(QObject::tr("EasyKiConverter - LCSC 转换工具"));

    // 创建托盘菜单
    QMenu* trayMenu = new QMenu();

    QAction* showAction = trayMenu->addAction(QObject::tr("显示窗口"));
    connect(showAction, &QAction::triggered, this, []() {
        qDebug() << "Show window action triggered";
        // 显示主窗口（如果最小化或隐藏）
        auto windows = QGuiApplication::topLevelWindows();
        for (auto* window : windows) {
            if (window) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    });

    QAction* quitAction = trayMenu->addAction(QObject::tr("退出"));
    connect(quitAction, &QAction::triggered, this, []() {
        qDebug() << "Quit action triggered";
        QGuiApplication::quit();
    });

    m_systemTrayIcon->setContextMenu(trayMenu);

    // 显示托盘图标
    m_systemTrayIcon->show();

    // 验证图标是否真的显示
    if (m_systemTrayIcon->isVisible()) {
        qDebug() << "System tray icon is visible";
    } else {
        qWarning() << "System tray icon is not visible after show()";
    }

    qDebug() << "System tray icon initialized and shown";
}
}  // namespace EasyKiConverter
