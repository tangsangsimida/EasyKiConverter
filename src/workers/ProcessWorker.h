#ifndef PROCESSWORKER_H
#define PROCESSWORKER_H

#include <QObject>
#include <QRunnable>
#include <QNetworkAccessManager>
#include "src/models/ComponentExportStatus.h"

namespace EasyKiConverter {

/**
 * @brief 处理工作线程
 *
 * 负责解析和转换数据（CPU密集型任务）
 */
class ProcessWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param status 导出状态
     * @param parent 父对象
     */
    explicit ProcessWorker(const ComponentExportStatus &status, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ProcessWorker() override;

    /**
     * @brief 执行处理任务
     */
    void run() override;

signals:
    /**
     * @brief 处理完成信号
     * @param status 导出状态
     */
    void processCompleted(const ComponentExportStatus &status);

private:
    /**
     * @brief 解析组件信息
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool parseComponentInfo(ComponentExportStatus &status);

    /**
     * @brief 解析CAD数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool parseCadData(ComponentExportStatus &status);

    /**
     * @brief 下载3D模型数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool fetch3DModelData(ComponentExportStatus &status);

    /**
     * @brief 解析3D模型数据
     * @param status 导出状态
     * @return bool 是否成功
     */
    bool parse3DModelData(ComponentExportStatus &status);

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

    /**
     * @brief 解压ZIP数据
     * @param zipData ZIP数据
     * @return QByteArray 解压后的数据
     */
    QByteArray decompressZip(const QByteArray &zipData);

    /**
     * @brief 清理资源
     */
    void cleanup();

private:
    ComponentExportStatus m_status;
    QNetworkAccessManager *m_networkAccessManager;
};

} // namespace EasyKiConverter

#endif // PROCESSWORKER_H