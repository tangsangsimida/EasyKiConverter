#ifndef MOCKNETWORKADAPTER_HPP
#define MOCKNETWORKADAPTER_HPP

#include "core/utils/INetworkAdapter.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QTimer>


namespace EasyKiConverter::Test {

/**
 * @brief 网络适配器 Mock 类
 */
class MockNetworkAdapter : public INetworkAdapter {
    Q_OBJECT
public:
    explicit MockNetworkAdapter(QObject* parent = nullptr) : INetworkAdapter(parent) {}
    ~MockNetworkAdapter() override = default;

    void sendGetRequest(const QString& url, int timeout = 30, int maxRetries = 3) override {
        Q_UNUSED(timeout);
        Q_UNUSED(maxRetries);
        m_lastUrl = url;

        // 延迟触发信号，模拟网络异步行为并确保 connect 已完成
        QTimer::singleShot(0, this, [this, url]() {
            if (m_responses.contains(url)) {
                emit requestSuccess(m_responses[url]);
            } else if (m_binaryResponses.contains(url)) {
                emit binaryDataFetched(m_binaryResponses[url]);
            } else {
                emit requestError("Mock: No response defined for " + url);
            }
        });
    }

    void cancelRequest() override {}
    void setHeader(const QString& key, const QString& value) override {
        m_headers[key] = value;
    }
    void clearHeaders() override {
        m_headers.clear();
    }
    void setExpectBinaryData(bool expectBinaryData) override {
        m_expectBinaryData = expectBinaryData;
    }

    // Helper methods for testing
    void addJsonResponse(const QString& url, const QJsonObject& data) {
        m_responses[url] = data;
    }
    void addBinaryResponse(const QString& url, const QByteArray& data) {
        m_binaryResponses[url] = data;
    }
    QString lastUrl() const {
        return m_lastUrl;
    }

private:
    QMap<QString, QJsonObject> m_responses;
    QMap<QString, QByteArray> m_binaryResponses;
    QMap<QString, QString> m_headers;
    QString m_lastUrl;
    bool m_expectBinaryData = false;
};

}  // namespace EasyKiConverter::Test

#endif  // MOCKNETWORKADAPTER_HPP
