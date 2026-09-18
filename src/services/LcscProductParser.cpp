#include "LcscProductParser.h"

#include "core/utils/UrlUtils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter {

namespace {

/** @brief 从产品对象中提取并规范化最多三张预览图地址。 */
QStringList extractPreviewImageUrls(const QJsonObject& product) {
    QStringList imageUrls;
    const QString imagesString = product.value(QStringLiteral("image")).toString();
    if (!imagesString.isEmpty()) {
        imageUrls = imagesString.split(QStringLiteral("<$>"), Qt::SkipEmptyParts);
    }

    while (imageUrls.size() > 3) {
        imageUrls.removeLast();
    }
    return UrlUtils::deduplicateAndNormalizeUrls(imageUrls);
}

/** @brief 从产品、设备信息和属性中提取元件编号。 */
QString extractComponentCode(const QJsonObject& product) {
    static const QStringList directKeys = {QStringLiteral("component_code"),
                                           QStringLiteral("productCode"),
                                           QStringLiteral("product_code"),
                                           QStringLiteral("productNo"),
                                           QStringLiteral("product_no"),
                                           QStringLiteral("number"),
                                           QStringLiteral("code"),
                                           QStringLiteral("lcscPart"),
                                           QStringLiteral("lcscPartNumber")};

    const auto findValue = [](const QJsonObject& object) {
        for (const QString& key : directKeys) {
            const QString value = object.value(key).toString().trimmed();
            if (!value.isEmpty()) {
                return value.toUpper();
            }
        }
        return QString();
    };

    const QString directCode = findValue(product);
    if (!directCode.isEmpty()) {
        return directCode;
    }

    const QJsonObject deviceInfo = product.value(QStringLiteral("device_info")).toObject();
    const QString deviceCode = findValue(deviceInfo);
    if (!deviceCode.isEmpty()) {
        return deviceCode;
    }

    const QJsonObject attributes = deviceInfo.value(QStringLiteral("attributes")).toObject();
    static const QStringList attributeKeys = {QStringLiteral("LCSC Part"),
                                              QStringLiteral("LCSC Part #"),
                                              QStringLiteral("LCSC Part Number"),
                                              QStringLiteral("Part Number")};
    for (const QString& key : attributeKeys) {
        const QString value = attributes.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value.toUpper();
        }
    }
    return QString();
}

/** @brief 在搜索结果中选择与请求编号精确匹配的产品。 */
QJsonObject selectProduct(const QString& componentId, const QJsonArray& products) {
    const QString normalizedId = componentId.trimmed().toUpper();
    for (const QJsonValue& value : products) {
        const QJsonObject product = value.toObject();
        if (extractComponentCode(product) == normalizedId) {
            return product;
        }
    }
    return QJsonObject();
}

}  // namespace

/** @brief 解析 JSON、精确匹配产品并提取制造商与数据手册字段。 */
std::optional<LcscProductInfo> LcscProductParser::parse(const QString& componentId, const QByteArray& responseJson) {
    const QJsonDocument document = QJsonDocument::fromJson(responseJson);
    if (document.isNull()) {
        return std::nullopt;
    }

    const QJsonObject result = document.object().value(QStringLiteral("result")).toObject();
    const QJsonArray products = result.value(QStringLiteral("productList")).toArray();
    if (products.isEmpty()) {
        return std::nullopt;
    }

    const QJsonObject product = selectProduct(componentId, products);
    if (product.isEmpty()) {
        return std::nullopt;
    }

    LcscProductInfo info;
    info.imageUrls = extractPreviewImageUrls(product);
    const QJsonObject attributes =
        product.value(QStringLiteral("device_info")).toObject().value(QStringLiteral("attributes")).toObject();
    info.manufacturerPart = attributes.value(QStringLiteral("Manufacturer Part")).toString();
    info.datasheetUrl = attributes.value(QStringLiteral("Datasheet")).toString();
    return info;
}

}  // namespace EasyKiConverter
