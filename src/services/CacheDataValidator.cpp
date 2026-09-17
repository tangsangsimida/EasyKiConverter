#include "CacheDataValidator.h"

#include "core/kicad/Exporter3DModel.h"

#include <QImage>
#include <QJsonDocument>
#include <QJsonParseError>

namespace EasyKiConverter {

// 校验 CAD 原始缓存是否为合法 JSON 对象。
bool CacheDataValidator::isValidCadData(const QByteArray& data) {
    if (data.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject();
}

// 使用 Qt 图片解码器校验预览图内容。
bool CacheDataValidator::isValidPreviewImage(const QByteArray& data) {
    if (data.isEmpty()) {
        return false;
    }

    QImage image;
    return image.loadFromData(data);
}

// 根据声明格式校验 PDF 签名或 HTML 文档标记。
bool CacheDataValidator::isValidDatasheet(const QByteArray& data, const QString& format) {
    if (data.isEmpty()) {
        return false;
    }
    if (format.compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0) {
        return data.startsWith("%PDF-");
    }

    const QByteArray normalized = data.toLower();
    return normalized.contains("<html") || normalized.contains("<!doctype html") || normalized.contains("<body");
}

// 按模型扩展名调用对应的几何内容校验器。
bool CacheDataValidator::isUsableModel3D(const QByteArray& data, const QString& extension) {
    const QString normalizedExtension = extension.toLower();
    if (normalizedExtension == QStringLiteral("obj")) {
        return Exporter3DModel::hasUsableObjGeometry(data);
    }
    if (normalizedExtension == QStringLiteral("wrl")) {
        return Exporter3DModel::hasUsableWrlGeometry(data);
    }
    if (normalizedExtension == QStringLiteral("step")) {
        return Exporter3DModel::hasUsableStepData(data);
    }
    return false;
}

}  // namespace EasyKiConverter
