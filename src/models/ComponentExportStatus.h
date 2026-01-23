#ifndef COMPONENTEXPORTSTATUS_H
#define COMPONENTEXPORTSTATUS_H

#include <QString>
#include <QSharedPointer>
#include <QElapsedTimer>
#include "models/ComponentData.h"
#include "models/SymbolData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"

namespace EasyKiConverter
{

    /**
     * @brief 元件导出状�?
     *
     * 跟踪元件在流水线各阶段的状�?
     */
    struct ComponentExportStatus
    {
        QString componentId; // 元件ID

        // 原始数据（抓取阶段）
        QByteArray componentInfoRaw; // 组件信息原始数据
        QByteArray cinfoJsonRaw;     // cinfo JSON 原始数据
        QByteArray cadDataRaw;       // CAD数据原始数据
        QByteArray cadJsonRaw;       // cad JSON 原始数据
        QByteArray advJsonRaw;       // adv JSON 原始数据
        QByteArray model3DObjRaw;    // 3D模型OBJ原始数据
        QByteArray model3DStepRaw;   // 3D模型STEP原始数据

        // 解析后的数据（处理阶段）
        QSharedPointer<ComponentData> componentData;
        QSharedPointer<SymbolData> symbolData;
        QSharedPointer<FootprintData> footprintData;
        QSharedPointer<Model3DData> model3DData;

        // 抓取阶段状�?
        bool fetchSuccess = false;
        QString fetchMessage;
        qint64 fetchDurationMs = 0; // 抓取耗时（毫秒）

        // 处理阶段状�?
        bool processSuccess = false;
        QString processMessage;
        qint64 processDurationMs = 0; // 处理耗时（毫秒）

        // 写入阶段状�?
        bool writeSuccess = false;
        QString writeMessage;
        qint64 writeDurationMs = 0; // 写入耗时（毫秒）

        // 调试日志（仅在调试模式下使用�?
        QStringList debugLog;

        // 是否需要导�?D模型
        bool need3DModel = false;

        /**
         * @brief 检查是否完全成�?
         * @return bool
         */
        bool isCompleteSuccess() const
        {
            return fetchSuccess && processSuccess && writeSuccess;
        }

        /**
         * @brief 获取失败阶段
         * @return QString 失败阶段名称
         */
        QString getFailedStage() const
        {
            if (!fetchSuccess)
                return "Fetch";
            if (!processSuccess)
                return "Process";
            if (!writeSuccess)
                return "Write";
            return "";
        }

        /**
         * @brief 获取失败原因
         * @return QString 失败原因
         */
        QString getFailureReason() const
        {
            if (!fetchSuccess)
                return fetchMessage;
            if (!processSuccess)
                return processMessage;
            if (!writeSuccess)
                return writeMessage;
            return "";
        }

        /**
         * @brief 获取总耗时
         * @return qint64 总耗时（毫秒）
         */
        qint64 getTotalDurationMs() const
        {
            return fetchDurationMs + processDurationMs + writeDurationMs;
        }

        /**
         * @brief 添加调试日志
         * @param message 日志消息
         */
        void addDebugLog(const QString &message)
        {
            debugLog.append(message);
        }
    };

    /**
     * @brief 导出统计数据结构
     */
    struct ExportStatistics
    {
        int total = 0;           // 总数
        int success = 0;         // 成功�?
        int failed = 0;          // 失败�?

        // 细分错误统计
        QMap<QString, int> failureReasons; // 例如: {"Network Error": 5, "Parse Error": 2}
        QMap<QString, int> stageFailures;  // 例如: {"Fetch": 5, "Process": 2, "Write": 0}

        // 时间统计
        qint64 totalDurationMs = 0;     // 总耗时
        qint64 avgFetchTimeMs = 0;      // 平均抓取时间
        qint64 avgProcessTimeMs = 0;    // 平均处理时间
        qint64 avgWriteTimeMs = 0;      // 平均写入时间

        // 最慢的组件（用于性能分析�?
        QList<QPair<QString, qint64>> slowestComponents; // <componentId, durationMs>

        /**
         * @brief 计算成功�?
         * @return double 成功率（0-100�?
         */
        double getSuccessRate() const
        {
            return total > 0 ? (success * 100.0 / total) : 0.0;
        }

        /**
         * @brief 获取最慢阶�?
         * @return QString 最慢阶段名�?
         */
        QString getSlowestStage() const
        {
            if (avgFetchTimeMs >= avgProcessTimeMs && avgFetchTimeMs >= avgWriteTimeMs)
                return "Fetch";
            if (avgProcessTimeMs >= avgFetchTimeMs && avgProcessTimeMs >= avgWriteTimeMs)
                return "Process";
            return "Write";
        }

        /**
         * @brief 获取统计摘要
         * @return QString 统计摘要
         */
        QString getSummary() const
        {
            return QString("Total: %1, Success: %2, Failed: %3, Success Rate: %4%, Duration: %5s")
                       .arg(total)
                       .arg(success)
                       .arg(failed)
                       .arg(total > 0 ? QString::number(success * 100.0 / total, 'f', 2) : "0.00")
                       .arg(totalDurationMs / 1000.0, 0, 'f', 2);
        }

        /**
         * @brief 获取详细统计摘要
         * @return QString 详细统计摘要
         */
        QString getDetailedSummary() const
        {
            QString summary = getSummary();
            summary += QString("\n\nTiming:\n");
            summary += QString("  Avg Fetch: %1ms\n").arg(avgFetchTimeMs);
            summary += QString("  Avg Process: %1ms\n").arg(avgProcessTimeMs);
            summary += QString("  Avg Write: %1ms\n").arg(avgWriteTimeMs);
            summary += QString("  Slowest Stage: %1\n").arg(getSlowestStage());

            if (failed > 0)
            {
                summary += QString("\nFailures:\n");
                for (auto it = stageFailures.constBegin(); it != stageFailures.constEnd(); ++it)
                {
                    summary += QString("  %1: %2\n").arg(it.key()).arg(it.value());
                }
            }

            return summary;
        }
    };

} // namespace EasyKiConverter

#endif // COMPONENTEXPORTSTATUS_H
