#ifndef COMPONENTDATA_H
#define COMPONENTDATA_H

#include "FootprintData.h"
#include "Model3DData.h"
#include "SymbolData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief 元件数据
     *
 * 包含元件的所有信息，包括符号、封装和 3D 模型
 */
class ComponentData {
public:
    ComponentData();
    ~ComponentData() = default;

    // Getter 和 Setter 方法
    QString lcscId() const {
        return m_lcscId;
    }

    void setLcscId(const QString& id) {
        m_lcscId = id;
    }

    QString name() const {
        return m_name;
    }

    void setName(const QString& name) {
        m_name = name;
    }

    QString prefix() const {
        return m_prefix;
    }

    void setPrefix(const QString& prefix) {
        m_prefix = prefix;
    }

    QString package() const {
        return m_package;
    }

    void setPackage(const QString& package) {
        m_package = package;
    }

    QString manufacturer() const {
        return m_manufacturer;
    }

    void setManufacturer(const QString& manufacturer) {
        m_manufacturer = manufacturer;
    }

    QString manufacturerPart() const {
        return m_manufacturerPart;
    }

    void setManufacturerPart(const QString& manufacturerPart) {
        m_manufacturerPart = manufacturerPart;
    }

    QString datasheet() const {
        return m_datasheet;
    }

    void setDatasheet(const QString& datasheet) {
        m_datasheet = datasheet;
    }

    QStringList previewImages() const {
        return m_previewImages;
    }

    void setPreviewImages(const QStringList& images) {
        m_previewImages = images;
    }

    void addPreviewImage(const QString& imageUrl) {
        if (!imageUrl.isEmpty() && !m_previewImages.contains(imageUrl)) {
            m_previewImages.append(imageUrl);
        }
    }

    QList<QByteArray> previewImageData() const {
        return m_previewImageData;
    }

    void setPreviewImageData(const QList<QByteArray>& data) {
        m_previewImageData = data;
    }

    void addPreviewImageData(const QByteArray& data, int imageIndex = -1) {
        qDebug() << "ComponentData::addPreviewImageData called - data size:" << data.size()
                 << "bytes, index:" << imageIndex << "current count:" << m_previewImageData.size();

        if (data.isEmpty()) {
            qDebug() << "  Skipping: data is empty";
            return;
        }

        // 如果指定了索引，检查该索引是否已经存在
        if (imageIndex >= 0 && imageIndex < m_previewImageData.size()) {
            // 如果该索引已经有数据，跳过（避免重复添加）
            if (!m_previewImageData[imageIndex].isEmpty()) {
                qDebug() << "  Skipping: index" << imageIndex << "already has data ("
                         << m_previewImageData[imageIndex].size() << "bytes)";
                return;
            }
            // 替换该索引的数据
            qDebug() << "  Replacing data at index:" << imageIndex;
            m_previewImageData[imageIndex] = data;
        } else {
            // 没有指定索引或索引超出范围，直接追加
            qDebug() << "  Appending data, new count:" << (m_previewImageData.size() + 1);
            m_previewImageData.append(data);
        }

        qDebug() << "  Final count:" << m_previewImageData.size();
    }

    QByteArray datasheetData() const {
        return m_datasheetData;
    }

    void setDatasheetData(const QByteArray& data) {
        m_datasheetData = data;
    }

    QString datasheetFormat() const {
        return m_datasheetFormat;
    }

    void setDatasheetFormat(const QString& format) {
        m_datasheetFormat = format;
    }

    QSharedPointer<SymbolData> symbolData() const {
        return m_symbolData;
    }

    void setSymbolData(const QSharedPointer<SymbolData>& data) {
        m_symbolData = data;
    }

    QSharedPointer<FootprintData> footprintData() const {
        return m_footprintData;
    }

    void setFootprintData(const QSharedPointer<FootprintData>& data) {
        m_footprintData = data;
    }

    QSharedPointer<Model3DData> model3DData() const {
        return m_model3DData;
    }

    void setModel3DData(const QSharedPointer<Model3DData>& data) {
        m_model3DData = data;
    }

    // Debug 导出用的原始数据访问器
    QByteArray cinfoJsonRaw() const {
        return m_cinfoJsonRaw;
    }

    void setCinfoJsonRaw(const QByteArray& data) {
        m_cinfoJsonRaw = data;
    }

    QByteArray cadJsonRaw() const {
        return m_cadJsonRaw;
    }

    void setCadJsonRaw(const QByteArray& data) {
        m_cadJsonRaw = data;
    }

    QByteArray model3DObjRaw() const {
        return m_model3DObjRaw;
    }

    void setModel3DObjRaw(const QByteArray& data) {
        m_model3DObjRaw = data;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    // 清空数据
    void clear();

private:
    QString m_lcscId;  // LCSC 元件编号
    QString m_name;  // 元件名称
    QString m_prefix;  // 元件前缀
    QString m_package;  // 封装名称
    QString m_manufacturer;  // 制造商
    QString m_manufacturerPart;  // 制造商部件号
    QString m_datasheet;  // 数据手册链接
    QByteArray m_datasheetData;  // 数据手册数据（内存）
    QString m_datasheetFormat;  // 数据手册格式（pdf/html）
    QStringList m_previewImages;  // 预览图 URL 列表
    QList<QByteArray> m_previewImageData;  // 预览图数据列表（内存）

    QSharedPointer<SymbolData> m_symbolData;  // 符号数据
    QSharedPointer<FootprintData> m_footprintData;  // 封装数据
    QSharedPointer<Model3DData> m_model3DData;  // 3D 模型数据

    // Debug 导出用的原始数据
    QByteArray m_cinfoJsonRaw;  // 元器件信息原始 JSON
    QByteArray m_cadJsonRaw;  // CAD 数据原始 JSON（包含符号和封装）
    QByteArray m_model3DObjRaw;  // 3D 模型 OBJ 原始数据
};

}  // namespace EasyKiConverter

#endif  // COMPONENTDATA_H
