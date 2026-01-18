#ifndef MOCKNETWORKUTILS_H
#define MOCKNETWORKUTILS_H

#include "src/core/interfaces/INetworkUtils.h"
#include <QJsonObject>
#include <QByteArray>
#include <QList>
#include <QMap>

namespace EasyKiConverter {

/**
 * @brief NetworkUtils 的 Mock 实现
 *
 * 用于单元测试，支持设置返回值和错误模式
 * 记录所有调用历史以便验证
 */
class MockNetworkUtils : public INetworkUtils
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit MockNetworkUtils(QObject *parent = nullptr) : INetworkUtils(parent) {}

    /**
     * @brief 析构函数
     */
    ~MockNetworkUtils() override = default;

    // ========== INetworkUtils 接口实现 ==========

    void sendGetRequest(const QString &url, int timeout = 30, int maxRetries = 3) override {
        m_sendGetRequest_calls.append({url, timeout, maxRetries});
        m_lastUrl = url;

        if (m_errorMode) {
            emit requestError(m_errorMessage);
            return;
        }

        if (m_delayMode) {
            QTimer::singleShot(m_delayMs, this, [this]() {
                if (m_expectBinaryData) {
                    emit binaryDataFetched(m_mockBinaryData);
                } else {
                    emit requestSuccess(m_mockJsonData);
                }
            });
        } else {
            if (m_expectBinaryData) {
                emit binaryDataFetched(m_mockBinaryData);
            } else {
                emit requestSuccess(m_mockJsonData);
            }
        }
    }

    void cancelRequest() override {
        m_cancelRequest_calls.append(true);
    }

    void setHeader(const QString &key, const QString &value) override {
        m_setHeader_calls.append({key, value});
        m_headers[key] = value;
    }

    void clearHeaders() override {
        m_clearHeaders_calls.append(true);
        m_headers.clear();
    }

    void setExpectBinaryData(bool expectBinaryData) override {
        m_setExpectBinaryData_calls.append(expectBinaryData);
        m_expectBinaryData = expectBinaryData;
    }

    // ========== Mock 设置方法 ==========

    /**
     * @brief 设置 Mock JSON 响应数据
     *
     * @param data JSON 数据
     */
    void setMockJsonData(const QJsonObject &data) {
        m_mockJsonData = data;
    }

    /**
     * @brief 设置 Mock 二进制响应数据
     *
     * @param data 二进制数据
     */
    void setMockBinaryData(const QByteArray &data) {
        m_mockBinaryData = data;
    }

    /**
     * @brief 设置错误模式
     *
     * @param error 是否启用错误模式
     * @param errorMessage 错误消息
     */
    void setErrorMode(bool error, const QString &errorMessage = "Mock network error") {
        m_errorMode = error;
        m_errorMessage = errorMessage;
    }

    /**
     * @brief 设置延迟模式（模拟网络延迟）
     *
     * @param delayMs 延迟时间（毫秒）
     */
    void setDelayMode(int delayMs) {
        m_delayMode = true;
        m_delayMs = delayMs;
    }

    /**
     * @brief 清除延迟模式
     */
    void clearDelayMode() {
        m_delayMode = false;
        m_delayMs = 0;
    }

    // ========== 调用历史查询方法 ==========

    /**
     * @brief 获取 sendGetRequest 调用历史
     *
     * @return QList<QVariantList> 调用参数列表（每个元素是 [url, timeout, maxRetries]）
     */
    QList<QVariantList> getSendGetRequestCalls() const {
        QList<QVariantList> result;
        for (const auto &call : m_sendGetRequest_calls) {
            result.append({call.url, call.timeout, call.maxRetries});
        }
        return result;
    }

    /**
     * @brief 获取 cancelRequest 调用次数
     *
     * @return int 调用次数
     */
    int getCancelRequestCalls() const {
        return m_cancelRequest_calls.size();
    }

    /**
     * @brief 获取 setHeader 调用历史
     *
     * @return QList<QVariantList> 调用参数列表（每个元素是 [key, value]）
     */
    QList<QVariantList> getSetHeaderCalls() const {
        QList<QVariantList> result;
        for (const auto &call : m_setHeader_calls) {
            result.append({call.key, call.value});
        }
        return result;
    }

    /**
     * @brief 获取 clearHeaders 调用次数
     *
     * @return int 调用次数
     */
    int getClearHeadersCalls() const {
        return m_clearHeaders_calls.size();
    }

    /**
     * @brief 获取 setExpectBinaryData 调用历史
     *
     * @return QList<bool> 调用参数列表
     */
    QList<bool> getSetExpectBinaryDataCalls() const {
        return m_setExpectBinaryData_calls;
    }

    /**
     * @brief 获取当前设置的请求头
     *
     * @return QMap<QString, QString> 请求头
     */
    QMap<QString, QString> getHeaders() const {
        return m_headers;
    }

    /**
     * @brief 清除所有调用历史
     */
    void clearCallHistory() {
        m_sendGetRequest_calls.clear();
        m_cancelRequest_calls.clear();
        m_setHeader_calls.clear();
        m_clearHeaders_calls.clear();
        m_setExpectBinaryData_calls.clear();
    }

private:
    // Mock 数据
    QJsonObject m_mockJsonData;
    QByteArray m_mockBinaryData;

    // Mock 行为控制
    bool m_errorMode = false;
    QString m_errorMessage;
    bool m_delayMode = false;
    int m_delayMs = 0;
    bool m_expectBinaryData = false;

    // 当前状态
    QMap<QString, QString> m_headers;

    // 调用历史
    struct SendGetRequestCall {
        QString url;
        int timeout;
        int maxRetries;
    };
    QList<SendGetRequestCall> m_sendGetRequest_calls;
    QList<bool> m_cancelRequest_calls;

    struct SetHeaderCall {
        QString key;
        QString value;
    };
    QList<SetHeaderCall> m_setHeader_calls;
    QList<bool> m_clearHeaders_calls;
    QList<bool> m_setExpectBinaryData_calls;

    // 最后调用的参数
    QString m_lastUrl;
};

} // namespace EasyKiConverter

#endif // MOCKNETWORKUTILS_H