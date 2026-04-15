#ifndef MOCKNETWORKCLIENT_HPP
#define MOCKNETWORKCLIENT_HPP

#include "core/network/AsyncNetworkRequest.h"
#include "core/network/INetworkClient.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace EasyKiConverter::Test {

class MockNetworkClient : public INetworkClient {
public:
    struct ResponseSpec {
        NetworkResult result;
    };

    void addJsonResponse(const QString& url, const QJsonObject& object, int statusCode = 200) {
        NetworkResult result;
        result.success = true;
        result.statusCode = statusCode;
        result.data = QJsonDocument(object).toJson(QJsonDocument::Compact);
        result.diagnostic.url = url;
        m_getResponses.insert(url, result);
    }

    void addErrorResponse(const QString& url, const QString& error, int statusCode = 0) {
        NetworkResult result;
        result.success = false;
        result.statusCode = statusCode;
        result.error = error;
        result.diagnostic.url = url;
        m_getResponses.insert(url, result);
    }

    QString lastUrl() const {
        return m_lastUrl;
    }

    NetworkResult get(const QUrl& url, const RetryPolicy& policy = {}) override {
        return get(url, ResourceType::Unknown, policy);
    }

    NetworkResult get(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override {
        Q_UNUSED(policy);
        m_lastUrl = url.toString();
        NetworkResult result = m_getResponses.value(m_lastUrl);
        result.diagnostic.url = m_lastUrl;
        result.diagnostic.host = url.host();
        result.diagnostic.resourceType = resourceType;
        result.diagnostic.profileName = RequestProfiles::fromType(resourceType).name;
        return result;
    }

    NetworkResult post(const QUrl& url, const QByteArray& body, const RetryPolicy& policy = {}) override {
        return post(url, body, ResourceType::Unknown, policy);
    }

    NetworkResult post(const QUrl& url,
                       const QByteArray& body,
                       ResourceType resourceType,
                       const RetryPolicy& policy = {}) override {
        Q_UNUSED(body);
        Q_UNUSED(policy);
        m_lastUrl = url.toString();
        NetworkResult result = m_postResponses.value(m_lastUrl);
        result.diagnostic.url = m_lastUrl;
        result.diagnostic.host = url.host();
        result.diagnostic.resourceType = resourceType;
        result.diagnostic.profileName = RequestProfiles::fromType(resourceType).name;
        return result;
    }

    AsyncNetworkRequest* getAsync(const QUrl& url, ResourceType resourceType, const RetryPolicy& policy = {}) override {
        Q_UNUSED(policy);
        return AsyncNetworkRequest::createFinished(get(url, resourceType, policy));
    }

    AsyncNetworkRequest* postAsync(const QUrl& url,
                                   const QByteArray& body,
                                   ResourceType resourceType,
                                   const RetryPolicy& policy = {}) override {
        Q_UNUSED(policy);
        return AsyncNetworkRequest::createFinished(post(url, body, resourceType, policy));
    }

private:
    QHash<QString, NetworkResult> m_getResponses;
    QHash<QString, NetworkResult> m_postResponses;
    QString m_lastUrl;
};

}  // namespace EasyKiConverter::Test

#endif  // MOCKNETWORKCLIENT_HPP
