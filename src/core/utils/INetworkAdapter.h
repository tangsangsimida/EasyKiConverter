#line 1 "c:/Users/48813/Desktop/workspace/github_projects/EasyKiConverter_QT/src/core/utils/INetworkAdapter.h"
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 网络适配器接口
 */
class INetworkAdapter : public QObject {
    Q_OBJECT
public:
    explicit INetworkAdapter(QObject* parent = nullptr) : QObject(parent) {}

    virtual ~INetworkAdapter();

    virtual void sendGetRequest(const QString& url, int timeout = 30, int maxRetries = 3) = 0;
    virtual void cancelRequest() = 0;
    virtual void setHeader(const QString& key, const QString& value) = 0;
    virtual void clearHeaders() = 0;
    virtual void setExpectBinaryData(bool expectBinaryData) = 0;

signals:
    // 接口层声明信号，Qt MOC 会处理这些信号的实现。
    // 子类不需要也不应该重新定义这些信号，只需在 emit 时使用它们。
    void requestSuccess(const QJsonObject& data);
    void binaryDataFetched(const QByteArray& data);
    void requestError(const QString& errorMessage);
    void requestProgress(qint64 bytesReceived, qint64 bytesTotal);
};

}  // namespace EasyKiConverter
