#ifndef COMPONENTEXPORTSTATUS_H
#define COMPONENTEXPORTSTATUS_H

#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QElapsedTimer>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 元件导出状态
 *
 * 跟踪元件在流水线各阶段的状态
 */
struct ComponentExportStatus {
    QString componentId;

    // 原始数据
    QByteArray componentInfoRaw;
    QByteArray cinfoJsonRaw;
    QByteArray cadDataRaw;
    QByteArray cadJsonRaw;
    QByteArray advJsonRaw;
    QByteArray model3DObjRaw;
    QByteArray model3DStepRaw;

    // 解析后的数据
    QSharedPointer<ComponentData> componentData;
    QSharedPointer<SymbolData> symbolData;
    QSharedPointer<FootprintData> footprintData;
    QSharedPointer<Model3DData> model3DData;

    // 状态标志
    bool fetchSuccess = false;
    QString fetchMessage;
    qint64 fetchDurationMs = 0;

    bool processSuccess = false;
    QString processMessage;
    qint64 processDurationMs = 0;

    bool writeSuccess = false;
    QString writeMessage;
    qint64 writeDurationMs = 0;

    // 取消标志 (v3.0.5+)
    bool isCancelled = false;

    // 详细成果标志位 (v3.0.4)
    bool symbolWritten = false;
    bool footprintWritten = false;
    bool model3DWritten = false;

    QStringList debugLog;
    bool need3DModel = false;

    struct NetworkDiagnostics {
        QString url;
        int statusCode = 0;
        QString errorString;
        int retryCount = 0;
        qint64 latencyMs = 0;
        bool wasRateLimited = false;
    };
    QList<NetworkDiagnostics> networkDiagnostics;

    bool isCompleteSuccess() const {
        return fetchSuccess && processSuccess && writeSuccess;
    }

    QString getFailedStage() const {
        if (!fetchSuccess)
            return "Fetch";
        if (!processSuccess)
            return "Process";
        if (!writeSuccess)
            return "Write";
        return "";
    }

    QString getFailureReason() const {
        if (!fetchSuccess)
            return fetchMessage;
        if (!processSuccess)
            return processMessage;
        if (!writeSuccess)
            return writeMessage;
        return "";
    }

    qint64 getTotalDurationMs() const {
        return fetchDurationMs + processDurationMs + writeDurationMs;
    }

    void addDebugLog(const QString& message) {
        debugLog.append(message);
    }
};

struct ExportStatistics {
    int total = 0;
    int success = 0;
    int failed = 0;

    // 详细分项统计 (v3.0.4)
    int successSymbol = 0;
    int successFootprint = 0;
    int successModel3D = 0;

    QMap<QString, int> failureReasons;
    QMap<QString, int> stageFailures;

    qint64 totalDurationMs = 0;
    qint64 avgFetchTimeMs = 0;
    qint64 avgProcessTimeMs = 0;
    qint64 avgWriteTimeMs = 0;

    int totalNetworkRequests = 0;
    int totalRetries = 0;
    qint64 avgNetworkLatencyMs = 0;
    int rateLimitHitCount = 0;
    QMap<int, int> statusCodeDistribution;

    double getSuccessRate() const {
        return total > 0 ? (success * 100.0 / total) : 0.0;
    }

    QString getSummary() const {
        return QString("Total: %1, Success: %2, Failed: %3, Symbol: %4, Footprint: %5, 3D: %6")
            .arg(total)
            .arg(success)
            .arg(failed)
            .arg(successSymbol)
            .arg(successFootprint)
            .arg(successModel3D);
    }
};

}  // namespace EasyKiConverter

#endif  // COMPONENTEXPORTSTATUS_H