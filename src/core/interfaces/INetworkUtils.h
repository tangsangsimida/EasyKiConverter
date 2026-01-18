#ifndef INETWORKUTILS_H
#define INETWORKUTILS_H

#include <QObject>
#include <QJsonObject>
#include <QByteArray>

namespace EasyKiConverter {

/**
 * @brief 网络工具接口类
 *
 * 定义了网络请求的标准接口
 * 用于依赖注入和单元测试
 */
class INetworkUtils : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit INetworkUtils(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief 析构函数
     */
    virtual ~INetworkUtils() = default;

    /**
     * @brief 发送 HTTP GET 请求
     *
     * @param url 请求 URL
     * @param timeout 超时时间（秒），默认 30 秒
     * @param maxRetries 最大重试次数，默认 3 次
     */
    virtual void sendGetRequest(const QString &url, int timeout = 30, int maxRetries = 3) = 0;

    /**
     * @brief 取消当前请求
     */
    virtual void cancelRequest() = 0;

    /**
     * @brief 设置请求头
     *
     * @param key 头字段名
     * @param value 头字段值
     */
    virtual void setHeader(const QString &key, const QString &value) = 0;

    /**
     * @brief 清除所有请求头
     */
    virtual void clearHeaders() = 0;

    /**
     * @brief 设置是否期望接收二进制数据
     *
     * @param expectBinaryData 是否期望接收二进制数据
     */
    virtual void setExpectBinaryData(bool expectBinaryData) = 0;

signals:
    /**
     * @brief 请求成功信号
     *
     * @param data 响应数据（JSON 格式）
     */
    void requestSuccess(const QJsonObject &data);

    /**
     * @brief 二进制数据获取成功信号
     *
     * @param binaryData 二进制数据
     */
    void binaryDataFetched(const QByteArray &binaryData);

    /**
     * @brief 请求失败信号
     *
     * @param errorMessage 错误消息
     */
    void requestError(const QString &errorMessage);

    /**
     * @brief 请求进度信号
     *
     * @param bytesReceived 已接收字节数
     * @param bytesTotal 总字节数
     */
    void requestProgress(qint64 bytesReceived, qint64 bytesTotal);
};

} // namespace EasyKiConverter

#endif // INETWORKUTILS_H