#ifndef EASYEDAAPI_H
#define EASYEDAAPI_H

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

class INetworkAdapter;

/**
 * @brief EasyEDA API 客户端类
 *
 * 用于与 EasyEDA 服务器通信，获取组件数据。
 */
class EasyedaApi : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit EasyedaApi(QObject* parent = nullptr);

    /**
     * @brief 构造函数（支持依赖注入）
     * @param adapter 网络适配器
     * @param parent 父对象
     */
    explicit EasyedaApi(INetworkAdapter* adapter, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~EasyedaApi() override;

    /**
     * @brief 获取组件信息
     * @param lcscId LCSC 组件 ID（应以 'C' 开头）
     */
    void fetchComponentInfo(const QString& lcscId);

    /**
     * @brief 获取组件 CAD 数据
     * @param lcscId LCSC 组件 ID
     */
    void fetchCadData(const QString& lcscId);

    /**
     * @brief 获取 3D 模型数据（OBJ 格式）
     * @param uuid 3D 模型 UUID
     */
    void fetch3DModelObj(const QString& uuid);

    /**
     * @brief 获取 3D 模型数据（STEP 格式）
     * @param uuid 3D 模型 UUID
     */
    void fetch3DModelStep(const QString& uuid);

    /**
     * @brief 取消当前请求
     */
    void cancelRequest();

signals:
    /**
     * @brief 组件信息获取成功信号
     * @param data 组件数据
     */
    void componentInfoFetched(const QJsonObject& data);

    /**
     * @brief CAD 数据获取成功信号
     * @param data CAD 数据
     */
    void cadDataFetched(const QJsonObject& data);

    /**
     * @brief 3D 模型数据获取成功信号
     * @param uuid 3D 模型 UUID
     * @param data 3D 模型数据
     */
    void model3DFetched(const QString& uuid, const QByteArray& data);

    /**
     * @brief 请求失败信号
     * @param errorMessage 错误消息
     */
    void fetchError(const QString& errorMessage);

    /**
     * @brief 请求失败信号（带 ID）
     * @param id 组件 ID 或 UUID
     * @param errorMessage 错误消息
     */
    void fetchError(const QString& id, const QString& errorMessage);

private slots:
    /**
     * @brief 处理组件信息响应
     */
    void handleComponentInfoResponse(const QJsonObject& data);

    /**
     * @brief 处理 CAD 数据响应
     */
    void handleCadDataResponse(const QJsonObject& data);

    /**
     * @brief 处理 3D 模型响应
     */
    void handleModel3DResponse(const QJsonObject& data);

    /**
     * @brief 处理网络错误
     */
    void handleNetworkError(const QString& errorMessage);

    /**
     * @brief 处理请求成功（支持并行请求）
     */
    void handleRequestSuccess(INetworkAdapter* networkUtils, const QString& lcscId, const QJsonObject& data);

    /**
     * @brief 处理请求错误（支持并行请求）
     */
    void handleRequestError(INetworkAdapter* networkUtils, const QString& lcscId, const QString& error);

    /**
     * @brief 处理二进制数据（支持并行请求）
     */
    void handleBinaryDataFetched(INetworkAdapter* networkUtils, const QString& lcscId, const QByteArray& data);

private:
    /**
     * @brief 请求类型枚举
     */
    enum class RequestType { None, ComponentInfo, CadData, Model3DObj, Model3DStep };

    /**
     * @brief 处理总体请求成功
     */
    void handleRequestSuccess(const QJsonObject& data);

    /**
     * @brief 重置请求状态
     */
    void resetRequestState();

    /**
     * @brief 构建 API URL
     */
    QString buildComponentApiUrl(const QString& lcscId) const;

    /**
     * @brief 构建 3D 模型 URL（OBJ 格式）
     */
    QString build3DModelObjUrl(const QString& uuid) const;

    /**
     * @brief 构建 3D 模型 URL（STEP 格式）
     */
    QString build3DModelStepUrl(const QString& uuid) const;

    /**
     * @brief 验证 LCSC ID 格式
     */
    bool validateLcscId(const QString& lcscId) const;

private:
    INetworkAdapter* m_networkUtils;
    QString m_currentLcscId;
    QString m_currentUuid;
    bool m_isFetching;
    RequestType m_requestType;

    // 跟踪所有活跃的并行 NetworkUtils 实例
    QVector<QPointer<INetworkAdapter>> m_activeRequests;
    QMutex m_requestsMutex;  // 保护 m_activeRequests 的线程安全访问
};

}  // namespace EasyKiConverter

#endif  // EASYEDAAPI_H
