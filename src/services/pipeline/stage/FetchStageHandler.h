#ifndef EASYKICONVERTER_FETCHSTAGEHANDLER_H
#define EASYKICONVERTER_FETCHSTAGEHANDLER_H

#include "../../../models/ComponentData.h"
#include "../../../models/ComponentExportStatus.h"
#include "../../../workers/FetchWorker.h"
#include "../../ExportService.h"  // ExportOptions 定义在此文件中
#include "StageHandler.h"

#include <QMap>
#include <QNetworkAccessManager>
#include <QSet>
#include <QThreadPool>

namespace EasyKiConverter {

/**
 * @brief 抓取阶段处理器
 */
class FetchStageHandler : public StageHandler {
    Q_OBJECT
public:
    FetchStageHandler(QAtomicInt& isCancelled,
                      QNetworkAccessManager* networkManager,
                      QThreadPool* threadPool,
                      QObject* parent = nullptr);

    /**
     * @brief 启动抓取阶段
     */
    void start() override;

    void setComponentIds(const QStringList& componentIds) {
        m_componentIds = componentIds;
    }

    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

    void setPreloadedData(const QMap<QString, QSharedPointer<ComponentData>>& data) {
        m_preloadedData = data;
    }

    void stop() override;

signals:
    /**
     * @brief 当一个元器件抓取完成时发出
     * @param status 导出状态
     * @param isPreloaded 是否是预加载数据
     */
    void componentFetchCompleted(QSharedPointer<ComponentExportStatus> status, bool isPreloaded);

private slots:
    void onWorkerCompleted(QSharedPointer<ComponentExportStatus> status, FetchWorker* worker);

private:
    QNetworkAccessManager* m_networkManager;
    QThreadPool* m_threadPool;
    QStringList m_componentIds;
    ExportOptions m_options;
    QMap<QString, QSharedPointer<ComponentData>> m_preloadedData;
    QSet<FetchWorker*> m_activeWorkers;
    QMutex m_workerMutex;
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_FETCHSTAGEHANDLER_H
