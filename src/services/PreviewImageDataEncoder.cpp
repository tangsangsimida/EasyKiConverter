#include "PreviewImageDataEncoder.h"

#include <QFile>
#include <QRegularExpression>

namespace EasyKiConverter {

/** @brief 从文件名中提取预览图序号。 */
int PreviewImageDataEncoder::imageIndexFromPath(const QString& path) {
    static const QRegularExpression re(QStringLiteral("preview_(\\d+)\\.jpg$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(path);
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok = false;
    const int index = match.captured(1).toInt(&ok);
    return ok ? index : -1;
}

/** @brief 读取预览图文件并同时生成 Base64 和原始数据。 */
PreviewImageDataResult PreviewImageDataEncoder::encodeFiles(const QStringList& imagePaths) {
    PreviewImageDataResult result;
    result.encodedImages.fill(QString(), 3);
    result.imageData.resize(3);

    for (const QString& path : imagePaths) {
        const int imageIndex = imageIndexFromPath(path);
        if (imageIndex < 0 || imageIndex >= result.encodedImages.size()) {
            continue;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QByteArray data = file.readAll();
        result.encodedImages[imageIndex] = QString::fromLatin1(data.toBase64());
        result.imageData[imageIndex] = data;
    }

    return result;
}

}  // namespace EasyKiConverter
