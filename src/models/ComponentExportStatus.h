#ifndef COMPONENTEXPORTSTATUS_H
#define COMPONENTEXPORTSTATUS_H

#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

struct MemorySnapshot {
    qint64 rawDataSize = 0;
    qint64 parsedModelSize = 0;  // 估算值
    qint64 totalSize = 0;
};

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
    bool fetch3DOnly = false;  // 是否是仅获取 3D 模式（符号和封装已从预加载数据复用）

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

    // 内存管理增强 (v3.0.5)

    // 清理中间数据（Process 阶段后调用）
    // 注意：model3DStepRaw 必须保留给 WriteWorker 使用，所以在 clearStepData() 中单独清理
    void clearIntermediateData(bool log = true) {
        qint64 freed = 0;

        auto clearAndCount = [&](QByteArray& ba) {
            qint64 size = ba.capacity();  // 使用 capacity 统计实际占用
            ba.clear();
            ba.squeeze();
            return size;
        };

        freed += clearAndCount(componentInfoRaw);
        freed += clearAndCount(cinfoJsonRaw);
        freed += clearAndCount(cadDataRaw);
        freed += clearAndCount(cadJsonRaw);
        freed += clearAndCount(advJsonRaw);

        // model3DObjRaw 已经被解析到 Model3DData 中（作为 QString），可以清理
        freed += clearAndCount(model3DObjRaw);

        if (log && freed > 0) {
            // qDebug() << "Freed" << freed << "bytes of intermediate data for" << componentId;
        }
    }

    // 清理 STEP 数据（Write 阶段完成后调用）
    void clearStepData() {
        model3DStepRaw.clear();
        model3DStepRaw.squeeze();
    }

    /**
     * @brief 获取内存快照
     * @return MemorySnapshot 内存使用估算
     * @note 此方法返回的是估算值，实际内存使用可能因 Qt 内部优化而不同
     */
    MemorySnapshot getMemorySnapshot() const {
        MemorySnapshot snapshot;
        snapshot.rawDataSize = componentInfoRaw.capacity() + cinfoJsonRaw.capacity() + cadDataRaw.capacity() +
                               cadJsonRaw.capacity() + advJsonRaw.capacity() + model3DObjRaw.capacity() +
                               model3DStepRaw.capacity();

        // 估算解析后的对象大小
        if (model3DData) {
            // QString 大小 = 字符数 * 2 + Qt 元数据开销（估算32字节）
            snapshot.parsedModelSize += model3DData->rawObj().length() * 2 + sizeof(Model3DData) + 32;
        }

        if (symbolData) {
            snapshot.parsedModelSize += sizeof(SymbolData);
            snapshot.parsedModelSize += symbolData->pins().size() * sizeof(SymbolPin);
            snapshot.parsedModelSize += symbolData->rectangles().size() * sizeof(SymbolRectangle);
            snapshot.parsedModelSize += symbolData->circles().size() * sizeof(SymbolCircle);
            snapshot.parsedModelSize += symbolData->arcs().size() * sizeof(SymbolArc);
            snapshot.parsedModelSize += symbolData->polylines().size() * sizeof(SymbolPolyline);
        }

        if (footprintData) {
            snapshot.parsedModelSize += sizeof(FootprintData);
            snapshot.parsedModelSize += footprintData->pads().size() * sizeof(FootprintPad);
            snapshot.parsedModelSize += footprintData->tracks().size() * sizeof(FootprintTrack);
            snapshot.parsedModelSize += footprintData->circles().size() * sizeof(FootprintCircle);
            snapshot.parsedModelSize += footprintData->arcs().size() * sizeof(FootprintArc);
            snapshot.parsedModelSize += footprintData->texts().size() * sizeof(FootprintText);
        }

        snapshot.totalSize = snapshot.rawDataSize + snapshot.parsedModelSize;
        return snapshot;
    }
};

/**
 * @brief RAII 辅助类：确保状态对象在析构时（或显式调用时）清理数据
 * 
 * 使用场景：
 * - 在 Worker 析构时自动清理
 * - 在异常处理中确保清理
 * 
 * 注意：此 RAII 类会在析构时清理数据，不要在数据仍需使用时析构此对象
 */
class ScopedDataClearer {
public:
    explicit ScopedDataClearer(QSharedPointer<ComponentExportStatus> status) : m_status(status) {}

    ~ScopedDataClearer() {
        if (auto status = m_status.toStrongRef()) {  // 尝试获取强引用
            status->clearIntermediateData(false);
        }
    }

    void release() {
        m_status.clear();
    }

private:
    QWeakPointer<ComponentExportStatus> m_status;  // 使用弱引用，避免延长生命周期
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

    // 内存统计 (v3.0.5)
    qint64 peakMemoryUsage = 0;

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
