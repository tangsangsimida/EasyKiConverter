#ifndef EASYKICONVERTER_PIPELINESTATISTICS_H
#define EASYKICONVERTER_PIPELINESTATISTICS_H

#include "../../ExportService.h"
#include "../../models/ComponentExportStatus.h"

#include <QList>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 导出流水线统计生成器
 *
 * 负责从已完成的任务状态中提取数据，生成统计报告和 JSON 文件。
 */
class PipelineStatistics {
public:
    static ExportStatistics generate(const QList<QSharedPointer<ComponentExportStatus>>& completedStatuses,
                                     const ExportOptions& options,
                                     int totalTasks,
                                     qint64 startTimeMs,
                                     bool isRetryMode,
                                     const ExportStatistics& originalStats);

    static bool saveReport(const ExportStatistics& statistics,
                           const QString& outputPath,
                           const QString& libName,
                           const QString& reportPath);
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_PIPELINESTATISTICS_H
