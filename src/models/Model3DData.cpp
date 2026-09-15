#include "Model3DData.h"

#include <QDebug>

namespace EasyKiConverter {

// ==================== Model3DBase ====================

QJsonObject Model3DBase::toJson() const {
    QJsonObject json;
    json["x"] = x;
    json["y"] = y;
    json["z"] = z;
    return json;
}

bool Model3DBase::fromJson(const QJsonObject& json) {
    const auto readCoordinate = [&json](const QString& name, double current, double* value) {
        if (!json.contains(name)) {
            *value = current;
            return true;
        }
        const QJsonValue jsonValue = json.value(name);
        if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble()))
            return false;
        *value = jsonValue.toDouble();
        return true;
    };
    double parsedX = x;
    double parsedY = y;
    double parsedZ = z;
    if (!readCoordinate(QStringLiteral("x"), x, &parsedX) || !readCoordinate(QStringLiteral("y"), y, &parsedY) ||
        !readCoordinate(QStringLiteral("z"), z, &parsedZ))
        return false;
    x = parsedX;
    y = parsedY;
    z = parsedZ;
    return true;
}

// ==================== Model3DData ====================

Model3DData::Model3DData()
    : m_name(), m_uuid(), m_translation(), m_rotation(), m_stepOffsetMm(), m_rawObj(), m_step() {}

QJsonObject Model3DData::toJson() const {
    QJsonObject json;

    json["name"] = m_name;
    json["uuid"] = m_uuid;
    json["translation"] = m_translation.toJson();
    json["rotation"] = m_rotation.toJson();
    json["step_offset_mm"] = m_stepOffsetMm.toJson();
    json["raw_obj"] = m_rawObj;

    // STEP 数据Base64 编码存储
    if (!m_step.isEmpty()) {
        json["step"] = QString::fromLatin1(m_step.toBase64());
    }

    return json;
}

bool Model3DData::fromJson(const QJsonObject& json) {
    QString parsedName = m_name;
    QString parsedUuid = m_uuid;
    QString parsedRawObj = m_rawObj;
    Model3DBase parsedTranslation = m_translation;
    Model3DBase parsedRotation = m_rotation;
    Model3DBase parsedStepOffsetMm = m_stepOffsetMm;
    QByteArray parsedStep = m_step;

    const auto readOptionalString = [&json](const QString& name, QString* value) {
        if (!json.contains(name))
            return true;
        const QJsonValue jsonValue = json.value(name);
        if (!jsonValue.isString())
            return false;
        *value = jsonValue.toString();
        return true;
    };
    if (!readOptionalString(QStringLiteral("name"), &parsedName) ||
        !readOptionalString(QStringLiteral("uuid"), &parsedUuid) ||
        !readOptionalString(QStringLiteral("raw_obj"), &parsedRawObj))
        return false;

    const auto readOptionalVector = [&json](const QString& name, Model3DBase* value) {
        if (!json.contains(name))
            return true;
        const QJsonValue jsonValue = json.value(name);
        if (!jsonValue.isObject())
            return false;
        return value->fromJson(jsonValue.toObject());
    };
    if (!readOptionalVector(QStringLiteral("translation"), &parsedTranslation)) {
        qWarning() << "Failed to parse 3D model translation";
        return false;
    }

    if (!readOptionalVector(QStringLiteral("rotation"), &parsedRotation)) {
        qWarning() << "Failed to parse 3D model rotation";
        return false;
    }

    if (!readOptionalVector(QStringLiteral("step_offset_mm"), &parsedStepOffsetMm)) {
        qWarning() << "Failed to parse 3D model STEP offset";
        return false;
    }

    // 解析 Base64 编码STEP 数据
    if (json.contains(QStringLiteral("step"))) {
        const QJsonValue jsonValue = json.value(QStringLiteral("step"));
        if (!jsonValue.isString())
            return false;
        parsedStep = QByteArray::fromBase64(jsonValue.toString().toLatin1());
    }

    m_name = parsedName;
    m_uuid = parsedUuid;
    m_rawObj = parsedRawObj;
    m_translation = parsedTranslation;
    m_rotation = parsedRotation;
    m_stepOffsetMm = parsedStepOffsetMm;
    m_step = parsedStep;

    return true;
}

bool Model3DData::isValid() const {
    // 至少要有名称UUID
    if (m_name.isEmpty() && m_uuid.isEmpty()) {
        return false;
    }

    return true;
}

QString Model3DData::validate() const {
    if (m_name.isEmpty() && m_uuid.isEmpty()) {
        return "3D model must have either name or UUID";
    }

    return QString();  // 返回空字符串表示验证通过
}

void Model3DData::clear() {
    m_name.clear();
    m_uuid.clear();
    m_translation = Model3DBase();
    m_rotation = Model3DBase();
    m_stepOffsetMm = Model3DBase();
    m_rawObj.clear();
    m_step.clear();
}

}  // namespace EasyKiConverter
