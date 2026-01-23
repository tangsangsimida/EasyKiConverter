#ifndef NETWORKWORKER_H
#define NETWORKWORKER_H

#include <QJsonObject>
#include <QNetworkReply>
#include <QObject>
#include <QRunnable>

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
     * @brief 构造函�?
         * @param componentId 元件ID
     * @param taskType 任务类型
     * @param uuid UUID（用�?D模型下载�?
         * @param parent 父对�?
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
     * @param progress 进度�?-100�?
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
     * @param compressedData 压缩的数�?
         * @return QByteArray 解压后的数据
     */
    QByteArray decompressGzip(const QByteArray& compressedData);

private:
    QString m_componentId;
    TaskType m_taskType;
    QString m_uuid;
};

}  // namespace EasyKiConverter

#endif  // NETWORKWORKER_H
