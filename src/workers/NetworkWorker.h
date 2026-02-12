#ifndef NETWORKWORKER_H
#define NETWORKWORKER_H

#include <QJsonObject>
#include <QMutex>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QTimer>

namespace EasyKiConverter {

/**
 * @brief 网络工作线程
 *
 * 用于在后台线程中执行网络请求任务
 */
class NetworkWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    /**
     * @brief 任务类型
     */
    enum class TaskType { FetchComponentInfo, FetchCadData, Fetch3DModelObj, Fetch3DModelMtl };

    /**
     * @brief 构造函数
         * @param componentId 元件ID
     * @param taskType 任务类型
     * @param uuid UUID（用于3D模型下载）
         * @param parent 父对象
         */
    explicit NetworkWorker(const QString& componentId,
                           TaskType taskType,
                           const QString& uuid = QString(),
                           QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~NetworkWorker() override;

    /**
     * @brief 执行网络请求任务
     */
    void run() override;

signals:
    /**
     * @brief 组件信息获取完成信号
     * @param componentId 元件ID
     * @param data 组件数据
     */
    void componentInfoFetched(const QString& componentId, const QJsonObject& data);

    /**
     * @brief CAD数据获取完成信号
     * @param componentId 元件ID
     * @param data CAD数据
     */
    void cadDataFetched(const QString& componentId, const QJsonObject& data);

    /**
     * @brief 3D模型数据获取完成信号
     * @param componentId 元件ID
     * @param uuid UUID
     * @param data 模型数据
     */
    void model3DFetched(const QString& componentId, const QString& uuid, const QByteArray& data);

    /**
     * @brief 网络请求失败信号
     * @param componentId 元件ID
     * @param errorMessage 错误消息
     */
    void fetchError(const QString& componentId, const QString& errorMessage);

    /**
     * @brief 请求进度信号
     * @param componentId 元件ID
     * @param progress 进度（0-100）
         */
    void requestProgress(const QString& componentId, int progress);

private:
    /**
     * @brief 获取组件信息
     * @return bool 是否成功
     */
    bool fetchComponentInfo();

    /**
     * @brief 获取CAD数据
     * @return bool 是否成功
     */
    bool fetchCadData();

    /**
     * @brief 获取3D模型OBJ数据
     * @return bool 是否成功
     */
    bool fetch3DModelObj();

    /**
     * @brief 获取3D模型MTL数据
     * @return bool 是否成功
     */
    bool fetch3DModelMtl();

    /**
     * @brief 解压gzip数据
     * @param compressedData 压缩的数据
         * @return QByteArray 解压后的数据
     */
    QByteArray decompressGzip(const QByteArray& compressedData);

public slots:
    /**
     * @brief 中断当前网络请求
     */
    void abort();

    /**
     * @brief 执行网络请求（含超时和重试）
     * @param manager 网络访问管理器
     * @param request 网络请求
     * @param timeoutMs 超时时间（毫秒）
     * @param maxRetries 最大重试次数
     * @param outData 响应数据输出
     * @param errorMsg 错误消息输出
     * @return bool 是否成功
     */
    bool executeRequest(QNetworkAccessManager& manager,
                        const QNetworkRequest& request,
                        int timeoutMs,
                        int maxRetries,
                        QByteArray& outData,
                        QString& errorMsg);

private:
    QString m_componentId;
    TaskType m_taskType;
    QString m_uuid;
    QPointer<QNetworkReply> m_currentReply;
    QMutex m_mutex;

    // 超时配置
    static const int DEFAULT_TIMEOUT_MS = 30000;                   // 默认超时 30 秒
    static const int MODEL_TIMEOUT_MS = 45000;                     // 3D 模型超时 45 秒
    static const int MAX_RETRIES = 3;                              // 最大重试次数
    static constexpr int RETRY_DELAYS_MS[] = {3000, 5000, 10000};  // 递增重试延迟
};

}  // namespace EasyKiConverter

#endif  // NETWORKWORKER_H
