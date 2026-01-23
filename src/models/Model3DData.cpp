#include "Model3DData.h"
#include <QDebug>

namespace EasyKiConverter
{

    // ==================== Model3DBase ====================

    QJsonObject Model3DBase::toJson() const
    {
        QJsonObject json;
        json["x"] = x;
        json["y"] = y;
        json["z"] = z;
        return json;
    }

    bool Model3DBase::fromJson(const QJsonObject &json)
    {
        x = json["x"].toDouble();
        y = json["y"].toDouble();
        z = json["z"].toDouble();
        return true;
    }

    // ==================== Model3DData ====================

    Model3DData::Model3DData()
        : m_name(), m_uuid(), m_translation(), m_rotation(), m_rawObj(), m_step()
    {
    }

    QJsonObject Model3DData::toJson() const
    {
        QJsonObject json;

        json["name"] = m_name;
        json["uuid"] = m_uuid;
        json["translation"] = m_translation.toJson();
        json["rotation"] = m_rotation.toJson();
        json["raw_obj"] = m_rawObj;

        // STEP 数据�?Base64 编码存储
        if (!m_step.isEmpty())
        {
            json["step"] = QString::fromLatin1(m_step.toBase64());
        }

        return json;
    }

    bool Model3DData::fromJson(const QJsonObject &json)
    {
        m_name = json["name"].toString();
        m_uuid = json["uuid"].toString();

        if (json.contains("translation") && json["translation"].isObject())
        {
            if (!m_translation.fromJson(json["translation"].toObject()))
            {
                qWarning() << "Failed to parse 3D model translation";
                return false;
            }
        }

        if (json.contains("rotation") && json["rotation"].isObject())
        {
            if (!m_rotation.fromJson(json["rotation"].toObject()))
            {
                qWarning() << "Failed to parse 3D model rotation";
                return false;
            }
        }

        m_rawObj = json["raw_obj"].toString();

        // 解析 Base64 编码�?STEP 数据
        if (json.contains("step") && json["step"].isString())
        {
            QString stepBase64 = json["step"].toString();
            m_step = QByteArray::fromBase64(stepBase64.toLatin1());
        }

        return true;
    }

    bool Model3DData::isValid() const
    {
        // 至少要有名称�?UUID
        if (m_name.isEmpty() && m_uuid.isEmpty())
        {
            return false;
        }

        return true;
    }

    QString Model3DData::validate() const
    {
        if (m_name.isEmpty() && m_uuid.isEmpty())
        {
            return "3D model must have either name or UUID";
        }

        return QString(); // 返回空字符串表示验证通过
    }

    void Model3DData::clear()
    {
        m_name.clear();
        m_uuid.clear();
        m_translation = Model3DBase();
        m_rotation = Model3DBase();
        m_rawObj.clear();
        m_step.clear();
    }

} // namespace EasyKiConverter
