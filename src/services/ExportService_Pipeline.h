#ifndef EXPORTSERVICE_PIPELINE_H
#define EXPORTSERVICE_PIPELINE_H

#include "ExportService.h"
#include "models/ComponentExportStatus.h"
#include "models/SymbolData.h"
#include "utils/BoundedThreadSafeQueue.h"
#include "workers/FetchWorker.h"

#include <QAtomicInt>
#include <QMap>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QSet>
#include <QThreadPool>

namespace EasyKiConverter {

/**
 * @brief 流水线阶段枚�?
 */
enum class PipelineStage {
    Fetch,    // 抓取阶段
    Process,  // 处理阶段
    Write     // 写入阶段
};

/**
 * @brief 流水线阶段进�?
 */
struct PipelineProgress {
    int fetchCompleted = 0;    // 抓取完成�?
    int processCompleted = 0;  // 处理完成�?
    int writeCompleted = 0;    // 写入完成�?
    int totalTasks = 0;        // 总任务数

    // 计算各阶段进度（0-100�?
    int fetchProgress() const {
        return totalTasks > 0 ? (fetchCompleted * 100 / totalTasks) : 0;
    }
    int processProgress() const {
        return totalTasks > 0 ? (processCompleted * 100 / totalTasks) : 0;
    }
    int writeProgress() const {
        return totalTasks > 0 ? (writeCompleted * 100 / totalTasks) : 0;
    }

    // 计算加权总进度（抓取30%，处�?0%，写�?0%�?
    int overallProgress() const {
        return (fetchProgress() * 30 + processProgress() * 50 + writeProgress() * 20) / 100;
    }
};

/**
 * @brief 流水线导出服务类
 *
 * 扩展ExportService，支持多阶段流水线并行架�?
 */
class ExportServicePipeline : public ExportService {
    Q_OBJECT

public:
    /**
     * @brief 构造函�?
     * @param parent 父对�?
     */
    explicit ExportServicePipeline(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ExportServicePipeline() override;

    /**
     * @brief 使用流水线架构执行批量导�?
     * @param componentIds 元件ID列表
     * @param options 导出选项
     */
    void executeExportPipelineWithStages(const QStringList& componentIds, const ExportOptions& options);
    void retryExport(const QStringList& componentIds, const ExportOptions& options) override;

    /**
     * @brief 获取流水线进�?
     * @return PipelineProgress 流水线进�?
     */
    PipelineProgress getPipelineProgress() const;

    /**
     * @brief 取消导出
     */
    void cancelExport() override;

signals:
    /**
     * @brief 流水线进度更新信�?
     * @param progress 流水线进�?
     */
    void pipelineProgressUpdated(const PipelineProgress& progress);

    /**
     * @brief 统计报告生成信号
     * @param reportPath 报告文件路径
     * @param statistics 统计数据
     */
    void statisticsReportGenerated(const QString& reportPath, const ExportStatistics& statistics);

private slots:
    /**
     * @brief 处理抓取完成
     * @param status 导出状态（使用 QSharedPointer 避免拷贝�?
     */
    void handleFetchCompleted(QSharedPointer<ComponentExportStatus> status);

    /**
     * @brief 处理处理完成
     * @param status 导出状态（使用 QSharedPointer 避免拷贝�?
     */
    void handleProcessCompleted(QSharedPointer<ComponentExportStatus> status);

    /**
     * @brief 处理写入完成
     * @param status 导出状态（使用 QSharedPointer 避免拷贝�?
     */
    void handleWriteCompleted(QSharedPointer<ComponentExportStatus> status);

private:
    /**
     * @brief 启动抓取阶段
     */
    void startFetchStage();

    /**
     * @brief 启动处理阶段
     */
    void startProcessStage();

    /**
     * @brief 启动写入阶段
     */
    void startWriteStage();

    /**
     * @brief 检查流水线是否完成
     */
    void checkPipelineCompletion();

    /**
     * @brief 清理流水线资�?
     */
    void cleanupPipeline();

    /**
     * @brief 合并符号�?
     * @return bool 是否成功
     */
    bool mergeSymbolLibrary();

private:
    // 线程�?
    QThreadPool* m_fetchThreadPool;    // 抓取线程池（I/O密集型，32个线程）
    QThreadPool* m_processThreadPool;  // 处理线程池（CPU密集型，等于核心数）
    QThreadPool* m_writeThreadPool;    // 写入线程池（磁盘I/O密集型，8个线程）

    // 线程安全队列（使�?QSharedPointer 避免数据拷贝�?
    BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>* m_fetchProcessQueue;  // 抓取->处理队列
    BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>* m_processWriteQueue;  // 处理->写入队列

    // 网络访问管理器（共享�?
    QNetworkAccessManager* m_networkAccessManager;

    // 流水线状�?
    PipelineProgress m_pipelineProgress;
    QStringList m_componentIds;
    ExportOptions m_options;
    bool m_isPipelineRunning;
    QAtomicInt m_isCancelled;

    // 互斥�?
    QMutex* m_mutex;
    QMutex m_workerMutex;  // 保护活跃工作线程列表

    // 活跃的抓取工作线程列表（用于中断）
    QSet<FetchWorker*> m_activeFetchWorkers;

    // 临时文件列表（用于合并符号库�?
    QStringList m_tempSymbolFiles;

    // 符号数据列表（用于合并符号库�?
    QList<SymbolData> m_symbols;

    // 结果统计
    int m_successCount;
    int m_failureCount;

    // 完整的状态列表（用于生成统计报告�?
    QVector<QSharedPointer<ComponentExportStatus>> m_completedStatuses;

    // 导出开始时间（用于计算总耗时�?
    qint64 m_exportStartTimeMs;

    /**
     * @brief 生成统计报告
     * @return ExportStatistics 统计数据
     */
    ExportStatistics generateStatistics();

    /**
     * @brief 保存统计报告到文�?
     * @param statistics 统计数据
     * @param reportPath 报告文件路径
     * @return bool 是否成功
     */
    bool saveStatisticsReport(const ExportStatistics& statistics, const QString& reportPath);
};

}  // namespace EasyKiConverter

#endif  // EXPORTSERVICE_PIPELINE_H
