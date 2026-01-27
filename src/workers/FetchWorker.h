#ifndef FETCHWORKER_H
#define FETCHWORKER_H

#include "models/ComponentExportStatus.h"

#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QRunnable>

namespace EasyKiConverter {

/**
 * @brief 抓取工作线程
 *
 * 负责从网络下载原始数据（I/O密集型任务）
 */
class FetchWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief 构造函�?
         * @param componentId 元件ID
     * @param networkAccessManager 共享的网络访问管理器
     * @param need3DModel 是否需�?D模型
     * @param parent 父对�?
         */
    explicit FetchWorker(const QString& componentId,
                         QNetworkAccessManager* networkAccessManager,
                         bool need3DModel,
                         QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FetchWorker() override;

    /**
     * @brief 执行抓取任务
     */
    void run() override;

signals:
    /**
     * @brief 抓取完成信号
     * @param status 导出状态（使用 QSharedPointer 避免拷贝�?
         */
    void fetchCompleted(QSharedPointer<ComponentExportStatus> status);

private:
    /**
     * @brief 执行HTTP GET请求
     * @param url URL
     * @param timeoutMs 超时时间（毫秒）
     * @param status 导出状态对象（用于记录网络诊断）
     * @return QByteArray 响应数据
     */
    QByteArray httpGet(const QString& url, int timeoutMs = 30000, QSharedPointer<ComponentExportStatus> status = nullptr);

    /**
     * @brief 解压gzip数据
     * @param compressedData 压缩的数�?
         * @return QByteArray 解压后的数据
     */
    QByteArray decompressGzip(const QByteArray& compressedData);

    /**
     * @brief 解压ZIP数据
     * @param zipData ZIP数据
     * @return QByteArray 解压后的数据
     */
    QByteArray decompressZip(const QByteArray& zipData);

    /**
     * @brief 下载3D模型数据
     * @param status 导出状�?
         * @return bool 是否成功
     */
    bool fetch3DModelData(QSharedPointer<ComponentExportStatus> status);

private:
    QString m_componentId;
    QNetworkAccessManager* m_networkAccessManager;
    QNetworkAccessManager* m_ownNetworkManager;
    bool m_need3DModel;

    // 速率限制检测（静态成员，所有 FetchWorker 共享）
    static QAtomicInt s_activeRequests;    // 活跃请求计数
    static QMutex s_rateLimitMutex;        // 速率限制互斥锁
    static QDateTime s_lastRateLimitTime;  // 上次触发速率限制的时间
    static int s_backoffMs;                // 退避延迟（毫秒）
};

}  // namespace EasyKiConverter

#endif  // FETCHWORKER_H
