#include "ComponentData.h"
#include <QJsonDocument>
#include <QDebug>

namespace EasyKiConverter
{

    ComponentData::ComponentData()
        : m_lcscId(), m_name(), m_prefix(), m_package(), m_manufacturer(), m_datasheet(), m_symbolData(nullptr), m_footprintData(nullptr), m_model3DData(nullptr)
    {
    }

    QJsonObject ComponentData::toJson() const
    {
        QJsonObject json;

        // 基本信息
        json["lcsc_id"] = m_lcscId;
        json["name"] = m_name;
        json["prefix"] = m_prefix;
        json["package"] = m_package;
        json["manufacturer"] = m_manufacturer;
        json["datasheet"] = m_datasheet;

        // 符号数据
        if (m_symbolData)
        {
            json["symbol"] = m_symbolData->toJson();
        }

        // 封装数据
        if (m_footprintData)
        {
            json["footprint"] = m_footprintData->toJson();
        }

        // 3D 模型数据
        if (m_model3DData)
        {
            json["model_3d"] = m_model3DData->toJson();
        }

        return json;
    }

    bool ComponentData::fromJson(const QJsonObject &json)
    {
        // 读取基本信息
        m_lcscId = json["lcsc_id"].toString();
        m_name = json["name"].toString();
        m_prefix = json["prefix"].toString();
        m_package = json["package"].toString();
        m_manufacturer = json["manufacturer"].toString();
        m_datasheet = json["datasheet"].toString();

        // 读取符号数据
        if (json.contains("symbol") && json["symbol"].isObject())
        {
            if (!m_symbolData)
            {
                m_symbolData = QSharedPointer<SymbolData>::create();
            }
            if (!m_symbolData->fromJson(json["symbol"].toObject()))
            {
                qWarning() << "Failed to parse symbol data";
                return false;
            }
        }

        // 读取封装数据
        if (json.contains("footprint") && json["footprint"].isObject())
        {
            if (!m_footprintData)
            {
                m_footprintData = QSharedPointer<FootprintData>::create();
            }
            if (!m_footprintData->fromJson(json["footprint"].toObject()))
            {
                qWarning() << "Failed to parse footprint data";
                return false;
            }
        }

        // 读取 3D 模型数据
        if (json.contains("model_3d") && json["model_3d"].isObject())
        {
            if (!m_model3DData)
            {
                m_model3DData = QSharedPointer<Model3DData>::create();
            }
            if (!m_model3DData->fromJson(json["model_3d"].toObject()))
            {
                qWarning() << "Failed to parse 3D model data";
                return false;
            }
        }

        return true;
    }

    bool ComponentData::isValid() const
    {
        // 验证 LCSC ID 格式（以 C 开头，后面跟数字）
        if (m_lcscId.isEmpty())
        {
            return false;
        }

        if (!m_lcscId.startsWith('C', Qt::CaseInsensitive))
        {
            return false;
        }

        bool ok;
        m_lcscId.mid(1).toInt(&ok);
        if (!ok && m_lcscId.length() > 1)
        {
            return false;
        }

        // 至少要有符号或封装数�?
        if (!m_symbolData && !m_footprintData)
        {
            return false;
        }

        return true;
    }

    QString ComponentData::validate() const
    {
        if (m_lcscId.isEmpty())
        {
            return "LCSC ID is empty";
        }

        if (!m_lcscId.startsWith('C', Qt::CaseInsensitive))
        {
            return QString("LCSC ID must start with 'C': %1").arg(m_lcscId);
        }

        bool ok;
        m_lcscId.mid(1).toInt(&ok);
        if (!ok && m_lcscId.length() > 1)
        {
            return QString("LCSC ID must be in format 'C' followed by numbers: %1").arg(m_lcscId);
        }

        if (!m_symbolData && !m_footprintData)
        {
            return "Component must have at least symbol or footprint data";
        }

        if (m_symbolData)
        {
            QString symbolError = m_symbolData->validate();
            if (!symbolError.isEmpty())
            {
                return QString("Symbol validation failed: %1").arg(symbolError);
            }
        }

        if (m_footprintData)
        {
            QString footprintError = m_footprintData->validate();
            if (!footprintError.isEmpty())
            {
                return QString("Footprint validation failed: %1").arg(footprintError);
            }
        }

        return QString(); // 返回空字符串表示验证通过
    }

    void ComponentData::clear()
    {
        m_lcscId.clear();
        m_name.clear();
        m_prefix.clear();
        m_package.clear();
        m_manufacturer.clear();
        m_datasheet.clear();

        m_symbolData.reset();
        m_footprintData.reset();
        m_model3DData.reset();
    }

} // namespace EasyKiConverter
