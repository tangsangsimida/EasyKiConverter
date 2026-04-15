#ifndef EASYEDAAPI_H
#define EASYEDAAPI_H

#include "core/network/INetworkClient.h"

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

class AsyncNetworkRequest;

class EasyedaApi : public QObject {
    Q_OBJECT

public:
    explicit EasyedaApi(QObject* parent = nullptr);
    explicit EasyedaApi(INetworkClient* networkClient, QObject* parent = nullptr);
    ~EasyedaApi() override;

    void fetchComponentInfo(const QString& lcscId);
    void fetchCadData(const QString& lcscId);
    void fetch3DModelObj(const QString& uuid);
    void fetch3DModelStep(const QString& uuid);
    void cancelRequest();

signals:
    void componentInfoFetched(const QString& lcscId, const QJsonObject& data);
    void cadDataFetched(const QString& lcscId, const QJsonObject& data);
    void model3DFetched(const QString& uuid, const QByteArray& data);
    void fetchError(const QString& errorMessage);
    void fetchError(const QString& id, const QString& errorMessage);

private:
    void handleComponentInfoResponse(const QString& lcscId, const QJsonObject& data);
    void handleCadDataResponse(const QString& lcscId, const QJsonObject& data);
    void handleAsyncRequestFinished(AsyncNetworkRequest* request,
                                    const QString& id,
                                    ResourceType resourceType,
                                    bool isBinary);
    void fetchWithNetworkClient(const QString& id, const QUrl& url, ResourceType resourceType, bool isBinary);
    void resetRequestState();
    QString buildComponentApiUrl(const QString& lcscId) const;
    QString build3DModelObjUrl(const QString& uuid) const;
    QString build3DModelStepUrl(const QString& uuid) const;
    bool validateLcscId(const QString& lcscId) const;

private:
    INetworkClient* m_networkClient;
    QString m_currentLcscId;
    QString m_currentUuid;
    bool m_isFetching;
    QVector<QPointer<AsyncNetworkRequest>> m_activeRequests;
    QMutex m_requestsMutex;
};

}  // namespace EasyKiConverter

#endif  // EASYEDAAPI_H
