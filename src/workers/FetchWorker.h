#ifndef FETCHWORKER_H
#define FETCHWORKER_H

#include <QObject>
#include <QRunnable>
#include <QNetworkAccessManager>
#include "src/models/ComponentExportStatus.h"

namespace EasyKiConverter {

/**
 * @brief 抓取工作线程
 *
 * 负责从网络下载原始数据（I/O密集型任务）
 */
class FetchWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param componentId 元件ID
     * @param networkAccessManager 共享的网络访问管理器
     * @param need3DModel 是否需要3D模型
     * @param parent 父对象
     */
    explicit FetchWorker(
        const QString &componentId,
        QNetworkAccessManager *networkAccessManager,
        bool need3DModel,
        QObject *parent = nullptr);

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
     * @param status 导出状态
     */
    void fetchCompleted(const ComponentExportStatus &status);

private:
    /**
     * @brief 抓取组件信息
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool fetchComponentInfo(ComponentExportStatus &status);

    /**
     * @brief 抓取CAD数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool fetchCadData(ComponentExportStatus &status);

    /**
     * @brief 抓取3D模型数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool fetch3DModelData(ComponentExportStatus &status);

    /**
     * @brief 执行HTTP GET请求
     * @param url URL
     * @param timeoutMs 超时时间（毫秒）
     * @return QByteArray 响应数据
     */
    QByteArray httpGet(const QString &url, int timeoutMs = 30000);

    /**
     * @brief 解压gzip数据
     * @param compressedData 压缩的数据
     * @return QByteArray 解压后的数据
     */
    QByteArray decompressGzip(const QByteArray &compressedData);

private:
    QString m_componentId;
    QNetworkAccessManager *m_networkAccessManager;
    bool m_need3DModel;
};

} // namespace EasyKiConverter

#endif // FETCHWORKER_H