#include "core/utils/UrlUtils.h"

#include <QString>
#include <QStringList>

namespace EasyKiConverter::UrlUtils {

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

}  // namespace EasyKiConverter::UrlUtils
