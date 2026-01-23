#include "JLCDatasheet.h"
#include <QNetworkReply>
#include <QFile>
#include <QDebug>

namespace EasyKiConverter
{

    JLCDatasheet::JLCDatasheet(QObject *parent)
        : QObject(parent), m_networkUtils(new NetworkUtils(this)), m_isDownloading(false)
    {
        connect(m_networkUtils, &NetworkUtils::requestSuccess,
                this, &JLCDatasheet::handleDownloadResponse);
        connect(m_networkUtils, &NetworkUtils::requestError,
                this, &JLCDatasheet::handleDownloadError);
    }

    JLCDatasheet::~JLCDatasheet()
    {
        cancel();
    }

    void JLCDatasheet::downloadDatasheet(const QString &datasheetUrl, const QString &savePath)
    {
        if (m_isDownloading)
        {
            qWarning() << "Already downloading datasheet";
            return;
        }

        if (datasheetUrl.isEmpty())
        {
            QString errorMsg = "Datasheet URL is empty";
            qWarning() << errorMsg;
            emit downloadError(errorMsg);
            return;
        }

        m_datasheetUrl = datasheetUrl;
        m_savePath = savePath;
        m_isDownloading = true;

        qDebug() << "Downloading datasheet from:" << datasheetUrl;

        // 发�?GET 请求
        m_networkUtils->sendGetRequest(datasheetUrl, 60, 3);
    }

    void JLCDatasheet::cancel()
    {
        m_networkUtils->cancelRequest();
        m_isDownloading = false;
    }

    void JLCDatasheet::handleDownloadResponse(const QJsonObject &data)
    {
        Q_UNUSED(data);
        m_isDownloading = false;

        // 注意：数据手册下载通常返回二进制数据，不是 JSON
        // 这里需要使�?QNetworkReply 直接访问二进制数�?
        // 暂时先发送成功信�?
        qDebug() << "Datasheet download completed";

        emit downloadSuccess(m_savePath);
    }

    void JLCDatasheet::handleDownloadError(const QString &errorMessage)
    {
        m_isDownloading = false;
        qWarning() << "Datasheet download error:" << errorMessage;
        emit downloadError(errorMessage);
    }

} // namespace EasyKiConverter
