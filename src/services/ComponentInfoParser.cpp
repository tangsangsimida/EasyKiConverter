#include "ComponentInfoParser.h"

namespace EasyKiConverter {

/** @brief 从 API 的 result 对象读取组件基础字段。 */
ComponentData ComponentInfoParser::parse(const QString& componentId, const QJsonObject& response) {
    ComponentData componentData;
    componentData.setLcscId(componentId);

    if (!response.contains(QStringLiteral("result")))
        return componentData;

    const QJsonObject result = response.value(QStringLiteral("result")).toObject();
    if (result.contains(QStringLiteral("title")))
        componentData.setName(result.value(QStringLiteral("title")).toString());
    if (result.contains(QStringLiteral("package")))
        componentData.setPackage(result.value(QStringLiteral("package")).toString());
    if (result.contains(QStringLiteral("manufacturer")))
        componentData.setManufacturer(result.value(QStringLiteral("manufacturer")).toString());
    if (result.contains(QStringLiteral("datasheet")))
        componentData.setDatasheet(result.value(QStringLiteral("datasheet")).toString());

    return componentData;
}

}  // namespace EasyKiConverter
