#ifndef COMPONENTDATA_H
#define COMPONENTDATA_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QSharedPointer>
#include "SymbolData.h"
#include "FootprintData.h"
#include "Model3DData.h"

namespace EasyKiConverter
{

    /**
     * @brief 元件数据�?
     *
     * 包含元件的所有信息，包括符号、封装和 3D 模型
     */
    class ComponentData
    {
    public:
        ComponentData();
        ~ComponentData() = default;

        // Getter �?Setter 方法
        QString lcscId() const { return m_lcscId; }
        void setLcscId(const QString &id) { m_lcscId = id; }

        QString name() const { return m_name; }
        void setName(const QString &name) { m_name = name; }

        QString prefix() const { return m_prefix; }
        void setPrefix(const QString &prefix) { m_prefix = prefix; }

        QString package() const { return m_package; }
        void setPackage(const QString &package) { m_package = package; }

        QString manufacturer() const { return m_manufacturer; }
        void setManufacturer(const QString &manufacturer) { m_manufacturer = manufacturer; }

        QString datasheet() const { return m_datasheet; }
        void setDatasheet(const QString &datasheet) { m_datasheet = datasheet; }

        QSharedPointer<SymbolData> symbolData() const { return m_symbolData; }
        void setSymbolData(const QSharedPointer<SymbolData> &data) { m_symbolData = data; }

        QSharedPointer<FootprintData> footprintData() const { return m_footprintData; }
        void setFootprintData(const QSharedPointer<FootprintData> &data) { m_footprintData = data; }

        QSharedPointer<Model3DData> model3DData() const { return m_model3DData; }
        void setModel3DData(const QSharedPointer<Model3DData> &data) { m_model3DData = data; }

        // JSON 序列�?
        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);

        // 数据验证
        bool isValid() const;
        QString validate() const;

        // 清空数据
        void clear();

    private:
        QString m_lcscId;       // LCSC 元件编号
        QString m_name;         // 元件名称
        QString m_prefix;       // 元件前缀
        QString m_package;      // 封装名称
        QString m_manufacturer; // 制造商
        QString m_datasheet;    // 数据手册链接

        QSharedPointer<SymbolData> m_symbolData;       // 符号数据
        QSharedPointer<FootprintData> m_footprintData; // 封装数据
        QSharedPointer<Model3DData> m_model3DData;     // 3D 模型数据
    };

} // namespace EasyKiConverter

#endif // COMPONENTDATA_H
