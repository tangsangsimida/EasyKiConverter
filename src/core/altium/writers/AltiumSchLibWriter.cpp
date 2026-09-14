#include "AltiumSchLibWriter.h"

#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumWriterUtils.h"

#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QIODevice>
#include <QRandomGenerator>
#include <QSet>

namespace EasyKiConverter {

namespace {

struct ParameterField {
    QString name;
    QString value;
    int locationX = 0;
    int locationY = 0;
    int fontId = 1;
    uint32_t color = 0x000000;
    bool hasLocation = false;
    bool hasVisibility = false;
    bool isHidden = true;
    bool readOnly = false;
    int orientation = 0;
    int ownerPartId = -1;
};

/**
 * @brief 将来源元数据转换为 Altium 可编辑的参数字段。
 * @details Comment 是 Altium 的 Value 字段；其余字段使用 RECORD=41，
 *          保留未知键以支持新的供应商数据而无需修改协议模型。
 */
QList<ParameterField> componentParameterFields(const AltiumSchComponent& component) {
    static const QMap<QString, QString> knownNames = {
        {QStringLiteral("description"), QStringLiteral("Description")},
        {QStringLiteral("manufacturer"), QStringLiteral("Manufacturer")},
        {QStringLiteral("manufacturerPart"), QStringLiteral("Manufacturer Part Number")},
        {QStringLiteral("datasheet"), QStringLiteral("Datasheet")},
        {QStringLiteral("lcscId"), QStringLiteral("LCSC Part")},
        {QStringLiteral("jlcId"), QStringLiteral("JLCPCB Part")},
        {QStringLiteral("jlcpcbPartClass"), QStringLiteral("JLCPCB Part Class")},
        {QStringLiteral("supplierPart"), QStringLiteral("Supplier Part Number")},
        {QStringLiteral("supplier"), QStringLiteral("Supplier")},
        {QStringLiteral("category"), QStringLiteral("Category")},
        {QStringLiteral("source"), QStringLiteral("Source")},
    };

    QList<ParameterField> fields;
    QSet<QString> names;
    const auto appendField = [&](const QString& name,
                                 const QString& value,
                                 bool hasLocation = false,
                                 int locationX = 0,
                                 int locationY = 0,
                                 bool hasVisibility = false,
                                 bool isHidden = true,
                                 bool readOnly = false,
                                 int orientation = 0,
                                 int ownerPartId = -1,
                                 int fontId = 1,
                                 uint32_t color = 0x000000) {
        if (name.trimmed().isEmpty() || value.trimmed().isEmpty() || names.contains(name))
            return;
        ParameterField field;
        field.name = name.trimmed();
        field.value = value.trimmed();
        field.hasLocation = hasLocation;
        field.locationX = locationX;
        field.locationY = locationY;
        field.hasVisibility = hasVisibility;
        field.isHidden = isHidden;
        field.readOnly = readOnly;
        field.orientation = orientation;
        field.ownerPartId = ownerPartId;
        field.fontId = fontId;
        field.color = color;
        fields.append(field);
        names.insert(field.name);
    };

    QString value = component.sourceMetadata.value(QStringLiteral("value")).trimmed();
    if (value.isEmpty())
        value = component.name.trimmed();
    if (!value.isEmpty()) {
        appendField(QStringLiteral("Comment"), value);
    }

    QString description = component.sourceMetadata.value(QStringLiteral("description")).trimmed();
    if (description.isEmpty())
        description = component.description.trimmed();
    if (!description.isEmpty()) {
        appendField(QStringLiteral("Description"), description);
    }

    for (auto it = component.sourceMetadata.constBegin(); it != component.sourceMetadata.constEnd(); ++it) {
        const QString value = it.value().trimmed();
        if (value.isEmpty() || it.key() == QStringLiteral("value") || it.key() == QStringLiteral("description"))
            continue;

        const QString name = knownNames.value(it.key(), it.key()).trimmed();
        appendField(name, value);
    }

    if (!component.aliases.isEmpty())
        appendField(QStringLiteral("Aliases"), component.aliases.join(QStringLiteral(", ")));

    for (const AltiumSchParameter& parameter : component.parameters) {
        QString name = parameter.name.trimmed();
        if (name.compare(QStringLiteral("Value"), Qt::CaseInsensitive) == 0)
            name = QStringLiteral("Comment");
        appendField(name,
                    parameter.value,
                    parameter.locationX != 0 || parameter.locationY != 0,
                    parameter.locationX,
                    parameter.locationY,
                    true,
                    parameter.isHidden,
                    parameter.readOnly,
                    parameter.orientation,
                    parameter.ownerPartId,
                    parameter.fontId,
                    parameter.color);
    }
    return fields;
}

}  // namespace

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
int AltiumSchLibWriter::getOrAddFont(const QString& fontName, int fontSize, bool bold, bool italic, bool underline) {
    for (int i = 0; i < m_fonts.size(); ++i) {
        if (m_fonts[i].name == fontName && m_fonts[i].size == fontSize && m_fonts[i].bold == bold &&
            m_fonts[i].italic == italic && m_fonts[i].underline == underline) {
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
 * @brief 预注册符号文本使用的字体。
 * @details FileHeader 在 Data 流之前写入，因此所有动态字体必须提前加入字体表。
 */
void AltiumSchLibWriter::registerTextFonts(const QList<AltiumSchComponent>& components) {
    constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;
    for (const AltiumSchComponent& component : components) {
        for (const AltiumSchText& text : component.texts) {
            if (text.fontName.isEmpty() && text.fontId > 0 && text.fontSizeMm <= 0.0)
                continue;
            const QString fontName = text.fontName.isEmpty() ? QStringLiteral("Times New Roman") : text.fontName;
            const int fontSize = text.fontSizeMm > 0.0 ? qMax(1, qRound(text.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;
            getOrAddFont(fontName, fontSize, text.bold, text.italic);
        }
    }
}

/**
 * @brief 添加坐标参数（DXP 单位 + 小数部分）
 */
void AltiumSchLibWriter::addCoordParam(QMap<QString, QString>& params, const QString& key, int raw) {
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
void AltiumSchLibWriter::addColorParam(QMap<QString, QString>& params, const QString& key, uint32_t color) {
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
    if (components.isEmpty()) {
        qWarning() << "AltiumSchLibWriter: Refusing to write an empty library";
        return false;
    }
    m_fonts.clear();
    m_embeddedImageNames.clear();
    m_uniqueIdCounter = 0;
    m_libraryName = libraryName;
    m_diagnostics.clear();

    prepareImageStorageNames(components);

    // 确保有默认字体
    getOrAddFont("Times New Roman", 10);
    registerTextFonts(components);

    OLECompoundWriter ole;
    if (!ole.create()) {
        qWarning() << "AltiumSchLibWriter: Failed to create OLE compound document for" << filePath;
        return false;
    }

    QStringList names;
    for (const AltiumSchComponent& component : components) {
        names.append(component.name);
    }
    const QStringList sectionKeys = AltiumWriterUtils::makeUniqueSectionKeys(names);

    // 写入 FileHeader
    writeFileHeader(ole, components);

    // 写入 SectionKeys（如果需要）
    writeSectionKeys(ole, components, sectionKeys);

    // 写入每个元件的存储
    for (int i = 0; i < components.size(); ++i) {
        writeComponentStorage(ole, components[i], sectionKeys[i]);
    }

    writeImageStorage(ole, components);

    return ole.saveToFile(filePath);
}

/**
 * @brief 写入 FileHeader 流
 */
void AltiumSchLibWriter::writeFileHeader(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components) {
    QMap<QString, QString> params;
    params["HEADER"] = "Protel for Windows - Schematic Library Editor Binary File Version 5.0";
    int totalWeight = 0;
    for (const AltiumSchComponent& component : components) {
        totalWeight += componentRecordCount(component);
    }
    params["WEIGHT"] = QString::number(totalWeight);
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
        if (m_fonts[i].bold)
            params[QString("Bold%1").arg(idx)] = "T";
        if (m_fonts[i].italic)
            params[QString("Italic%1").arg(idx)] = "T";
        if (m_fonts[i].underline)
            params[QString("Underline%1").arg(idx)] = "T";
    }

    params["UseMBCS"] = "T";
    params["IsBOC"] = "T";
    params["SheetStyle"] = "9";
    params["BorderOn"] = "T";
    params["Display_Unit"] = "0";
    params["SYSTEMFONT"] = "1";
    params["SHEETNUMBERSPACESIZE"] = "12";
    params["AREACOLOR"] = "16317695";
    params["SNAPGRIDON"] = "T";
    params["SNAPGRIDSIZE"] = "10";
    params["VISIBLEGRIDON"] = "T";
    params["VISIBLEGRIDSIZE"] = "10";
    params["COMPCOUNT"] = QString::number(components.size());
    for (int i = 0; i < components.size(); ++i) {
        const AltiumSchComponent& component = components[i];
        params[QString("LIBREF%1").arg(i)] = component.name;
        params[QString("COMPDESCR%1").arg(i)] = component.description;
        // 磁盘格式把公共 Part 0 计入总数，IR 的 partCount 只表示用户可见部件。
        params[QString("PARTCOUNT%1").arg(i)] = QString::number(qMax(1, component.partCount) + 1);
    }

    // 序列化
    QByteArray headerData;
    AltiumBinaryWriter writer(headerData);

    // Header 中的库索引同样允许非 ASCII 名称/描述，统一用 ANSI fallback + %UTF8% 扩展。
    writer.writeCStringParameterBlockUtf8(params);

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
                                          const QList<AltiumSchComponent>& components,
                                          const QStringList& sectionKeys) {
    QMap<QString, QString> params;
    int keyCount = 0;

    for (int i = 0; i < components.size(); ++i) {
        const QString& sectionKey = sectionKeys[i];
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
        writer.writeCStringParameterBlockUtf8(params);
        ole.writeStream("SectionKeys", data);
    }
}

/**
 * @brief 写入元件存储
 */
void AltiumSchLibWriter::writeComponentStorage(OLECompoundWriter& ole,
                                               const AltiumSchComponent& component,
                                               const QString& sectionKey) {
    // 创建存储区
    ole.addStorage(sectionKey);

    // 构建 Data 流
    QByteArray data;
    AltiumBinaryWriter writer(data);
    m_nextIndexInSheet = 1;

    // 写入元件记录
    writeComponentRecord(writer, component);

    // 写入引脚
    for (const AltiumSchPin& pin : component.pins) {
        writePinRecord(writer, pin);
    }

    // 写入矩形
    for (const AltiumSchRectangle& rect : component.rectangles) {
        writeRectangleRecord(writer, rect);
    }
    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        writeRoundRectangleRecord(writer, rect);
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
    for (const AltiumSchPie& pie : component.pies) {
        writePieRecord(writer, pie);
    }
    for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
        writeEllipticalArcRecord(writer, arc);
    }

    // 写入折线
    for (const AltiumSchPolyline& polyline : component.polylines) {
        writePolylineRecord(writer, polyline);
    }

    for (const AltiumSchPath& path : component.paths) {
        writePathRecord(writer, path);
    }

    // 写入三次 Bézier 曲线
    for (const AltiumSchBezier& bezier : component.beziers) {
        writeBezierRecord(writer, bezier);
    }

    // 写入 IEEE 图形
    for (const AltiumSchIeee& ieee : component.ieeeSymbols) {
        writeIeeeRecord(writer, ieee);
    }

    // 写入文本
    for (const AltiumSchText& text : component.texts) {
        writeTextRecord(writer, text);
    }
    for (const AltiumSchTextFrame& frame : component.textFrames) {
        writeTextFrameRecord(writer, frame);
    }
    for (const AltiumSchImage& image : component.images) {
        writeImageRecord(writer, image);
    }

    writeComponentParameterRecords(writer, component);

    // 写入实现记录
    writeImplementationRecords(writer, component);

    // 写入流
    ole.writeStream(sectionKey, "Data", data);
}

/**
 * @brief 写入元件记录 (RECORD=1)
 */
void AltiumSchLibWriter::writeComponentRecord(AltiumBinaryWriter& writer, const AltiumSchComponent& component) {
    QMap<QString, QString> params;
    params["RECORD"] = "1";
    params["LibReference"] = component.name;

    if (!component.description.isEmpty()) {
        params["ComponentDescription"] = component.description;
    }

    params["PartCount"] = QString::number(qMax(1, component.partCount) + 1);
    params["DisplayModeCount"] = "1";
    params["IndexInSheet"] = "-1";
    params["OwnerPartId"] = "-1";
    params["CurrentPartId"] = "1";
    params["LibraryPath"] = "*";
    params["SourceLibraryName"] = "*";
    params["SheetPartFileName"] = "*";
    params["TargetFileName"] = "*";
    params["ALLPINCOUNT"] = QString::number(component.pins.size());
    if (!component.aliases.isEmpty())
        params["Aliases"] = component.aliases.join(",");

    addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

/**
 * @brief 写入引脚记录 (RECORD=2, 二进制格式)
 */
void AltiumSchLibWriter::writePinRecord(AltiumBinaryWriter& writer, const AltiumSchPin& pin) {
    writer.beginBlock(AltiumConstants::SCH_BLOCK_FLAG_BINARY_PIN);

    writer.writeInt32(2);  // Record type = 2
    writer.writeUInt8(0);  // Unknown
    writer.writeInt16(static_cast<int16_t>(qBound(-1, pin.ownerPartId, 32767)));  // OwnerPartId，-1 表示公共 Part Zero
    writer.writeUInt8(0);  // OwnerPartDisplayMode

    // Symbol edges / IEEE 装饰。四个字节必须位于描述字符串之前。
    writer.writeUInt8(pin.symbolInnerEdge);
    writer.writeUInt8(pin.symbolOuterEdge);
    writer.writeUInt8(pin.symbolInside);
    writer.writeUInt8(pin.symbolOutside);

    // Description (空 Pascal 短字符串)
    writer.writePascalShortString("");

    // FormalType=1 表示普通的有效引脚。值为 0 时，Altium 仍可能绘制
    // 引脚名称/编号，但不会将该记录作为可连接的 Pin 对象处理。
    writer.writeUInt8(1);  // FormalType: normal pin
    writer.writeUInt8(static_cast<uint8_t>(pin.electricalType));  // ElectricalType

    // PinConglomerate 字节
    uint8_t conglomerate = static_cast<uint8_t>(pin.orientation);  // Bit 0-1: orientation
    if (pin.isHidden)
        conglomerate |= 0x04;  // Bit 2: hidden
    if (pin.showName)
        conglomerate |= 0x08;  // Bit 3: show name
    if (pin.showDesignator)
        conglomerate |= 0x10;  // Bit 4: show designator
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
void AltiumSchLibWriter::writeRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRectangle& rect) {
    QMap<QString, QString> params;
    params["RECORD"] = "14";
    addOwnerParams(params, rect.ownerPartId);
    addCoordParam(params, "Location.X", rect.locationX);
    addCoordParam(params, "Location.Y", rect.locationY);
    addCoordParam(params, "Corner.X", rect.cornerX);
    addCoordParam(params, "Corner.Y", rect.cornerY);

    if (rect.lineWidth != 0)
        params["LineWidth"] = QString::number(rect.lineWidth);
    if (rect.lineStyle != 0)
        params["LineStyleExt"] = QString::number(rect.lineStyle);
    addColorParam(params, "Color", rect.color);
    if (rect.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(rect.areaColor);
    if (rect.isSolid)
        params["IsSolid"] = "T";

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入圆角矩形记录 (RECORD=10)
 */
void AltiumSchLibWriter::writeRoundRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRoundRectangle& rect) {
    QMap<QString, QString> params;
    params["RECORD"] = "10";
    addOwnerParams(params, rect.ownerPartId);
    addCoordParam(params, "Location.X", rect.locationX);
    addCoordParam(params, "Location.Y", rect.locationY);
    addCoordParam(params, "Corner.X", rect.cornerX);
    addCoordParam(params, "Corner.Y", rect.cornerY);
    addCoordParam(params, "CornerXRadius", rect.cornerXRadius);
    addCoordParam(params, "CornerYRadius", rect.cornerYRadius);
    if (rect.lineWidth != 0)
        params["LineWidth"] = QString::number(rect.lineWidth);
    if (rect.lineStyle != 0)
        params["LineStyle"] = QString::number(rect.lineStyle);
    addColorParam(params, "Color", rect.color);
    if (rect.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(rect.areaColor);
    if (rect.isSolid)
        params["IsSolid"] = "T";
    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入线段记录 (RECORD=13)
 */
void AltiumSchLibWriter::writeLineRecord(AltiumBinaryWriter& writer, const AltiumSchLine& line) {
    QMap<QString, QString> params;
    params["RECORD"] = "13";
    addOwnerParams(params, line.ownerPartId);
    addCoordParam(params, "Location.X", line.locationX);
    addCoordParam(params, "Location.Y", line.locationY);
    addCoordParam(params, "Corner.X", line.cornerX);
    addCoordParam(params, "Corner.Y", line.cornerY);

    params["LineWidth"] = QString::number(AltiumCoord::lineWidthToIndex(line.lineWidth));
    if (line.lineStyle != 0)
        params["LineStyle"] = QString::number(line.lineStyle);
    addColorParam(params, "Color", line.color);

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入弧线记录 (RECORD=12)
 */
void AltiumSchLibWriter::writeArcRecord(AltiumBinaryWriter& writer, const AltiumSchArc& arc) {
    QMap<QString, QString> params;
    params["RECORD"] = "12";
    addOwnerParams(params, arc.ownerPartId);
    addCoordParam(params, "Location.X", arc.centerX);
    addCoordParam(params, "Location.Y", arc.centerY);
    addCoordParam(params, "Radius", arc.radius);

    if (arc.lineWidth != 0)
        params["LineWidth"] = QString::number(arc.lineWidth);
    if (arc.lineStyle != 0)
        params["LineStyle"] = QString::number(arc.lineStyle);
    if (arc.startAngle != 0.0)
        params["StartAngle"] = QString::number(arc.startAngle, 'f', 3);
    params["EndAngle"] = QString::number(arc.endAngle, 'f', 3);
    addColorParam(params, "Color", arc.color);

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入多边形记录 (RECORD=7)
 */
void AltiumSchLibWriter::writePolygonRecord(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon) {
    QMap<QString, QString> params;
    params["RECORD"] = "7";
    addOwnerParams(params, polygon.ownerPartId);
    params["LineWidth"] = QString::number(polygon.lineWidth);
    if (polygon.lineStyle != 0)
        params["LineStyle"] = QString::number(polygon.lineStyle);
    addColorParam(params, "Color", polygon.color);
    if (polygon.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(polygon.areaColor);
    if (polygon.isSolid)
        params["IsSolid"] = "T";

    params["LocationCount"] = QString::number(polygon.vertices.size());
    for (int i = 0; i < polygon.vertices.size(); ++i) {
        int idx = i + 1;
        int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(polygon.vertices[i].x()));
        int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(polygon.vertices[i].y()));
        if (x != 0)
            params[QString("X%1").arg(idx)] = QString::number(x);
        if (y != 0)
            params[QString("Y%1").arg(idx)] = QString::number(y);
    }

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入椭圆记录 (RECORD=8)
 */
void AltiumSchLibWriter::writeEllipseRecord(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse) {
    QMap<QString, QString> params;
    params["RECORD"] = "8";
    addOwnerParams(params, ellipse.ownerPartId);
    addCoordParam(params, "Location.X", ellipse.centerX);
    addCoordParam(params, "Location.Y", ellipse.centerY);
    addCoordParam(params, "Radius", ellipse.radiusX);
    addCoordParam(params, "SecondaryRadius", ellipse.radiusY);

    if (ellipse.lineWidth != 0)
        params["LineWidth"] = QString::number(ellipse.lineWidth);
    if (ellipse.lineStyle != 0)
        params["LineStyle"] = QString::number(ellipse.lineStyle);
    addColorParam(params, "Color", ellipse.color);
    if (ellipse.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(ellipse.areaColor);
    if (ellipse.isSolid)
        params["IsSolid"] = "T";

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入扇形记录 (RECORD=9)
 */
void AltiumSchLibWriter::writePieRecord(AltiumBinaryWriter& writer, const AltiumSchPie& pie) {
    QMap<QString, QString> params;
    params["RECORD"] = "9";
    addOwnerParams(params, pie.ownerPartId);
    addCoordParam(params, "Location.X", pie.centerX);
    addCoordParam(params, "Location.Y", pie.centerY);
    addCoordParam(params, "Radius", pie.radius);
    if (pie.lineWidth != 0)
        params["LineWidth"] = QString::number(pie.lineWidth);
    if (pie.lineStyle != 0)
        params["LineStyle"] = QString::number(pie.lineStyle);
    if (pie.startAngle != 0.0)
        params["StartAngle"] = QString::number(pie.startAngle, 'f', 3);
    params["EndAngle"] = QString::number(pie.endAngle, 'f', 3);
    addColorParam(params, "Color", pie.color);
    if (pie.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(pie.areaColor);
    if (pie.isSolid)
        params["IsSolid"] = "T";
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入椭圆弧记录 (RECORD=11)
 */
void AltiumSchLibWriter::writeEllipticalArcRecord(AltiumBinaryWriter& writer, const AltiumSchEllipticalArc& arc) {
    QMap<QString, QString> params;
    params["RECORD"] = "11";
    addOwnerParams(params, arc.ownerPartId);
    addCoordParam(params, "Location.X", arc.centerX);
    addCoordParam(params, "Location.Y", arc.centerY);
    addCoordParam(params, "Radius", arc.radiusX);
    addCoordParam(params, "SecondaryRadius", arc.radiusY);
    if (arc.lineWidth != 0)
        params["LineWidth"] = QString::number(arc.lineWidth);
    if (arc.lineStyle != 0)
        params["LineStyle"] = QString::number(arc.lineStyle);
    if (arc.startAngle != 0.0)
        params["StartAngle"] = QString::number(arc.startAngle, 'f', 3);
    params["EndAngle"] = QString::number(arc.endAngle, 'f', 3);
    addColorParam(params, "Color", arc.color);
    if (arc.areaColor != 0xFFFFFF)
        params["AreaColor"] = QString::number(arc.areaColor);
    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入折线记录 (RECORD=6)
 */
void AltiumSchLibWriter::writePolylineRecord(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline) {
    QMap<QString, QString> params;
    params["RECORD"] = "6";
    addOwnerParams(params, polyline.ownerPartId);
    params["LineWidth"] = QString::number(polyline.lineWidth);
    if (polyline.lineStyle != 0)
        params["LineStyle"] = QString::number(polyline.lineStyle);
    addColorParam(params, "Color", polyline.color);

    params["LocationCount"] = QString::number(polyline.vertices.size());
    for (int i = 0; i < polyline.vertices.size(); ++i) {
        int idx = i + 1;
        int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(polyline.vertices[i].x()));
        int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(polyline.vertices[i].y()));
        if (x != 0)
            params[QString("X%1").arg(idx)] = QString::number(x);
        if (y != 0)
            params[QString("Y%1").arg(idx)] = QString::number(y);
    }

    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入路径记录（委托给折线记录）
 * @param writer 二进制写入器
 * @param path 路径数据
 */
void AltiumSchLibWriter::writePathRecord(AltiumBinaryWriter& writer, const AltiumSchPath& path) {
    AltiumSchPolyline polyline;
    polyline.vertices = path.vertices;
    polyline.lineWidth = path.lineWidth;
    polyline.lineStyle = path.lineStyle;
    polyline.color = path.color;
    polyline.ownerPartId = path.ownerPartId;
    writePolylineRecord(writer, polyline);
}

/**
 * @brief 写入三次 Bézier 曲线记录 (RECORD=5)
 */
void AltiumSchLibWriter::writeBezierRecord(AltiumBinaryWriter& writer, const AltiumSchBezier& bezier) {
    if (bezier.controlPoints.size() != 4)
        return;

    QMap<QString, QString> params;
    params["RECORD"] = "5";
    addOwnerParams(params, bezier.ownerPartId);
    params["LineWidth"] = QString::number(bezier.lineWidth);
    addColorParam(params, "Color", bezier.color);
    params["LocationCount"] = "4";
    for (int i = 0; i < 4; ++i) {
        const int32_t x = AltiumCoord::toSchematicUnits(static_cast<int>(bezier.controlPoints[i].x()));
        const int32_t y = AltiumCoord::toSchematicUnits(static_cast<int>(bezier.controlPoints[i].y()));
        if (x != 0)
            params[QString("X%1").arg(i + 1)] = QString::number(x);
        if (y != 0)
            params[QString("Y%1").arg(i + 1)] = QString::number(y);
    }
    addUniqueID(params);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入 IEEE 图形记录 (RECORD=3)
 */
void AltiumSchLibWriter::writeIeeeRecord(AltiumBinaryWriter& writer, const AltiumSchIeee& ieee) {
    QMap<QString, QString> params;
    params["RECORD"] = "3";
    addOwnerParams(params, ieee.ownerPartId);
    params["Symbol"] = QString::number(ieee.symbol);
    addCoordParam(params, "Location.X", ieee.locationX);
    addCoordParam(params, "Location.Y", ieee.locationY);
    params["ScaleFactor"] = QString::number(qMax(1, ieee.scaleFactor));
    if (ieee.orientation != 0)
        params["Orientation"] = QString::number(ieee.orientation);
    params["LineWidth"] = QString::number(qMax(0, ieee.lineWidth));
    if (ieee.mirrored)
        params["Mirror"] = "T";
    addColorParam(params, "Color", ieee.color);
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入文本记录 (RECORD=4, Label)
 */
void AltiumSchLibWriter::writeTextRecord(AltiumBinaryWriter& writer, const AltiumSchText& text) {
    QMap<QString, QString> params;
    params["RECORD"] = "4";
    addOwnerParams(params, text.ownerPartId);
    addCoordParam(params, "Location.X", text.locationX);
    addCoordParam(params, "Location.Y", text.locationY);

    if (text.orientation != 0)
        params["Orientation"] = QString::number(text.orientation);
    addColorParam(params, "Color", text.color);
    int fontId = text.fontId;
    if (!text.fontName.isEmpty() || fontId <= 0) {
        constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;
        const QString fontName = text.fontName.isEmpty() ? QStringLiteral("Times New Roman") : text.fontName;
        const int fontSize = text.fontSizeMm > 0.0 ? qMax(1, qRound(text.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;
        fontId = getOrAddFont(fontName, fontSize, text.bold, text.italic);
    }
    params["FontID"] = QString::number(fontId);
    params["Text"] = text.text;
    if (text.isHidden || !text.isDisplayed)
        params["IsHidden"] = "T";
    if (!text.anchor.isEmpty())
        params["TextAnchor"] = text.anchor;
    if (text.fontSizeMm > 0.0)
        params["FontSize"] = QString::number(text.fontSizeMm, 'f', 4);

    addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

/**
 * @brief 写入文本框记录 (RECORD=28)
 */
void AltiumSchLibWriter::writeTextFrameRecord(AltiumBinaryWriter& writer, const AltiumSchTextFrame& frame) {
    QMap<QString, QString> params;
    params["RECORD"] = "28";
    addOwnerParams(params, frame.ownerPartId);
    addCoordParam(params, "Location.X", frame.locationX);
    addCoordParam(params, "Location.Y", frame.locationY);
    addCoordParam(params, "Corner.X", frame.cornerX);
    addCoordParam(params, "Corner.Y", frame.cornerY);
    if (frame.lineWidth != 0)
        params["LineWidth"] = QString::number(frame.lineWidth);
    if (frame.lineStyle != 0)
        params["LineStyle"] = QString::number(frame.lineStyle);
    addColorParam(params, "Color", frame.color);
    params["AreaColor"] = QString::number(frame.areaColor);
    addColorParam(params, "TextColor", frame.textColor);
    params["FontID"] = QString::number(frame.fontId);
    if (frame.isSolid)
        params["IsSolid"] = "T";
    if (frame.showBorder)
        params["ShowBorder"] = "T";
    if (frame.orientation != 0)
        params["Orientation"] = QString::number(frame.orientation);
    if (frame.alignment != 0)
        params["Alignment"] = QString::number(frame.alignment);
    if (frame.wordWrap)
        params["WordWrap"] = "T";
    if (frame.clipToRect)
        params["ClipToRect"] = "T";
    params["Text"] = frame.text;
    addCoordParam(params, "TextMargin", frame.textMargin);
    if (frame.transparent)
        params["Transparent"] = "T";
    addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

/**
 * @brief 写入图片记录 (RECORD=30)
 */
void AltiumSchLibWriter::writeImageRecord(AltiumBinaryWriter& writer, const AltiumSchImage& image) {
    QMap<QString, QString> params;
    params["RECORD"] = "30";
    addOwnerParams(params, image.ownerPartId);
    addCoordParam(params, "Location.X", image.locationX);
    addCoordParam(params, "Location.Y", image.locationY);
    addCoordParam(params, "Corner.X", image.cornerX);
    addCoordParam(params, "Corner.Y", image.cornerY);
    if (image.lineWidth != 0)
        params["LineWidth"] = QString::number(image.lineWidth);
    if (image.lineStyle != 0)
        params["LineStyle"] = QString::number(image.lineStyle);
    addColorParam(params, "Color", image.color);
    if (image.areaColor != 0)
        params["AreaColor"] = QString::number(image.areaColor);
    if (image.isSolid)
        params["IsSolid"] = "T";
    if (image.transparent)
        params["Transparent"] = "T";
    if (image.showBorder)
        params["ShowBorder"] = "T";
    if (image.keepAspect)
        params["KeepAspect"] = "T";
    const QString storageFileName = m_embeddedImageNames.value(&image, image.fileName);
    const bool hasEmbeddedImage = image.embedImage && !image.data.isEmpty() && !storageFileName.isEmpty() &&
                                  storageFileName.toLocal8Bit().size() <= 255;
    if (hasEmbeddedImage)
        params["EmbedImage"] = "T";
    if (!storageFileName.isEmpty())
        params["FileName"] = storageFileName;
    addUniqueID(params);
    writer.writeCStringParameterBlockUtf8(params);
}

/**
 * @brief 为嵌入图片生成稳定且唯一的 Storage 文件名。
 * @details Altium 的 Storage 以文件名关联图片记录；重复名称会导致多个图元指向同一条目。
 */
void AltiumSchLibWriter::prepareImageStorageNames(const QList<AltiumSchComponent>& components) {
    QSet<QString> usedNames;
    for (const AltiumSchComponent& component : components) {
        for (int imageIndex = 0; imageIndex < component.images.size(); ++imageIndex) {
            const AltiumSchImage& image = component.images.at(imageIndex);
            if (!image.embedImage)
                continue;

            QString sourceName = image.fileName;
            sourceName.replace('\\', '/');
            const QString embeddedName = QFileInfo(sourceName).fileName();
            const QByteArray encodedName = embeddedName.toLocal8Bit();
            if (image.data.isEmpty()) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入数据为空，已跳过 Storage")
                                               .arg(component.name)
                                               .arg(imageIndex);
                m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }
            if (embeddedName.isEmpty() || embeddedName == QStringLiteral(".") || embeddedName == QStringLiteral("..") ||
                embeddedName.contains('|') || embeddedName.contains(QChar::Null) || encodedName.size() > 255) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入文件名无效: %3，已跳过 Storage")
                                               .arg(component.name)
                                               .arg(imageIndex)
                                               .arg(image.fileName);
                m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }

            const QFileInfo fileInfo(embeddedName);
            const QString suffix = fileInfo.suffix();
            QString baseName =
                suffix.isEmpty() ? embeddedName : embeddedName.left(embeddedName.size() - suffix.size() - 1);
            QString candidate = embeddedName;
            int duplicateIndex = 1;
            while (usedNames.contains(candidate.toCaseFolded())) {
                ++duplicateIndex;
                const QString suffixText = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
                const QString marker = QStringLiteral("_%1").arg(duplicateIndex);
                QString trimmedBase = baseName;
                while (!trimmedBase.isEmpty() && (trimmedBase + marker + suffixText).toLocal8Bit().size() > 255)
                    trimmedBase.chop(1);
                candidate = trimmedBase + marker + suffixText;
            }
            usedNames.insert(candidate.toCaseFolded());
            m_embeddedImageNames.insert(&image, candidate);
            if (candidate != embeddedName) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入文件名 %3 重复，已改为 %4")
                                               .arg(component.name)
                                               .arg(imageIndex)
                                               .arg(embeddedName)
                                               .arg(candidate);
                m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
            }
        }
    }
}

/**
 * @brief 写入 SchLib 根 /Storage 图片流
 * @details 图片内容使用 Qt zlib 压缩结果（去除 qCompress 的四字节长度头），
 *          并按 Altium 的 D0 标记和 Pascal 文件名组织条目。
 */
void AltiumSchLibWriter::writeImageStorage(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components) {
    QList<const AltiumSchImage*> embeddedImages;
    for (const AltiumSchComponent& component : components) {
        for (const AltiumSchImage& image : component.images) {
            if (image.embedImage && !image.data.isEmpty() && m_embeddedImageNames.contains(&image))
                embeddedImages.append(&image);
        }
    }

    QByteArray storageData;
    AltiumBinaryWriter storageWriter(storageData);
    QMap<QString, QString> storageParams;
    storageParams["HEADER"] = "Icon storage";
    if (!embeddedImages.isEmpty())
        storageParams["Weight"] = QString::number(embeddedImages.size());
    storageWriter.writeCStringParameterBlock(storageParams);

    for (const AltiumSchImage* image : embeddedImages) {
        const QByteArray compressed = qCompress(image->data, 9).mid(4);
        const QByteArray name = m_embeddedImageNames.value(image).toLocal8Bit();
        if (name.isEmpty() || name.size() > 255)
            continue;

        QByteArray entry;
        QDataStream entryStream(&entry, QIODevice::WriteOnly);
        entryStream.setByteOrder(QDataStream::LittleEndian);
        entryStream << static_cast<quint8>(0xD0) << static_cast<quint8>(name.size());
        entry.append(name);
        entryStream.device()->seek(entry.size());
        entryStream << static_cast<quint32>(compressed.size());
        entry.append(compressed);

        QDataStream blockStream(&storageData, QIODevice::WriteOnly | QIODevice::Append);
        blockStream.setByteOrder(QDataStream::LittleEndian);
        blockStream << static_cast<quint32>(0x01000000U | static_cast<quint32>(entry.size()));
        blockStream.writeRawData(entry.constData(), entry.size());
    }
    ole.writeStream("Storage", storageData);
}

/**
 * @brief 写入元件参数记录（Designator、Value 和自定义参数）
 * @param writer 二进制写入器
 * @param component 元件数据
 */
void AltiumSchLibWriter::writeComponentParameterRecords(AltiumBinaryWriter& writer,
                                                        const AltiumSchComponent& component) {
    QString designator = component.designatorPrefix.trimmed();
    if (designator.isEmpty()) {
        designator = "?";
    } else if (!designator.endsWith('?')) {
        designator += '?';
    }

    QMap<QString, QString> designatorParams;
    designatorParams["RECORD"] = "34";
    designatorParams["OWNERPARTID"] = "-1";
    designatorParams["LOCATION.X_FRAC"] = "-5";
    designatorParams["LOCATION.Y_FRAC"] = "5";
    designatorParams["COLOR"] = "8388608";
    designatorParams["FONTID"] = "1";
    designatorParams["TEXT"] = designator;
    designatorParams["NAME"] = "Designator";
    designatorParams["READONLYSTATE"] = "1";
    addUniqueID(designatorParams);
    writer.writeCStringParameterBlockUtf8(designatorParams);

    for (const ParameterField& field : componentParameterFields(component)) {
        QMap<QString, QString> parameterParams;
        parameterParams["RECORD"] = "41";
        parameterParams["OWNERPARTID"] = QString::number(field.ownerPartId);
        if (field.ownerPartId >= 1) {
            parameterParams["IndexInSheet"] = QString::number(m_nextIndexInSheet);
            ++m_nextIndexInSheet;
        }
        if (field.hasLocation) {
            addCoordParam(parameterParams, "LOCATION.X", field.locationX);
            addCoordParam(parameterParams, "LOCATION.Y", field.locationY);
        } else {
            parameterParams["LOCATION.X_FRAC"] = "-5";
            parameterParams["LOCATION.Y_FRAC"] = "-15";
        }
        if (field.color != 0)
            parameterParams["COLOR"] = QString::number(field.color);
        parameterParams["FONTID"] = QString::number(field.fontId);
        parameterParams["TEXT"] = field.value;
        parameterParams["NAME"] = field.name;
        if (field.orientation != 0)
            parameterParams["Orientation"] = QString::number(field.orientation);
        if (field.hasVisibility && field.isHidden)
            parameterParams["IsHidden"] = "T";
        if (field.readOnly)
            parameterParams["READONLYSTATE"] = "1";
        addUniqueID(parameterParams);
        writer.writeCStringParameterBlockUtf8(parameterParams);
    }
}

/**
 * @brief 写入实现记录 (RECORD=44-48)
 */
void AltiumSchLibWriter::writeImplementationRecords(AltiumBinaryWriter& writer, const AltiumSchComponent& component) {
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
    for (int implementationIndex = 0; implementationIndex < component.implementations.size(); ++implementationIndex) {
        const AltiumSchComponent::Implementation& impl = component.implementations.at(implementationIndex);
        // RECORD=45: Implementation
        {
            QMap<QString, QString> params;
            params["RECORD"] = "45";
            params["DESCRIPTION"] = impl.modelName;
            params["MODELNAME"] = impl.modelName;
            params["MODELTYPE"] = impl.modelType;
            const QString modelType = impl.modelType.trimmed().toUpper();
            const bool isPcbLibrary = modelType.isEmpty() || modelType == QStringLiteral("PCBLIB");
            const bool hasExplicitDataFileKind =
                !impl.dataFileKind.trimmed().isEmpty() &&
                (isPcbLibrary || impl.dataFileKind.compare(QStringLiteral("PCBLib"), Qt::CaseInsensitive) != 0);
            const bool hasDataFile =
                isPcbLibrary || hasExplicitDataFileKind || !impl.dataFileEntity.trimmed().isEmpty();
            params["DATAFILECOUNT"] = hasDataFile ? "1" : "0";
            if (hasDataFile) {
                params["MODELDATAFILEKIND1"] = impl.dataFileKind.isEmpty() ? "PCBLib" : impl.dataFileKind;
                params["MODELDATAFILEENTITY1"] =
                    impl.dataFileEntity.isEmpty()
                        ? (isPcbLibrary && !m_libraryName.isEmpty() ? m_libraryName + ".PcbLib" : "*")
                        : impl.dataFileEntity;
            }
            // Altium 只允许一个默认实现，候选封装和附加模型不能全部标记为当前。
            if (implementationIndex == 0)
                params["ISCURRENT"] = "T";
            addUniqueID(params);
            writer.writeCStringParameterBlock(params);
        }

        // RECORD=46: MapDefinerList（容器）
        {
            QMap<QString, QString> params;
            params["RECORD"] = "46";
            writer.writeCStringParameterBlock(params);
        }

        // RECORD=47: 将每个引脚映射到当前实现。
        // Altium 会依据这些索引建立符号引脚与 PCBLib 实现之间的关系；
        // 缺少该记录时，库虽然可以打开，但引脚归属和封装关联并不完整。
        for (int pinIndex = 1; pinIndex <= component.pins.size(); ++pinIndex) {
            QMap<QString, QString> pinMappingParams;
            pinMappingParams["RECORD"] = "47";
            pinMappingParams["DESINTF"] = QString::number(pinIndex);
            pinMappingParams["DESIMPCOUNT"] = "1";
            const QString mappedPin = impl.pinMappings.value(QString::number(pinIndex), QString::number(pinIndex));
            pinMappingParams["DESIMP0"] = mappedPin;
            pinMappingParams["ISTRIVIAL"] = "T";
            addUniqueID(pinMappingParams);
            writer.writeCStringParameterBlock(pinMappingParams);
        }

        // RECORD=48: ImplementationParameters
        {
            QMap<QString, QString> params;
            params["RECORD"] = "48";
            for (auto it = impl.parameters.constBegin(); it != impl.parameters.constEnd(); ++it)
                params[it.key()] = it.value();
            writer.writeCStringParameterBlock(params);
        }
    }
}

/**
 * @brief 计算元件的记录总数（用于 FileHeader WEIGHT 字段）
 * @param component 元件数据
 * @return 记录数（含元件记录、图元、参数和实现记录）
 */
int AltiumSchLibWriter::componentRecordCount(const AltiumSchComponent& component) const {
    int validBezierCount = 0;
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() == 4)
            ++validBezierCount;
    }
    const int graphics = component.pins.size() + component.rectangles.size() + component.roundRectangles.size() +
                         component.lines.size() + component.arcs.size() + component.polygons.size() +
                         component.ellipses.size() + component.pies.size() + component.ellipticalArcs.size() +
                         component.polylines.size() + component.paths.size() + validBezierCount +
                         component.ieeeSymbols.size() + component.texts.size() + component.textFrames.size() +
                         component.images.size();
    // Component + graphics + 参数字段 + ImplementationList + implementation records.
    // 每个实现包含 RECORD=45、46、48，以及每个符号引脚对应的 RECORD=47。
    return 1 + graphics + componentParameterRecordCount(component) + 1 +
           component.implementations.size() * (3 + component.pins.size());
}

/**
 * @brief 计算元件参数记录数。
 * @param component 元件数据
 * @return Designator、Value 和自定义参数记录总数
 */
int AltiumSchLibWriter::componentParameterRecordCount(const AltiumSchComponent& component) const {
    return 1 + componentParameterFields(component).size();
}

/**
 * @brief 向参数映射中添加 OwnerPartId 和 ISNOTACCESIBLE 字段
 * @param params 参数映射（输出）
 * @param ownerPartId 所属部件 ID
 */
void AltiumSchLibWriter::addOwnerParams(QMap<QString, QString>& params, int ownerPartId) {
    params["ISNOTACCESIBLE"] = "T";
    params["IndexInSheet"] = QString::number(m_nextIndexInSheet);
    params["OWNERPARTID"] = QString::number(ownerPartId < 0 ? -1 : qMax(1, ownerPartId));
    ++m_nextIndexInSheet;
}

}  // namespace EasyKiConverter
