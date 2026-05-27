#include "core/utils/UrlUtils.h"

#include "core/network/INetworkClient.h"

#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace EasyKiConverter::UrlUtils {

namespace {

// 按 ResourceType 返回允许的 host 集合
QSet<QString> allowedHostsForType(ResourceType type) {
    switch (type) {
        case ResourceType::ComponentInfo:
        case ResourceType::CadData:
            return {
                QStringLiteral("easyeda.com"),
                QStringLiteral("modules.easyeda.com"),
                QStringLiteral("lceda.cn"),
            };
        case ResourceType::ProductSearch:
            return {
                QStringLiteral("lcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("so.szlcsc.com"),
                QStringLiteral("item.szlcsc.com"),
                QStringLiteral("pro.lceda.cn"),
            };
        case ResourceType::PreviewImage:
            return {
                QStringLiteral("image.lceda.cn"),
                QStringLiteral("file.elecfans.com"),
                QStringLiteral("alimg.szlcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("lcsc.com"),
            };
        case ResourceType::Datasheet:
            return {
                QStringLiteral("file.elecfans.com"),
                QStringLiteral("image.lceda.cn"),
                QStringLiteral("lcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("item.szlcsc.com"),
                QStringLiteral("atta.szlcsc.com"),
                QStringLiteral("www.ti.com"),
                QStringLiteral("www.everlighteurope.com"),
            };
        case ResourceType::Model3DObj:
        case ResourceType::Model3DStep:
            return {
                QStringLiteral("modules.easyeda.com"),
                QStringLiteral("easyeda.com"),
                QStringLiteral("lceda.cn"),
                QStringLiteral("file.elecfans.com"),
            };
        case ResourceType::UpdateCheck:
            return {
                QStringLiteral("github.com"),
                QStringLiteral("api.github.com"),
            };
        case ResourceType::WorkerRequest:
            // WorkerRequest 是通用请求，允许所有已知 host
            return {
                QStringLiteral("easyeda.com"),
                QStringLiteral("modules.easyeda.com"),
                QStringLiteral("lceda.cn"),
                QStringLiteral("lcsc.com"),
                QStringLiteral("www.lcsc.com"),
                QStringLiteral("so.szlcsc.com"),
                QStringLiteral("item.szlcsc.com"),
                QStringLiteral("pro.lceda.cn"),
                QStringLiteral("image.lceda.cn"),
                QStringLiteral("file.elecfans.com"),
                QStringLiteral("alimg.szlcsc.com"),
                QStringLiteral("github.com"),
                QStringLiteral("api.github.com"),
            };
        case ResourceType::Unknown:
        default:
            return {};
    }
}

// 检查 host 是否匹配允许列表（支持子域名匹配）
bool isHostAllowed(const QString& host, const QSet<QString>& allowedHosts) {
    if (host.isEmpty()) {
        return false;
    }

    // 精确匹配
    if (allowedHosts.contains(host)) {
        return true;
    }

    // 子域名匹配：检查 host 是否以 ".allowedHost" 结尾
    for (const QString& allowed : allowedHosts) {
        if (host.endsWith(QLatin1Char('.') + allowed)) {
            return true;
        }
    }

    return false;
}

}  // namespace

QString normalizePreviewImageUrl(const QString& imageUrl) {
    QString normalizedUrl = imageUrl.trimmed();
    if (normalizedUrl.isEmpty()) {
        return QString();
    }

    if (normalizedUrl.startsWith(QStringLiteral("//"))) {
        normalizedUrl.prepend(QStringLiteral("https:"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/image.lceda.cn/")) ||
               normalizedUrl.startsWith(QStringLiteral("/file.elecfans.com/")) ||
               normalizedUrl.startsWith(QStringLiteral("/www.lcsc.com/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/web1/")) ||
               normalizedUrl.startsWith(QStringLiteral("/M00/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://file.elecfans.com/"));
    } else if (normalizedUrl.startsWith('/')) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://image.lceda.cn/"));
    } else if (normalizedUrl.contains("alimg.szlcsc.com")) {
        if (!normalizedUrl.startsWith("http")) {
            normalizedUrl = "https://" + normalizedUrl;
        }
    } else if (!normalizedUrl.contains("://")) {
        normalizedUrl = "https://image.lceda.cn/" + normalizedUrl;
    }

    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("http://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn/image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));

    return normalizedUrl;
}

QStringList deduplicateAndNormalizeUrls(const QStringList& imageUrls) {
    QStringList normalizedUrls;
    for (const QString& imageUrl : imageUrls) {
        const QString normalizedUrl = normalizePreviewImageUrl(imageUrl);
        if (!normalizedUrl.isEmpty() && !normalizedUrls.contains(normalizedUrl)) {
            normalizedUrls.append(normalizedUrl);
        }
    }
    return normalizedUrls;
}

bool isAllowedUrl(const QUrl& url, ResourceType resourceType, QString* errorMessage) {
    if (!url.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid URL: %1").arg(url.toString());
        }
        return false;
    }

    // 仅允许 https scheme
    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("https")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("URL scheme must be https, got: %1").arg(scheme);
        }
        return false;
    }

    const QString host = url.host().toLower();

    // Unknown 类型：仅允许已知域名（拒绝未知 host）
    if (resourceType == ResourceType::Unknown) {
        static const QSet<QString> allKnown =
            allowedHostsForType(ResourceType::ComponentInfo) + allowedHostsForType(ResourceType::ProductSearch) +
            allowedHostsForType(ResourceType::PreviewImage) + allowedHostsForType(ResourceType::Datasheet) +
            allowedHostsForType(ResourceType::Model3DObj) + allowedHostsForType(ResourceType::UpdateCheck) +
            allowedHostsForType(ResourceType::WorkerRequest);
        if (!isHostAllowed(host, allKnown)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("URL host '%1' is not in any known allowlist").arg(host);
            }
            return false;
        }
        return true;
    }
    const QSet<QString> allowed = allowedHostsForType(resourceType);

    if (allowed.isEmpty()) {
        // 空白名单 = 允许所有（向后兼容）
        return true;
    }

    if (!isHostAllowed(host, allowed)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("URL host '%1' is not allowed for resource type %2")
                                .arg(host)
                                .arg(static_cast<int>(resourceType));
        }
        return false;
    }

    return true;
}

}  // namespace EasyKiConverter::UrlUtils
