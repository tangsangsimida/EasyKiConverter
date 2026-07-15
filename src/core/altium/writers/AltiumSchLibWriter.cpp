#include "AltiumSchLibWriter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"
#include "utils/AltiumWriterUtils.h"

#include <QRandomGenerator>

namespace EasyKiConverter {

/**
 * @brief 获取元件的 Section Key（存储键）
 * @details 委托给 AltiumWriterUtils::getSectionKey()
 */
QString AltiumSchLibWriter::getSectionKey(const QString& name) const {
    return AltiumWriterUtils::getSectionKey(name);
}

/**
 * @brief 获取或添加字体到字体表
 */
int AltiumSchLibWriter::getOrAddFont(const QString& fontName, int fontSize,
                                      bool bold, bool italic, bool underline) {
    for (int i = 0; i < m_fonts.size(); ++i) {
        if (m_fonts[i].name == fontName && m_fonts[i].size == fontSize
            && m_fonts[i].bold == bold && m_fonts[i].italic == italic
            && m_fonts[i].underline == underline) {
            return i + 1;  // 1-based
        }
    }
    AltiumModels::FontEntry entry;
    entry.name = fontName;
    entry.size = fontSize;
    entry.bold = bold;
    entry.italic = italic;
    entry.underline = underline;
    m_fonts.append(entry);
    return m_fonts.size();  // 1-based
}

/**
 * @brief 添加坐标参数（DXP 单位 + 小数部分）
 */
void AltiumSchLibWriter::addCoordParam(QMap<QString, QString>& params,
                                        const QString& key, int raw) {
    int16_t dxp = AltiumCoord::toDxpInt(raw);
    int32_t frac = AltiumCoord::toDxpFrac(raw);
    params[key] = QString::number(dxp);
    if (frac != 0) {
        params[key + "_Frac"] = QString::number(frac);
    }
}

/**
 * @brief 添加颜色参数
 */
void AltiumSchLibWriter::addColorParam(QMap<QString, QString>& params,
                                        const QString& key, uint32_t color) {
    if (color != 0) {
        params[key] = QString::number(color);
    }
}

/**
 * @brief 生成并添加 UniqueID
 */
void AltiumSchLibWriter::addUniqueID(QMap<QString, QString>& params) {
    m_uniqueIdCounter++;
    QString id = QString("%1").arg(m_uniqueIdCounter, 8, 10, QChar('0'));
    params["UniqueID"] = id;
}

/**
 * @brief 写入 SchLib 文件
 */
bool AltiumSchLibWriter::write(const QList<AltiumSchComponent>& components,
                                const QString& filePath,
                                const QString& libraryName) {
    m_fonts.clear();
    m_uniqueIdCounter = 0;

    // 确保有默认字体
    getOrAddFont("Times New Roman", 10);

    OLECompoundWriter ole;
    if (!ole.create()) {
        return false;
    }

    // 写入 FileHeader
    writeFileHeader(ole, components);

    // 写入 SectionKeys（如果需要）
    writeSectionKeys(ole, components);

    // 写入每个元件的存储
    for (const AltiumSchComponent& component : components) {
        writeComponentStorage(ole, component);
    }

    // 写入空的 Storage 流（嵌入图像）
    ole.writeStream("Storage", QByteArray());

    return ole.saveToFile(filePath);
}

/**
 * @brief 写入 FileHeader 流
 */
void AltiumSchLibWriter::writeFileHeader(OLECompoundWriter& ole,
                                          const QList<AltiumSchComponent>& components) {
    QMap<QString, QString> params;
    params["HEADER"] = "Protel for Windows - Schematic Library Editor Binary File Version 5.0";
    params["WEIGHT"] = QString::number(components.size());
    params["MINORVERSION"] = "2";

    // 生成 8 字符随机 UniqueID
    QString uid;
    for (int i = 0; i < 8; ++i) {
        uid += QChar('A' + QRandomGenerator::global()->bounded(26));
    }
    params["UniqueID"] = uid;

    // 字体表
    params["FontIdCount"] = QString::number(m_fonts.size());
    for (int i = 0; i < m_fonts.size(); ++i) {
        int idx = i + 1;
        params[QString("FontName%1").arg(idx)] = m_fonts[i].name;
        params[QString("Size%1").arg(idx)] = QString::number(m_fonts[i].size);
        if (m_fonts[i].bold) params[QString("Bold%1").arg(idx)] = "T";
        if (m_fonts[i].italic) params[QString("Italic%1").arg(idx)] = "T";
        if (m_fonts[i].underline) params[QString("Underline%1").arg(idx)] = "T";
    }

    params["UseMBCS"] = "T";
    params["IsBOC"] = "T";
    params["SheetStyle"] = "9";
    params["BorderOn"] = "T";
    params["Display_Unit"] = "0";

    // 构建参数字符串
    QString paramStr;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        paramStr += "|" + it.key() + "=" + it.value();
    }
    paramStr += "|";

    // 序列化
    QByteArray headerData;
    AltiumBinaryWriter writer(headerData);

    // 写入参数块
    writer.writeCStringParameterBlockRaw(paramStr);

    // 写入元件数量
    writer.writeInt32(static_cast<int32_t>(components.size()));

    // 写入每个元件名称
    for (const AltiumSchComponent& comp : components) {
        writer.writeStringBlock(comp.name);
    }

    ole.writeStream("FileHeader", headerData);
}

/**
 * @brief 写入 SectionKeys 流
 */
void AltiumSchLibWriter::writeSectionKeys(OLECompoundWriter& ole,
                                           const QList<AltiumSchComponent>& components) {
    QMap<QString, QString> params;
    int keyCount = 0;

    for (int i = 0; i < components.size(); ++i) {
        QString sectionKey = getSectionKey(components[i].name);
        if (sectionKey != components[i].name) {
            params[QString("LibRef%1").arg(keyCount)] = components[i].name;
            params[QString("SectionKey%1").arg(keyCount)] = sectionKey;
            keyCount++;
        }
    }

    if (keyCount > 0) {
        params["KeyCount"] = QString::number(keyCount);
        QByteArray data;
        AltiumBinaryWriter writer(data);
        writer.writeCStringParameterBlock(params);
        ole.writeStream("SectionKeys", data);
    }
}

/**
 * @brief 写入元件存储
 */
void AltiumSchLibWriter::writeComponentStorage(OLECompoundWriter& ole,
                                                const AltiumSchComponent& component) {
    QString sectionKey = getSectionKey(component.name);

    // 创建存储区
    ole.addStorage(sectionKey);

    // 构建 Data 流
    QByteArray data;
    AltiumBinaryWriter writer(data);

    // 写入元件记录
    writeComponentRecord(writer, component);

    // 写入引脚
    for (const AltiumSchPin& pin : component.pins) {
        writePinRecord(writer, pin, 1);
    }

    // 写入矩形
    for (const AltiumSchRectangle& rect : component.rectangles) {
        writeRectangleRecord(writer, rect);
    }

    // 写入线段
    for (const AltiumSchLine& line : component.lines) {
        writeLineRecord(writer, line);
    }

    // 写入弧线
    for (const AltiumSchArc& arc : component.arcs) {
        writeArcRecord(writer, arc);
    }

    // 写入多边形
    for (const AltiumSchPolygon& polygon : component.polygons) {
        writePolygonRecord(writer, polygon);
    }

    // 写入椭圆
    for (const AltiumSchEllipse& ellipse : component.ellipses) {
        writeEllipseRecord(writer, ellipse);
    }

    // 写入折线
    for (const AltiumSchPolyline& polyline : component.polylines) {
        writePolylineRecord(writer, polyline);
    }

    // 写入文本
    for (const AltiumSchText& text : component.texts) {
        writeTextRecord(writer, text);
    }

    // 写入实现记录
    writeImplementationRecords(writer, component);

    // 写入流
    ole.writeStream(sectionKey, "Data", data);
}

/**
 * @brief 写入元件记录 (RECORD=1)
 */
void AltiumSchLibWriter::writeComponentRecord(AltiumBinaryWriter& writer,
                                               const AltiumSchComponent& component) {
    QMap<QString, QString> params;
    params["RECORD"] = "1";
    params["LibReference"] = component.name;

    if (!component.description.isEmpty()) {
        params["ComponentDescription"] = component.description;
    }

    params["PartCount"] = QString::number(component.partCount + 1);
    params["DisplayModeCount"] = "1";
    params["IndexInSheet"] = "-1";
    params["OwnerPartId"] = "-1";
    params["CurrentPartId"] = "1";
    params["LibraryPath"] = "*";
    params["SourceLibraryName"] = "*";
    params["SheetPartFileName"] = "*";
    params["TargetFileName"] = "*";

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入引脚记录 (RECORD=2, 二进制格式)
 */
void AltiumSchLibWriter::writePinRecord(AltiumBinaryWriter& writer,
                                         const AltiumSchPin& pin, int partId) {
    writer.beginBlock(0x01);  // flags = 0x01 表示二进制引脚记录

    writer.writeInt32(2);  // Record type = 2
    writer.writeUInt8(0);  // Unknown
    writer.writeInt16(static_cast<int16_t>(partId));  // OwnerPartId
    writer.writeUInt8(0);  // OwnerPartDisplayMode

    // Symbol edges
    writer.writeUInt8(0);  // SymbolInnerEdge
    writer.writeUInt8(0);  // SymbolOuterEdge
    writer.writeUInt8(0);  // SymbolInside
    writer.writeUInt8(0);  // SymbolOutside

    // Description (空 Pascal 短字符串)
    writer.writePascalShortString("");

    writer.writeUInt8(0);  // FormalType
    writer.writeUInt8(static_cast<uint8_t>(pin.electricalType));  // ElectricalType

    // PinConglomerate 字节
    uint8_t conglomerate = static_cast<uint8_t>(pin.orientation);  // Bit 0-1: orientation
    if (pin.isHidden) conglomerate |= 0x04;          // Bit 2: hidden
    if (pin.showName) conglomerate |= 0x08;          // Bit 3: show name
    if (pin.showDesignator) conglomerate |= 0x10;    // Bit 4: show designator
    writer.writeUInt8(conglomerate);

    // PinLength (DXP 整数单位)
    int16_t lengthDxp = AltiumCoord::toDxpInt(pin.length);
    writer.writeInt16(lengthDxp);

    // Location (DXP 整数单位)
    writer.writeInt16(AltiumCoord::toDxpInt(pin.locationX));
    writer.writeInt16(AltiumCoord::toDxpInt(pin.locationY));

    // Color
    writer.writeUInt32(pin.color);

    // Name (Pascal 短字符串)
    writer.writePascalShortString(pin.name);

    // Designator (Pascal 短字符串)
    writer.writePascalShortString(pin.designator);

    // SwapIdGroup (空 Pascal 短字符串)
    writer.writePascalShortString("");

    // PartAndSequence (空 Pascal 短字符串)
    writer.writePascalShortString("");

    // DefaultValue (空 Pascal 短字符串)
    writer.writePascalShortString("");

    writer.endBlock();
}

/**
 * @brief 写入矩形记录 (RECORD=14)
 */
void AltiumSchLibWriter::writeRectangleRecord(AltiumBinaryWriter& writer,
                                               const AltiumSchRectangle& rect) {
    QMap<QString, QString> params;
    params["RECORD"] = "14";
    addCoordParam(params, "Location.X", rect.locationX);
    addCoordParam(params, "Location.Y", rect.locationY);
    addCoordParam(params, "Corner.X", rect.cornerX);
    addCoordParam(params, "Corner.Y", rect.cornerY);

    if (rect.lineWidth != 0) params["LineWidth"] = QString::number(rect.lineWidth);
    addColorParam(params, "Color", rect.color);
    if (rect.areaColor != 0xFFFFFF) params["AreaColor"] = QString::number(rect.areaColor);
    if (rect.isSolid) params["IsSolid"] = "T";

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入线段记录 (RECORD=13)
 */
void AltiumSchLibWriter::writeLineRecord(AltiumBinaryWriter& writer,
                                          const AltiumSchLine& line) {
    QMap<QString, QString> params;
    params["RECORD"] = "13";
    addCoordParam(params, "Location.X", line.locationX);
    addCoordParam(params, "Location.Y", line.locationY);
    addCoordParam(params, "Corner.X", line.cornerX);
    addCoordParam(params, "Corner.Y", line.cornerY);

    params["LineWidth"] = QString::number(AltiumCoord::lineWidthToIndex(line.lineWidth));
    addColorParam(params, "Color", line.color);

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入弧线记录 (RECORD=12)
 */
void AltiumSchLibWriter::writeArcRecord(AltiumBinaryWriter& writer,
                                         const AltiumSchArc& arc) {
    QMap<QString, QString> params;
    params["RECORD"] = "12";
    addCoordParam(params, "Location.X", arc.centerX);
    addCoordParam(params, "Location.Y", arc.centerY);
    addCoordParam(params, "Radius", arc.radius);

    if (arc.lineWidth != 0) params["LineWidth"] = QString::number(arc.lineWidth);
    if (arc.startAngle != 0.0) params["StartAngle"] = QString::number(arc.startAngle, 'f', 3);
    params["EndAngle"] = QString::number(arc.endAngle, 'f', 3);
    addColorParam(params, "Color", arc.color);

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入多边形记录 (RECORD=7)
 */
void AltiumSchLibWriter::writePolygonRecord(AltiumBinaryWriter& writer,
                                             const AltiumSchPolygon& polygon) {
    QMap<QString, QString> params;
    params["RECORD"] = "7";
    params["LineWidth"] = QString::number(polygon.lineWidth);
    addColorParam(params, "Color", polygon.color);
    if (polygon.areaColor != 0xFFFFFF) params["AreaColor"] = QString::number(polygon.areaColor);
    if (polygon.isSolid) params["IsSolid"] = "T";

    params["LocationCount"] = QString::number(polygon.vertices.size());
    for (int i = 0; i < polygon.vertices.size(); ++i) {
        int idx = i + 1;
        int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(polygon.vertices[i].x()));
        int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(polygon.vertices[i].y()));
        if (x != 0) params[QString("X%1").arg(idx)] = QString::number(x);
        if (y != 0) params[QString("Y%1").arg(idx)] = QString::number(y);
    }

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入椭圆记录 (RECORD=8)
 */
void AltiumSchLibWriter::writeEllipseRecord(AltiumBinaryWriter& writer,
                                             const AltiumSchEllipse& ellipse) {
    QMap<QString, QString> params;
    params["RECORD"] = "8";
    addCoordParam(params, "Location.X", ellipse.centerX);
    addCoordParam(params, "Location.Y", ellipse.centerY);
    addCoordParam(params, "Radius", ellipse.radiusX);
    addCoordParam(params, "SecondaryRadius", ellipse.radiusY);

    if (ellipse.lineWidth != 0) params["LineWidth"] = QString::number(ellipse.lineWidth);
    addColorParam(params, "Color", ellipse.color);
    if (ellipse.areaColor != 0xFFFFFF) params["AreaColor"] = QString::number(ellipse.areaColor);
    if (ellipse.isSolid) params["IsSolid"] = "T";

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入折线记录 (RECORD=6)
 */
void AltiumSchLibWriter::writePolylineRecord(AltiumBinaryWriter& writer,
                                              const AltiumSchPolyline& polyline) {
    QMap<QString, QString> params;
    params["RECORD"] = "6";
    params["LineWidth"] = QString::number(polyline.lineWidth);
    addColorParam(params, "Color", polyline.color);

    params["LocationCount"] = QString::number(polyline.vertices.size());
    for (int i = 0; i < polyline.vertices.size(); ++i) {
        int idx = i + 1;
        int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(polyline.vertices[i].x()));
        int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(polyline.vertices[i].y()));
        if (x != 0) params[QString("X%1").arg(idx)] = QString::number(x);
        if (y != 0) params[QString("Y%1").arg(idx)] = QString::number(y);
    }

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入文本记录 (RECORD=4, Label)
 */
void AltiumSchLibWriter::writeTextRecord(AltiumBinaryWriter& writer,
                                          const AltiumSchText& text) {
    QMap<QString, QString> params;
    params["RECORD"] = "4";
    addCoordParam(params, "Location.X", text.locationX);
    addCoordParam(params, "Location.Y", text.locationY);

    if (text.orientation != 0) params["Orientation"] = QString::number(text.orientation);
    addColorParam(params, "Color", text.color);
    params["FontID"] = QString::number(text.fontId);
    params["Text"] = text.text;
    if (text.isHidden) params["IsHidden"] = "T";

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入实现记录 (RECORD=44-48)
 */
void AltiumSchLibWriter::writeImplementationRecords(AltiumBinaryWriter& writer,
                                                     const AltiumSchComponent& component) {
    // RECORD=44: ImplementationList（容器，始终写入）
    {
        QMap<QString, QString> params;
        params["RECORD"] = "44";
        params["DataFileFormatID"] = "";
        params["Description"] = "";
        params["FileName"] = "";
        writer.writeCStringParameterBlock(params);
    }

    // 每个实现
    for (const AltiumSchComponent::Implementation& impl : component.implementations) {
        // RECORD=45: Implementation
        {
            QMap<QString, QString> params;
            params["RECORD"] = "45";
            params["DESCRIPTION"] = impl.modelName;
            params["MODELNAME"] = impl.modelName;
            params["MODELTYPE"] = impl.modelType;
            params["DATAFILECOUNT"] = "1";
            params["MODELDATAFILEKIND1"] = "PCB";
            params["MODELDATAFILEENTITY1"] = "1";
            params["ISCURRENT"] = "T";
            writer.writeCStringParameterBlock(params);
        }

        // RECORD=46: MapDefinerList（容器）
        {
            QMap<QString, QString> params;
            params["RECORD"] = "46";
            writer.writeCStringParameterBlock(params);
        }

        // RECORD=48: ImplementationParameters（空容器）
        {
            QMap<QString, QString> params;
            params["RECORD"] = "48";
            writer.writeCStringParameterBlock(params);
        }
    }

    // 如果没有实现，仍然写入空的容器记录
    if (component.implementations.isEmpty()) {
        QMap<QString, QString> params;
        params["RECORD"] = "46";
        writer.writeCStringParameterBlock(params);

        params.clear();
        params["RECORD"] = "48";
        writer.writeCStringParameterBlock(params);
    }
}

}  // namespace EasyKiConverter
