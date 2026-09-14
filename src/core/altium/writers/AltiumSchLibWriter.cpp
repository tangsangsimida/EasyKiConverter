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

#include <algorithm>
#include <cmath>

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
    double fontSizeMm = 0.0;
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
                                 uint32_t color = 0x000000,
                                 double fontSizeMm = 0.0) {
        const QString normalizedName = name.trimmed();
        const QString deduplicationKey = QStringLiteral("%1:%2").arg(normalizedName).arg(ownerPartId);
        if (normalizedName.isEmpty() || value.trimmed().isEmpty() || names.contains(deduplicationKey))
            return;
        ParameterField field;
        field.name = normalizedName;
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
        field.fontSizeMm = fontSizeMm;
        fields.append(field);
        names.insert(deduplicationKey);
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
                    parameter.color,
                    parameter.fontSizeMm);
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
            const bool hasValidFontSize = std::isfinite(text.fontSizeMm) && text.fontSizeMm > 0.0;
            if (text.fontName.isEmpty() && text.fontId > 0 && !hasValidFontSize)
                continue;
            const QString fontName = text.fontName.isEmpty() ? QStringLiteral("Times New Roman") : text.fontName;
            const int fontSize = hasValidFontSize ? qMax(1, qRound(text.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;
            getOrAddFont(fontName, fontSize, text.bold, text.italic);
        }
        for (const AltiumSchParameter& parameter : component.parameters) {
            if (!std::isfinite(parameter.fontSizeMm) || parameter.fontSizeMm <= 0.0)
                continue;
            constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;
            const int fontSize = qMax(1, qRound(parameter.fontSizeMm / MILLIMETERS_PER_POINT));
            getOrAddFont(QStringLiteral("Times New Roman"), fontSize);
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
    m_diagnostics.clear();
    if (components.isEmpty()) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 输入组件为空，已拒绝写入"));
        qWarning() << "AltiumSchLibWriter: Refusing to write an empty library";
        return false;
    }
    if (filePath.trimmed().isEmpty()) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 输出路径为空，已拒绝写入"));
        qWarning() << "AltiumSchLibWriter: Refusing to write without an output path";
        return false;
    }
    for (const AltiumSchComponent& component : components) {
        if (component.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("Altium SchLib 组件名称为空，已拒绝写入"));
            qWarning() << "AltiumSchLibWriter: Refusing to write a component without a name";
            return false;
        }
        if (!validateGeometry(component))
            return false;
        if (!validatePartOwnership(component))
            return false;
        if (component.partCount <= 0) {
            m_diagnostics.append(
                QStringLiteral("Altium SchLib 组件 %1 的 partCount 无效，已规范化为 1").arg(component.name));
        }
    }
    m_fonts.clear();
    m_embeddedImageNames.clear();
    m_uniqueIdCounter = 0;
    m_libraryName = libraryName;

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

    if (ole.hasError()) {
        qWarning() << "AltiumSchLibWriter: Failed to construct OLE document:" << ole.errorString();
        return false;
    }

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
 * @brief 按来源顺序写入单个符号图元
 * @details 路径可能拆分为多个 Altium 原生记录，因此按 sourceSegmentIndex 重新合并其写出顺序。
 */
void AltiumSchLibWriter::writeOrderedGraphic(AltiumBinaryWriter& writer,
                                             const AltiumSchComponent& component,
                                             const AltiumSchGraphicOrder& order) {
    const auto matchesPart = [&order](int sourcePartIndex) { return sourcePartIndex == order.partIndex; };
    if (order.type == QStringLiteral("P")) {
        int localIndex = 0;
        for (const AltiumSchPin& pin : component.pins) {
            const bool matchesSourcePart =
                order.partIndex < 0 ? pin.ownerPartId == -1 : pin.sourcePartIndex == order.partIndex;
            if (!matchesSourcePart)
                continue;
            if (localIndex == order.index) {
                writePinRecord(writer, pin);
                return;
            }
            ++localIndex;
        }
        return;
    }
    if (order.type == QStringLiteral("R")) {
        for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
            if (rect.sourceGraphicIndex == order.index && matchesPart(rect.sourcePartIndex)) {
                writeRoundRectangleRecord(writer, rect);
                return;
            }
        }
        for (const AltiumSchRectangle& rect : component.rectangles) {
            if (rect.sourceGraphicIndex == order.index && matchesPart(rect.sourcePartIndex)) {
                writeRectangleRecord(writer, rect);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("C") || order.type == QStringLiteral("E")) {
        for (const AltiumSchEllipse& ellipse : component.ellipses) {
            if (ellipse.sourceGraphicType == order.type && ellipse.sourceGraphicIndex == order.index &&
                matchesPart(ellipse.sourcePartIndex)) {
                writeEllipseRecord(writer, ellipse);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("A")) {
        for (const AltiumSchArc& arc : component.arcs) {
            if (arc.sourceGraphicType == order.type && arc.sourceGraphicIndex == order.index &&
                matchesPart(arc.sourcePartIndex)) {
                writeArcRecord(writer, arc);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("PL")) {
        for (const AltiumSchPolyline& polyline : component.polylines) {
            if (polyline.sourceGraphicIndex == order.index && matchesPart(polyline.sourcePartIndex)) {
                writePolylineRecord(writer, polyline);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("PG")) {
        for (const AltiumSchPolygon& polygon : component.polygons) {
            if (polygon.sourceGraphicIndex == order.index && matchesPart(polygon.sourcePartIndex)) {
                writePolygonRecord(writer, polygon);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("T")) {
        for (const AltiumSchText& text : component.texts) {
            if (!text.isPinLabel && text.sourceGraphicIndex == order.index && matchesPart(text.sourcePartIndex)) {
                writeTextRecord(writer, text);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("I")) {
        for (const AltiumSchImage& image : component.images) {
            if (image.sourceGraphicIndex == order.index && matchesPart(image.sourcePartIndex)) {
                writeImageRecord(writer, image);
                return;
            }
        }
        return;
    }
    if (order.type == QStringLiteral("PT")) {
        for (const AltiumSchPath& path : component.paths) {
            if (path.sourceGraphicType == order.type && path.sourceGraphicIndex == order.index &&
                matchesPart(path.sourcePartIndex) && path.sourceSegmentIndex < 0) {
                writePathRecord(writer, path);
                return;
            }
        }
        QSet<int> segmentIndexSet;
        const auto collectSegmentIndices = [&](const auto& graphics) {
            for (const auto& graphic : graphics) {
                if (graphic.sourceGraphicType == order.type && graphic.sourceGraphicIndex == order.index &&
                    matchesPart(graphic.sourcePartIndex) && graphic.sourceSegmentIndex >= 0) {
                    segmentIndexSet.insert(graphic.sourceSegmentIndex);
                }
            }
        };
        collectSegmentIndices(component.paths);
        collectSegmentIndices(component.beziers);
        collectSegmentIndices(component.arcs);
        collectSegmentIndices(component.ellipticalArcs);
        QList<int> segmentIndices = segmentIndexSet.values();
        std::sort(segmentIndices.begin(), segmentIndices.end());
        for (const int segmentIndex : segmentIndices) {
            for (const AltiumSchPath& path : component.paths) {
                if (path.sourceGraphicType == order.type && path.sourceGraphicIndex == order.index &&
                    matchesPart(path.sourcePartIndex) && path.sourceSegmentIndex == segmentIndex) {
                    writePathRecord(writer, path);
                    break;
                }
            }
            for (const AltiumSchBezier& bezier : component.beziers) {
                if (bezier.sourceGraphicType == order.type && bezier.sourceGraphicIndex == order.index &&
                    matchesPart(bezier.sourcePartIndex) && bezier.sourceSegmentIndex == segmentIndex) {
                    writeBezierRecord(writer, bezier);
                    break;
                }
            }
            for (const AltiumSchArc& arc : component.arcs) {
                if (arc.sourceGraphicType == order.type && arc.sourceGraphicIndex == order.index &&
                    matchesPart(arc.sourcePartIndex) && arc.sourceSegmentIndex == segmentIndex) {
                    writeArcRecord(writer, arc);
                    break;
                }
            }
            for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
                if (arc.sourceGraphicType == order.type && arc.sourceGraphicIndex == order.index &&
                    matchesPart(arc.sourcePartIndex) && arc.sourceSegmentIndex == segmentIndex) {
                    writeEllipticalArcRecord(writer, arc);
                    break;
                }
            }
        }
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
    // Altium 对图元和二进制引脚使用同一个从 0 开始的内容记录计数器。
    // 首条内容记录隐含索引 0，文本记录因此省略 IndexInSheet=0。
    m_nextIndexInSheet = 0;

    // 写入元件记录
    writeComponentRecord(writer, component);

    const bool useGraphicOrder = !component.graphicOrder.isEmpty() && hasCompleteGraphicOrder(component);
    const bool hasImageOrder = std::any_of(
        component.graphicOrder.cbegin(), component.graphicOrder.cend(), [](const AltiumSchGraphicOrder& order) {
            return order.type == QStringLiteral("I");
        });
    if (!component.graphicOrder.isEmpty() && !useGraphicOrder) {
        m_diagnostics.append(
            QStringLiteral("符号 %1 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序").arg(component.name));
    }

    if (useGraphicOrder) {
        for (const AltiumSchGraphicOrder& order : component.graphicOrder)
            writeOrderedGraphic(writer, component, order);
        // 引脚名称和编号是由引脚派生出的文本，不在源 shape 顺序中。
        for (const AltiumSchText& text : component.texts) {
            if (text.isPinLabel)
                writeTextRecord(writer, text);
        }
        for (const AltiumSchText& text : component.texts) {
            if (!text.isPinLabel && text.sourceGraphicIndex < 0)
                writeTextRecord(writer, text);
        }
        // 兼容没有来源顺序引用的扩展图元（例如手工构造的 Bézier）。
        for (const AltiumSchLine& line : component.lines)
            writeLineRecord(writer, line);
        for (const AltiumSchPie& pie : component.pies)
            writePieRecord(writer, pie);
        for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
            if (arc.sourceGraphicType.isEmpty())
                writeEllipticalArcRecord(writer, arc);
        }
        for (const AltiumSchIeee& ieee : component.ieeeSymbols)
            writeIeeeRecord(writer, ieee);
        for (const AltiumSchBezier& bezier : component.beziers) {
            if (bezier.sourceGraphicType.isEmpty())
                writeBezierRecord(writer, bezier);
        }
        for (const AltiumSchRectangle& rect : component.rectangles) {
            if (rect.sourceGraphicIndex < 0)
                writeRectangleRecord(writer, rect);
        }
        for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
            if (rect.sourceGraphicIndex < 0)
                writeRoundRectangleRecord(writer, rect);
        }
        for (const AltiumSchPolygon& polygon : component.polygons) {
            if (polygon.sourceGraphicIndex < 0)
                writePolygonRecord(writer, polygon);
        }
        for (const AltiumSchEllipse& ellipse : component.ellipses) {
            if (ellipse.sourceGraphicType.isEmpty())
                writeEllipseRecord(writer, ellipse);
        }
        for (const AltiumSchArc& arc : component.arcs) {
            if (arc.sourceGraphicType.isEmpty())
                writeArcRecord(writer, arc);
        }
        for (const AltiumSchPolyline& polyline : component.polylines) {
            if (polyline.sourceGraphicIndex < 0)
                writePolylineRecord(writer, polyline);
        }
        for (const AltiumSchPath& path : component.paths) {
            if (path.sourceGraphicType.isEmpty())
                writePathRecord(writer, path);
        }
    } else {
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
    }
    for (const AltiumSchTextFrame& frame : component.textFrames) {
        writeTextFrameRecord(writer, frame);
    }
    for (const AltiumSchImage& image : component.images) {
        if (!useGraphicOrder || !hasImageOrder || image.sourceGraphicIndex < 0)
            writeImageRecord(writer, image);
    }

    writeComponentParameterRecords(writer, component);

    // 写入实现记录
    writeImplementationRecords(writer, component);

    // 写入流
    ole.writeStream(sectionKey, "Data", data);
}

/**
 * @brief 检查来源图元顺序是否覆盖所有可排序图元。
 * @details 不完整的顺序会导致图元被静默跳过，并使 FileHeader 的 WEIGHT 与 Data 流不一致。
 */
bool AltiumSchLibWriter::hasCompleteGraphicOrder(const AltiumSchComponent& component) const {
    QSet<QString> expected;
    QSet<QString> actual;
    QMap<QString, QSet<int>> pathSegments;
    QSet<QString> unsplitPaths;
    const auto key = [](const QString& type, int index, int partIndex) {
        return QStringLiteral("%1:%2:%3").arg(type).arg(index).arg(partIndex);
    };

    QMap<int, int> partPinIndexes;
    QMap<int, int> commonPinIndexes;
    for (const AltiumSchPin& pin : component.pins) {
        const int partIndex = pin.sourcePartIndex;
        expected.insert(key(QStringLiteral("P"), partPinIndexes[partIndex]++, partIndex));
        if (pin.ownerPartId == -1)
            expected.insert(key(QStringLiteral("P"), commonPinIndexes[-1]++, -1));
    }

    const auto addIndexed = [&expected, &key](const QString& type, int index, int partIndex) {
        if (index >= 0)
            expected.insert(key(type, index, partIndex));
    };
    QSet<QString> rectangleOrderKeys;
    const auto addRectangleOrder = [&expected, &rectangleOrderKeys, &key](int index, int partIndex) {
        if (index < 0)
            return true;
        const QString orderKey = key(QStringLiteral("R"), index, partIndex);
        if (rectangleOrderKeys.contains(orderKey))
            return false;
        rectangleOrderKeys.insert(orderKey);
        expected.insert(orderKey);
        return true;
    };
    for (const AltiumSchRectangle& rect : component.rectangles) {
        if (!addRectangleOrder(rect.sourceGraphicIndex, rect.sourcePartIndex))
            return false;
    }
    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (!addRectangleOrder(rect.sourceGraphicIndex, rect.sourcePartIndex))
            return false;
    }
    for (const AltiumSchEllipse& ellipse : component.ellipses)
        addIndexed(ellipse.sourceGraphicType, ellipse.sourceGraphicIndex, ellipse.sourcePartIndex);
    for (const AltiumSchArc& arc : component.arcs)
        addIndexed(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourcePartIndex);
    for (const AltiumSchPolyline& polyline : component.polylines)
        addIndexed(QStringLiteral("PL"), polyline.sourceGraphicIndex, polyline.sourcePartIndex);
    for (const AltiumSchPolygon& polygon : component.polygons)
        addIndexed(QStringLiteral("PG"), polygon.sourceGraphicIndex, polygon.sourcePartIndex);
    for (const AltiumSchText& text : component.texts) {
        if (!text.isPinLabel)
            addIndexed(QStringLiteral("T"), text.sourceGraphicIndex, text.sourcePartIndex);
    }
    const bool hasImageOrder = std::any_of(
        component.graphicOrder.cbegin(), component.graphicOrder.cend(), [](const AltiumSchGraphicOrder& order) {
            return order.type == QStringLiteral("I");
        });
    if (hasImageOrder) {
        for (const AltiumSchImage& image : component.images)
            addIndexed(QStringLiteral("I"), image.sourceGraphicIndex, image.sourcePartIndex);
    }
    const auto addPathSegment = [&pathSegments, &unsplitPaths, &key](
                                    const QString& type, int index, int segmentIndex, int partIndex) {
        if (type != QStringLiteral("PT") || index < 0)
            return;
        const QString pathKey = key(type, index, partIndex);
        if (segmentIndex < 0)
            unsplitPaths.insert(pathKey);
        else
            pathSegments[pathKey].insert(segmentIndex);
    };
    for (const AltiumSchPath& path : component.paths) {
        addIndexed(path.sourceGraphicType, path.sourceGraphicIndex, path.sourcePartIndex);
        addPathSegment(path.sourceGraphicType, path.sourceGraphicIndex, path.sourceSegmentIndex, path.sourcePartIndex);
    }
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() == 4) {
            addIndexed(bezier.sourceGraphicType, bezier.sourceGraphicIndex, bezier.sourcePartIndex);
            addPathSegment(
                bezier.sourceGraphicType, bezier.sourceGraphicIndex, bezier.sourceSegmentIndex, bezier.sourcePartIndex);
        }
    }
    for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
        addIndexed(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourcePartIndex);
        addPathSegment(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourceSegmentIndex, arc.sourcePartIndex);
    }
    for (const AltiumSchArc& arc : component.arcs)
        addPathSegment(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourceSegmentIndex, arc.sourcePartIndex);

    // 这些图元只有在来源类型和索引同时有效时才能由 writeOrderedGraphic() 写出。
    // 否则应回退到默认顺序，避免来源类型非空但索引缺失的图元被静默丢弃。
    const auto hasUnresolvedOrderedGraphic = [](const auto& graphics) {
        for (const auto& graphic : graphics) {
            if (!graphic.sourceGraphicType.isEmpty() && graphic.sourceGraphicIndex < 0)
                return true;
        }
        return false;
    };
    if (hasUnresolvedOrderedGraphic(component.ellipses) || hasUnresolvedOrderedGraphic(component.arcs) ||
        hasUnresolvedOrderedGraphic(component.paths) || hasUnresolvedOrderedGraphic(component.beziers) ||
        hasUnresolvedOrderedGraphic(component.ellipticalArcs)) {
        return false;
    }

    for (const AltiumSchGraphicOrder& order : component.graphicOrder) {
        if (order.index < 0 || order.type.isEmpty())
            return false;
        const QString orderKey = key(order.type, order.index, order.partIndex);
        if (actual.contains(orderKey))
            return false;
        actual.insert(orderKey);
    }

    QSet<int> emittedPins;
    for (const AltiumSchGraphicOrder& order : component.graphicOrder) {
        if (order.type != QStringLiteral("P"))
            continue;
        int localIndex = 0;
        for (int pinIndex = 0; pinIndex < component.pins.size(); ++pinIndex) {
            const AltiumSchPin& pin = component.pins.at(pinIndex);
            const bool matchesPart =
                order.partIndex < 0 ? pin.ownerPartId == -1 : pin.sourcePartIndex == order.partIndex;
            if (!matchesPart)
                continue;
            if (localIndex == order.index) {
                if (emittedPins.contains(pinIndex))
                    return false;
                emittedPins.insert(pinIndex);
                break;
            }
            ++localIndex;
        }
    }
    if (emittedPins.size() != component.pins.size())
        return false;

    for (const QString& pathKey : unsplitPaths) {
        if (pathSegments.contains(pathKey))
            return false;
    }
    for (auto it = pathSegments.cbegin(); it != pathSegments.cend(); ++it) {
        // 段索引允许出现缺口：前面的来源段可能因几何量化或参数无效被跳过，
        // 但后续有效段仍应按实际索引写出，而不是迫使整个图元回退到分组顺序。
        if (it.value().isEmpty())
            return false;
    }

    for (const QString& expectedKey : expected) {
        if (expectedKey.startsWith(QStringLiteral("P:")))
            continue;
        if (!actual.contains(expectedKey))
            return false;
    }
    for (const QString& actualKey : actual) {
        if (!expected.contains(actualKey))
            return false;
    }
    return true;
}

bool AltiumSchLibWriter::validateGeometry(const AltiumSchComponent& component) {
    const auto hasFinitePoints = [](const QList<QPointF>& points) {
        return std::all_of(points.cbegin(), points.cend(), [](const QPointF& point) {
            return std::isfinite(point.x()) && std::isfinite(point.y());
        });
    };
    const auto reject = [this, &component](const QString& message) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 组件 %1 的%2，已拒绝写入").arg(component.name, message));
        qWarning() << "AltiumSchLibWriter:" << m_diagnostics.constLast();
        return false;
    };

    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (rect.cornerXRadius < 0 || rect.cornerYRadius < 0)
            return reject(QStringLiteral("圆角矩形圆角半径无效"));
    }
    for (const AltiumSchArc& arc : component.arcs) {
        if (arc.radius <= 0)
            return reject(QStringLiteral("圆弧半径无效"));
    }
    for (const AltiumSchEllipse& ellipse : component.ellipses) {
        if (ellipse.radiusX <= 0 || ellipse.radiusY <= 0)
            return reject(QStringLiteral("椭圆半径无效"));
    }
    for (const AltiumSchPie& pie : component.pies) {
        if (pie.radius <= 0)
            return reject(QStringLiteral("扇形半径无效"));
    }
    for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
        if (arc.radiusX <= 0 || arc.radiusY <= 0)
            return reject(QStringLiteral("椭圆弧半径无效"));
    }
    for (const AltiumSchPolygon& polygon : component.polygons) {
        if (polygon.vertices.size() < 3)
            return reject(QStringLiteral("多边形顶点数量不足"));
        if (!hasFinitePoints(polygon.vertices))
            return reject(QStringLiteral("多边形顶点包含非有限坐标"));
    }
    for (const AltiumSchPolyline& polyline : component.polylines) {
        if (polyline.vertices.size() < 2)
            return reject(QStringLiteral("折线顶点数量不足"));
        if (!hasFinitePoints(polyline.vertices))
            return reject(QStringLiteral("折线顶点包含非有限坐标"));
    }
    for (const AltiumSchPath& path : component.paths) {
        if (path.vertices.size() < 2)
            return reject(QStringLiteral("路径顶点数量不足"));
        if (!hasFinitePoints(path.vertices))
            return reject(QStringLiteral("路径顶点包含非有限坐标"));
    }
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() == 4 && !hasFinitePoints(bezier.controlPoints))
            return reject(QStringLiteral("Bézier 控制点包含非有限坐标"));
    }
    for (const AltiumSchImage& image : component.images) {
        if (!std::isfinite(image.rotation))
            return reject(QStringLiteral("图片旋转角度无效"));
    }
    return true;
}

bool AltiumSchLibWriter::validatePartOwnership(const AltiumSchComponent& component) {
    const int partCount = qMax(1, component.partCount);
    const auto isOutOfRange = [partCount](int ownerPartId) { return ownerPartId < -1 || ownerPartId > partCount; };
    const auto reject = [this, &component](const QString& context, int ownerPartId) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 组件 %1 的%2 OWNERPARTID=%3 超出部件范围，已拒绝写入")
                                 .arg(component.name, context)
                                 .arg(ownerPartId));
        qWarning() << "AltiumSchLibWriter:" << m_diagnostics.constLast();
        return false;
    };

    const auto validate = [&isOutOfRange, &reject](const auto& objects, const QString& context) {
        for (const auto& object : objects) {
            if (isOutOfRange(object.ownerPartId))
                return reject(context, object.ownerPartId);
        }
        return true;
    };

    return validate(component.pins, QStringLiteral("引脚")) &&
           validate(component.rectangles, QStringLiteral("矩形图元")) &&
           validate(component.roundRectangles, QStringLiteral("圆角矩形图元")) &&
           validate(component.lines, QStringLiteral("线段图元")) &&
           validate(component.arcs, QStringLiteral("圆弧图元")) &&
           validate(component.polygons, QStringLiteral("多边形图元")) &&
           validate(component.ellipses, QStringLiteral("椭圆图元")) &&
           validate(component.pies, QStringLiteral("扇形图元")) &&
           validate(component.ellipticalArcs, QStringLiteral("椭圆弧图元")) &&
           validate(component.polylines, QStringLiteral("折线图元")) &&
           validate(component.paths, QStringLiteral("路径图元")) &&
           validate(component.beziers, QStringLiteral("Bézier 图元")) &&
           validate(component.ieeeSymbols, QStringLiteral("IEEE 图元")) &&
           validate(component.texts, QStringLiteral("文本图元")) &&
           validate(component.textFrames, QStringLiteral("文本框图元")) &&
           validate(component.images, QStringLiteral("图片图元")) &&
           validate(component.parameters, QStringLiteral("参数"));
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
    writer.writeInt16(static_cast<int16_t>(normalizeOwnerPartId(pin.ownerPartId, QStringLiteral("引脚"))));
    // OwnerPartId 为 -1 表示公共 Part Zero。
    // 当前每个符号只有一个显示模式；与文本图元的 OWNERPARTDISPLAYMODE=1 保持一致。
    writer.writeUInt8(1);  // OwnerPartDisplayMode

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
    // 二进制引脚没有文本形式的 IndexInSheet，但仍占用共享内容记录序号。
    ++m_nextIndexInSheet;
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
    const double startAngle = normalizeFiniteAngle(arc.startAngle, 0.0, QStringLiteral("圆弧起始角度"));
    const double endAngle = normalizeFiniteAngle(arc.endAngle, 360.0, QStringLiteral("圆弧结束角度"));
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
    if (startAngle != 0.0)
        params["StartAngle"] = QString::number(startAngle, 'f', 3);
    params["EndAngle"] = QString::number(endAngle, 'f', 3);
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
    const double startAngle = normalizeFiniteAngle(pie.startAngle, 0.0, QStringLiteral("扇形起始角度"));
    const double endAngle = normalizeFiniteAngle(pie.endAngle, 360.0, QStringLiteral("扇形结束角度"));
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
    if (startAngle != 0.0)
        params["StartAngle"] = QString::number(startAngle, 'f', 3);
    params["EndAngle"] = QString::number(endAngle, 'f', 3);
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
    const double startAngle = normalizeFiniteAngle(arc.startAngle, 0.0, QStringLiteral("椭圆弧起始角度"));
    const double endAngle = normalizeFiniteAngle(arc.endAngle, 360.0, QStringLiteral("椭圆弧结束角度"));
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
    if (startAngle != 0.0)
        params["StartAngle"] = QString::number(startAngle, 'f', 3);
    params["EndAngle"] = QString::number(endAngle, 'f', 3);
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
    if (bezier.controlPoints.size() != 4) {
        m_diagnostics.append(QStringLiteral("Altium SchLib Bézier 图元控制点数量无效（数量为 %1），已跳过")
                                 .arg(bezier.controlPoints.size()));
        return;
    }

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

    if (text.text.trimmed().isEmpty()) {
        const QString diagnostic = QStringLiteral("Altium SchLib 文本内容为空，仍保留记录以维持记录计数");
        m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }

    const bool hasValidFontSize = std::isfinite(text.fontSizeMm) && text.fontSizeMm > 0.0;
    if (text.fontSizeMm != 0.0 && !hasValidFontSize) {
        const QString diagnostic = QStringLiteral("Altium SchLib 文本字体大小无效，已回退为默认字体大小");
        m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
    }

    if (text.orientation != 0)
        params["Orientation"] = QString::number(text.orientation);
    addColorParam(params, "Color", text.color);
    int fontId = text.fontId;
    if (!text.fontName.isEmpty() || hasValidFontSize || fontId <= 0) {
        constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;
        const QString fontName = text.fontName.isEmpty() ? QStringLiteral("Times New Roman") : text.fontName;
        const int fontSize = hasValidFontSize ? qMax(1, qRound(text.fontSizeMm / MILLIMETERS_PER_POINT)) : 10;
        fontId = getOrAddFont(fontName, fontSize, text.bold, text.italic);
    } else if (fontId < 1 || fontId > m_fonts.size())
        fontId = 1;
    params["FontID"] = QString::number(fontId);
    params["Text"] = text.text;
    if (text.isHidden || !text.isDisplayed)
        params["IsHidden"] = "T";
    if (!text.anchor.isEmpty())
        params["TextAnchor"] = text.anchor;
    if (hasValidFontSize)
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
    params["FontID"] = QString::number(frame.fontId >= 1 && frame.fontId <= m_fonts.size() ? frame.fontId : 1);
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
    if (image.rotation != 0.0)
        params["Rotation"] = QString::number(image.rotation, 'f', 3);
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
    const bool isEmbeddedImage = image.embedImage;
    const QString storageFileName = isEmbeddedImage ? m_embeddedImageNames.value(&image) : image.fileName;
    const bool hasEmbeddedImage = isEmbeddedImage && !image.data.isEmpty() && !storageFileName.isEmpty() &&
                                  storageFileName.toLocal8Bit().size() <= 255;
    if (hasEmbeddedImage)
        params["EmbedImage"] = "T";
    if (!isEmbeddedImage && !storageFileName.isEmpty())
        params["FileName"] = storageFileName;
    else if (hasEmbeddedImage)
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
            if (!image.embedImage) {
                if (image.fileName.trimmed().isEmpty()) {
                    const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的外部文件名为空，已跳过文件引用")
                                                   .arg(component.name)
                                                   .arg(imageIndex);
                    m_diagnostics.append(diagnostic);
                    qWarning() << "AltiumSchLibWriter:" << diagnostic;
                }
                continue;
            }

            QString sourceName = image.fileName;
            sourceName.replace('\\', '/');
            const QString embeddedName = QFileInfo(sourceName).fileName();
            if (image.data.isEmpty()) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入数据为空，已跳过 Storage")
                                               .arg(component.name)
                                               .arg(imageIndex);
                m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }
            if (!AltiumWriterUtils::isValidImageStorageName(embeddedName)) {
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
        const int ownerPartId = normalizeOwnerPartId(field.ownerPartId, QStringLiteral("参数"));
        parameterParams["OWNERPARTID"] = QString::number(ownerPartId);
        if (ownerPartId >= 1) {
            parameterParams["OWNERPARTDISPLAYMODE"] = "1";
            addContentIndex(parameterParams);
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
        int fontId = field.fontId;
        if (std::isfinite(field.fontSizeMm) && field.fontSizeMm > 0.0) {
            constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;
            fontId = getOrAddFont(QStringLiteral("Times New Roman"),
                                  qMax(1, qRound(field.fontSizeMm / MILLIMETERS_PER_POINT)));
        } else if (fontId < 1 || fontId > m_fonts.size()) {
            fontId = 1;
        }
        parameterParams["FONTID"] = QString::number(fontId);
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
        writer.writeCStringParameterBlockUtf8(params);
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
            writer.writeCStringParameterBlockUtf8(params);
        }

        // RECORD=46: MapDefinerList（容器）
        {
            QMap<QString, QString> params;
            params["RECORD"] = "46";
            writer.writeCStringParameterBlockUtf8(params);
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
            writer.writeCStringParameterBlockUtf8(pinMappingParams);
        }

        // RECORD=48: ImplementationParameters
        {
            QMap<QString, QString> params;
            params["RECORD"] = "48";
            for (auto it = impl.parameters.constBegin(); it != impl.parameters.constEnd(); ++it)
                params[it.key()] = it.value();
            writer.writeCStringParameterBlockUtf8(params);
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
 * @brief 向内容记录添加共享的 IndexInSheet 序号
 * @param params 参数映射（输出）
 * @details 首条内容记录的索引 0 由 Altium 隐含表示，因此不写出字段。
 */
void AltiumSchLibWriter::addContentIndex(QMap<QString, QString>& params) {
    if (m_nextIndexInSheet != 0)
        params["IndexInSheet"] = QString::number(m_nextIndexInSheet);
    ++m_nextIndexInSheet;
}

/**
 * @brief 向参数映射中添加 OwnerPartId 和 ISNOTACCESIBLE 字段
 * @param params 参数映射（输出）
 * @param ownerPartId 所属部件 ID
 */
int AltiumSchLibWriter::normalizeOwnerPartId(int ownerPartId, const QString& context) {
    if (ownerPartId == 0 || ownerPartId < -1 || ownerPartId > 32767) {
        const int normalized = ownerPartId < 0 ? -1 : qBound(1, ownerPartId, 32767);
        const QString diagnostic = QStringLiteral("Altium SchLib %1 OWNERPARTID=%2 无效，已规范化为 %3")
                                       .arg(context)
                                       .arg(ownerPartId)
                                       .arg(normalized);
        m_diagnostics.append(diagnostic);
        qWarning() << "AltiumSchLibWriter:" << diagnostic;
        return normalized;
    }
    return ownerPartId;
}

double AltiumSchLibWriter::normalizeFiniteAngle(double angle, double fallback, const QString& context) {
    if (std::isfinite(angle))
        return angle;

    const QString diagnostic =
        QStringLiteral("Altium SchLib %1无效，已规范化为 %2 度").arg(context).arg(fallback, 0, 'f', 3);
    m_diagnostics.append(diagnostic);
    qWarning() << "AltiumSchLibWriter:" << diagnostic;
    return fallback;
}

void AltiumSchLibWriter::addOwnerParams(QMap<QString, QString>& params, int ownerPartId) {
    params["ISNOTACCESIBLE"] = "T";
    addContentIndex(params);
    // 当前每个符号只有一个显示模式；真实 SchLib 样本使用从 1 开始的显示模式编号。
    params["OWNERPARTDISPLAYMODE"] = "1";
    params["OWNERPARTID"] = QString::number(normalizeOwnerPartId(ownerPartId, QStringLiteral("图元")));
}

}  // namespace EasyKiConverter
