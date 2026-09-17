#include "AltiumSchLibWriter.h"

#include "AltiumSchGraphicOrderWriter.h"
#include "AltiumSchImageRecordWriter.h"
#include "AltiumSchImageStorageEncoder.h"
#include "AltiumSchPinRecordWriter.h"
#include "AltiumSchPrimitiveRecordWriter.h"
#include "AltiumSchTextRecordWriter.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumWriterUtils.h"

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
    QSet<QString> componentNames;
    for (const AltiumSchComponent& component : components) {
        if (component.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("Altium SchLib 组件名称为空，已拒绝写入"));
            qWarning() << "AltiumSchLibWriter: Refusing to write a component without a name";
            return false;
        }
        if (component.name.toLatin1().size() > 255) {
            const QString diagnostic =
                QStringLiteral("Altium SchLib 组件 %1 名称超过 255 字节，已拒绝写入").arg(component.name);
            m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            return false;
        }
        if (QString::fromLatin1(component.name.toLatin1()) != component.name) {
            const QString diagnostic =
                QStringLiteral("Altium SchLib 组件名称包含无法编码的字符: %1，已拒绝写入").arg(component.name);
            m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            return false;
        }
        const auto validateParameterValue = [this, &component](const QString& value, const QString& context) {
            if (!value.contains(QChar('|')) && !value.contains(QChar::Null))
                return true;
            const QString diagnostic = QStringLiteral("Altium SchLib 组件 %1 的%2包含参数分隔符或 NUL，已拒绝写入")
                                           .arg(component.name, context);
            m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            return false;
        };
        if (!validateParameterValue(component.name, QStringLiteral("组件名称")) ||
            !validateParameterValue(component.description, QStringLiteral("组件描述")) ||
            !validateParameterValue(component.designatorPrefix, QStringLiteral("位号前缀")))
            return false;
        for (const QString& alias : component.aliases) {
            if (!validateParameterValue(alias, QStringLiteral("组件别名")))
                return false;
        }
        for (const AltiumSchPin& pin : component.pins) {
            if (!validateParameterValue(pin.name, QStringLiteral("引脚名称")) ||
                !validateParameterValue(pin.designator, QStringLiteral("引脚编号")))
                return false;
        }
        for (const AltiumSchText& text : component.texts) {
            if (!validateParameterValue(text.text, QStringLiteral("文本内容")) ||
                !validateParameterValue(text.fontName, QStringLiteral("文本字体名称")) ||
                !validateParameterValue(text.anchor, QStringLiteral("文本对齐锚点")))
                return false;
        }
        for (const AltiumSchTextFrame& frame : component.textFrames) {
            if (!validateParameterValue(frame.text, QStringLiteral("文本框内容")) ||
                !validateParameterValue(frame.fontName, QStringLiteral("文本框字体名称")))
                return false;
        }
        for (const AltiumSchImage& image : component.images) {
            // 有效的嵌入图片名称由 prepareImageStorageNames() 负责诊断并跳过；只有
            // 外部引用或嵌入数据为空时，文件名才会直接进入参数块。
            if ((!image.embedImage || image.data.isEmpty()) &&
                !validateParameterValue(image.fileName, QStringLiteral("图片文件名")))
                return false;
        }
        for (const AltiumSchParameter& parameter : component.parameters) {
            if (!validateParameterValue(parameter.value, QStringLiteral("参数值")))
                return false;
        }
        for (const auto& implementation : component.implementations) {
            if (!validateParameterValue(implementation.modelName, QStringLiteral("实现模型名称")) ||
                !validateParameterValue(implementation.modelType, QStringLiteral("实现模型类型")) ||
                !validateParameterValue(implementation.dataFileKind, QStringLiteral("实现数据文件类型")) ||
                !validateParameterValue(implementation.dataFileEntity, QStringLiteral("实现数据文件实体")))
                return false;
            for (auto it = implementation.parameters.cbegin(); it != implementation.parameters.cend(); ++it) {
                if (!validateParameterValue(it.value(), QStringLiteral("实现参数值")))
                    return false;
            }
            for (auto it = implementation.pinMappings.cbegin(); it != implementation.pinMappings.cend(); ++it) {
                if (!validateParameterValue(it.value(), QStringLiteral("引脚映射值")))
                    return false;
            }
        }
        const QString foldedName = component.name.trimmed().toCaseFolded();
        if (componentNames.contains(foldedName)) {
            const QString diagnostic =
                QStringLiteral("Altium SchLib 组件名称重复（不区分大小写）: %1，已拒绝写入").arg(component.name);
            m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            return false;
        }
        componentNames.insert(foldedName);
        if (component.partCount > 32767) {
            const QString diagnostic = QStringLiteral("Altium SchLib 组件 %1 的 partCount 超出支持范围: %2，已拒绝写入")
                                           .arg(component.name)
                                           .arg(component.partCount);
            m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            return false;
        }
        for (const AltiumSchComponent::Implementation& implementation : component.implementations) {
            for (auto it = implementation.parameters.cbegin(); it != implementation.parameters.cend(); ++it) {
                if (it.key().trimmed().isEmpty() || it.key().contains(QChar('|')) || it.key().contains(QChar::Null)) {
                    const QString diagnostic = QStringLiteral("Altium SchLib 组件 %1 的实现参数键无效: %2，已拒绝写入")
                                                   .arg(component.name, it.key());
                    m_diagnostics.append(diagnostic);
                    qWarning() << "AltiumSchLibWriter:" << diagnostic;
                    return false;
                }
            }
        }
        const auto validateParameterName = [this, &component](const QString& name, const QString& context) {
            if (!name.trimmed().isEmpty() && !name.contains(QChar('|')) && !name.contains(QChar::Null))
                return true;
            const QString diagnostic = QStringLiteral("Altium SchLib 组件 %1 的%2参数名无效: %3，已拒绝写入")
                                           .arg(component.name, context, name);
            m_diagnostics.append(diagnostic);
            qWarning() << "AltiumSchLibWriter:" << diagnostic;
            return false;
        };
        for (auto it = component.sourceMetadata.cbegin(); it != component.sourceMetadata.cend(); ++it) {
            if (!validateParameterName(it.key(), QStringLiteral("源元数据")))
                return false;
            if (!validateParameterValue(it.value(), QStringLiteral("源元数据值")))
                return false;
        }
        for (const AltiumSchParameter& parameter : component.parameters) {
            if (!validateParameterName(parameter.name, QStringLiteral("参数")))
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
    m_fontRegistry.clear();
    m_embeddedImageNames.clear();
    m_uniqueIdCounter = 0;
    m_nextIndexInSheet = 0;
    m_libraryName = libraryName;

    prepareImageStorageNames(components);

    // 确保有默认字体
    m_fontRegistry.getOrAdd(QStringLiteral("Times New Roman"), 10);
    m_fontRegistry.registerTextFonts(components);

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
    const QList<AltiumModels::FontEntry>& fonts = m_fontRegistry.entries();
    params["FontIdCount"] = QString::number(fonts.size());
    for (int i = 0; i < fonts.size(); ++i) {
        int idx = i + 1;
        params[QString("FontName%1").arg(idx)] = fonts[i].name;
        params[QString("Size%1").arg(idx)] = QString::number(fonts[i].size);
        if (fonts[i].bold)
            params[QString("Bold%1").arg(idx)] = "T";
        if (fonts[i].italic)
            params[QString("Italic%1").arg(idx)] = "T";
        if (fonts[i].underline)
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
    AltiumSchGraphicOrderWriter graphicOrderWriter(*this);
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
            graphicOrderWriter.write(writer, component, order);
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

    const auto addUniqueIndexed = [&expected, &key](const QString& type, int index, int partIndex) {
        if (index < 0)
            return true;
        const QString orderKey = key(type, index, partIndex);
        if (expected.contains(orderKey))
            return false;
        expected.insert(orderKey);
        return true;
    };
    const auto addUniqueNonPathIndexed = [&addUniqueIndexed, &expected, &key](
                                             const QString& type, int index, int partIndex) {
        if (type == QStringLiteral("PT")) {
            if (index >= 0)
                expected.insert(key(type, index, partIndex));
            return true;
        }
        return addUniqueIndexed(type, index, partIndex);
    };
    for (const AltiumSchRectangle& rect : component.rectangles) {
        if (!addUniqueIndexed(QStringLiteral("R"), rect.sourceGraphicIndex, rect.sourcePartIndex))
            return false;
    }
    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (!addUniqueIndexed(QStringLiteral("R"), rect.sourceGraphicIndex, rect.sourcePartIndex))
            return false;
    }
    for (const AltiumSchEllipse& ellipse : component.ellipses) {
        if (!addUniqueNonPathIndexed(ellipse.sourceGraphicType, ellipse.sourceGraphicIndex, ellipse.sourcePartIndex))
            return false;
    }
    for (const AltiumSchArc& arc : component.arcs) {
        if (!addUniqueNonPathIndexed(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourcePartIndex))
            return false;
    }
    for (const AltiumSchPolyline& polyline : component.polylines) {
        if (!addUniqueIndexed(QStringLiteral("PL"), polyline.sourceGraphicIndex, polyline.sourcePartIndex))
            return false;
    }
    for (const AltiumSchPolygon& polygon : component.polygons) {
        if (!addUniqueIndexed(QStringLiteral("PG"), polygon.sourceGraphicIndex, polygon.sourcePartIndex))
            return false;
    }
    for (const AltiumSchText& text : component.texts) {
        if (!text.isPinLabel && !addUniqueIndexed(QStringLiteral("T"), text.sourceGraphicIndex, text.sourcePartIndex))
            return false;
    }
    const bool hasImageOrder = std::any_of(
        component.graphicOrder.cbegin(), component.graphicOrder.cend(), [](const AltiumSchGraphicOrder& order) {
            return order.type == QStringLiteral("I");
        });
    if (hasImageOrder) {
        for (const AltiumSchImage& image : component.images) {
            if (!addUniqueIndexed(QStringLiteral("I"), image.sourceGraphicIndex, image.sourcePartIndex))
                return false;
        }
    }
    const auto addPathSegment = [&pathSegments, &unsplitPaths, &key](
                                    const QString& type, int index, int segmentIndex, int partIndex) {
        if (type != QStringLiteral("PT") || index < 0)
            return true;
        const QString pathKey = key(type, index, partIndex);
        if (segmentIndex < 0)
            unsplitPaths.insert(pathKey);
        else {
            QSet<int>& segments = pathSegments[pathKey];
            if (segments.contains(segmentIndex))
                return false;
            segments.insert(segmentIndex);
        }
        return true;
    };
    for (const AltiumSchPath& path : component.paths) {
        if (path.sourceGraphicIndex >= 0)
            expected.insert(key(path.sourceGraphicType, path.sourceGraphicIndex, path.sourcePartIndex));
        if (!addPathSegment(
                path.sourceGraphicType, path.sourceGraphicIndex, path.sourceSegmentIndex, path.sourcePartIndex))
            return false;
    }
    for (const AltiumSchBezier& bezier : component.beziers) {
        if (bezier.controlPoints.size() == 4) {
            if (bezier.sourceGraphicIndex >= 0)
                expected.insert(key(bezier.sourceGraphicType, bezier.sourceGraphicIndex, bezier.sourcePartIndex));
            if (!addPathSegment(bezier.sourceGraphicType,
                                bezier.sourceGraphicIndex,
                                bezier.sourceSegmentIndex,
                                bezier.sourcePartIndex))
                return false;
        }
    }
    for (const AltiumSchEllipticalArc& arc : component.ellipticalArcs) {
        if (!addUniqueNonPathIndexed(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourcePartIndex))
            return false;
        if (!addPathSegment(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourceSegmentIndex, arc.sourcePartIndex))
            return false;
    }
    for (const AltiumSchArc& arc : component.arcs) {
        if (!addPathSegment(arc.sourceGraphicType, arc.sourceGraphicIndex, arc.sourceSegmentIndex, arc.sourcePartIndex))
            return false;
    }

    // 这些图元只有在来源类型和索引同时有效时才能由 AltiumSchGraphicOrderWriter 写出。
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

// 校验组件中所有图元的有限坐标、尺寸、角度和控制点数量。
bool AltiumSchLibWriter::validateGeometry(const AltiumSchComponent& component) {
    const auto hasFinitePoints = [](const QList<QPointF>& points) {
        // 拒绝包含 NaN 或无穷值的点，避免生成无法被 Altium 读取的记录。
        return std::all_of(points.cbegin(), points.cend(), [](const QPointF& point) {
            return std::isfinite(point.x()) && std::isfinite(point.y());
        });
    };
    const auto reject = [this, &component](const QString& message) {
        m_diagnostics.append(QStringLiteral("Altium SchLib 组件 %1 的%2，已拒绝写入").arg(component.name, message));
        qWarning() << "AltiumSchLibWriter:" << m_diagnostics.constLast();
        return false;
    };
    const auto validateLineWidths = [&reject](const auto& objects, const QString& context) {
        for (const auto& object : objects) {
            if (object.lineWidth < 0 || object.lineWidth > 3)
                return reject(QStringLiteral("%1线宽索引无效: %2").arg(context).arg(object.lineWidth));
        }
        return true;
    };
    const auto validateLineStyles = [&reject](const auto& objects, const QString& context) {
        for (const auto& object : objects) {
            if (object.lineStyle < 0 || object.lineStyle > 2)
                return reject(QStringLiteral("%1线型无效: %2").arg(context).arg(object.lineStyle));
        }
        return true;
    };
    const auto validateShortString = [&reject](const QString& value, const QString& context) {
        const QByteArray encoded = value.toLatin1();
        if (encoded.size() > 255)
            return reject(QStringLiteral("%1超过 255 字节").arg(context));
        if (value.contains(QChar::Null))
            return reject(QStringLiteral("%1包含 NUL 字符").arg(context));
        if (QString::fromLatin1(encoded) != value)
            return reject(QStringLiteral("%1包含无法编码的字符").arg(context));
        return true;
    };

    for (int i = 0; i < component.pins.size(); ++i) {
        const AltiumSchPin& pin = component.pins.at(i);
        if (!validateShortString(pin.name, QStringLiteral("引脚 %1 名称").arg(i)) ||
            !validateShortString(pin.designator, QStringLiteral("引脚 %1 编号").arg(i)))
            return false;
        if (static_cast<uint8_t>(pin.orientation) > 3)
            return reject(QStringLiteral("引脚 %1 方向无效: %2").arg(i).arg(static_cast<uint8_t>(pin.orientation)));
        if (static_cast<uint8_t>(pin.electricalType) > 7)
            return reject(
                QStringLiteral("引脚 %1 电气类型无效: %2").arg(i).arg(static_cast<uint8_t>(pin.electricalType)));
    }

    if (!validateLineWidths(component.rectangles, QStringLiteral("矩形图元")) ||
        !validateLineWidths(component.roundRectangles, QStringLiteral("圆角矩形图元")) ||
        !validateLineWidths(component.lines, QStringLiteral("线段图元")) ||
        !validateLineWidths(component.arcs, QStringLiteral("圆弧图元")) ||
        !validateLineWidths(component.polygons, QStringLiteral("多边形图元")) ||
        !validateLineWidths(component.ellipses, QStringLiteral("椭圆图元")) ||
        !validateLineWidths(component.pies, QStringLiteral("扇形图元")) ||
        !validateLineWidths(component.ellipticalArcs, QStringLiteral("椭圆弧图元")) ||
        !validateLineWidths(component.polylines, QStringLiteral("折线图元")) ||
        !validateLineWidths(component.paths, QStringLiteral("路径图元")) ||
        !validateLineWidths(component.beziers, QStringLiteral("Bézier 图元")) ||
        !validateLineWidths(component.ieeeSymbols, QStringLiteral("IEEE 图元")) ||
        !validateLineWidths(component.textFrames, QStringLiteral("文本框图元")) ||
        !validateLineWidths(component.images, QStringLiteral("图片图元")) ||
        !validateLineStyles(component.rectangles, QStringLiteral("矩形图元")) ||
        !validateLineStyles(component.roundRectangles, QStringLiteral("圆角矩形图元")) ||
        !validateLineStyles(component.lines, QStringLiteral("线段图元")) ||
        !validateLineStyles(component.arcs, QStringLiteral("圆弧图元")) ||
        !validateLineStyles(component.polygons, QStringLiteral("多边形图元")) ||
        !validateLineStyles(component.ellipses, QStringLiteral("椭圆图元")) ||
        !validateLineStyles(component.pies, QStringLiteral("扇形图元")) ||
        !validateLineStyles(component.ellipticalArcs, QStringLiteral("椭圆弧图元")) ||
        !validateLineStyles(component.polylines, QStringLiteral("折线图元")) ||
        !validateLineStyles(component.paths, QStringLiteral("路径图元")) ||
        !validateLineStyles(component.textFrames, QStringLiteral("文本框图元")) ||
        !validateLineStyles(component.images, QStringLiteral("图片图元"))) {
        return false;
    }
    const auto validateOrientation = [&reject](int orientation, const QString& context) {
        if (orientation < 0 || orientation > 3)
            return reject(QStringLiteral("%1方向无效: %2").arg(context).arg(orientation));
        return true;
    };

    for (const AltiumSchIeee& ieee : component.ieeeSymbols) {
        if (!validateOrientation(ieee.orientation, QStringLiteral("IEEE 图元")))
            return false;
        if (ieee.symbol < 0 || ieee.symbol > 34)
            return reject(QStringLiteral("IEEE 图元符号编号无效: %1").arg(ieee.symbol));
        if (ieee.scaleFactor < 1)
            return reject(QStringLiteral("IEEE 图元缩放因子无效: %1").arg(ieee.scaleFactor));
        if (ieee.lineWidth < 0 || ieee.lineWidth > 3)
            return reject(QStringLiteral("IEEE 图元线宽索引无效: %1").arg(ieee.lineWidth));
    }
    for (const AltiumSchText& text : component.texts) {
        if (!validateOrientation(text.orientation, QStringLiteral("文本图元")))
            return false;
    }
    for (const AltiumSchTextFrame& frame : component.textFrames) {
        if (!validateOrientation(frame.orientation, QStringLiteral("文本框图元")))
            return false;
    }
    for (const AltiumSchParameter& parameter : component.parameters) {
        if (!validateOrientation(parameter.orientation, QStringLiteral("参数")))
            return false;
    }

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
    for (const AltiumSchRectangle& rect : component.rectangles) {
        if (rect.locationX == rect.cornerX || rect.locationY == rect.cornerY)
            return reject(QStringLiteral("矩形图元边界尺寸无效"));
    }
    for (const AltiumSchRoundRectangle& rect : component.roundRectangles) {
        if (rect.locationX == rect.cornerX || rect.locationY == rect.cornerY)
            return reject(QStringLiteral("圆角矩形图元边界尺寸无效"));
    }
    for (const AltiumSchLine& line : component.lines) {
        if (line.locationX == line.cornerX && line.locationY == line.cornerY)
            return reject(QStringLiteral("线段图元长度无效"));
    }
    for (const AltiumSchTextFrame& frame : component.textFrames) {
        if (frame.locationX == frame.cornerX || frame.locationY == frame.cornerY)
            return reject(QStringLiteral("文本框边界尺寸无效"));
        if (frame.textMargin < 0)
            return reject(QStringLiteral("文本框文本边距无效: %1").arg(frame.textMargin));
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
        if (bezier.controlPoints.size() != 4)
            return reject(QStringLiteral("Bézier 控制点数量无效"));
        if (!hasFinitePoints(bezier.controlPoints))
            return reject(QStringLiteral("Bézier 控制点包含非有限坐标"));
    }
    for (const AltiumSchImage& image : component.images) {
        if (!std::isfinite(image.rotation))
            return reject(QStringLiteral("图片旋转角度无效"));
        if (image.locationX == image.cornerX || image.locationY == image.cornerY)
            return reject(QStringLiteral("图片边界尺寸无效"));
    }
    return true;
}

// 校验所有图元和参数记录的 OWNERPARTID 是否落在组件部件范围内。
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
    AltiumSchPinRecordWriter pinWriter(*this);
    pinWriter.write(writer, pin);
}

/**
 * @brief 写入矩形记录 (RECORD=14)
 */
void AltiumSchLibWriter::writeRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRectangle& rect) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeRectangle(writer, rect);
}

/**
 * @brief 写入圆角矩形记录 (RECORD=10)
 */
void AltiumSchLibWriter::writeRoundRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRoundRectangle& rect) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeRoundRectangle(writer, rect);
}

/**
 * @brief 写入线段记录 (RECORD=13)
 */
void AltiumSchLibWriter::writeLineRecord(AltiumBinaryWriter& writer, const AltiumSchLine& line) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeLine(writer, line);
}

/**
 * @brief 写入弧线记录 (RECORD=12)
 */
void AltiumSchLibWriter::writeArcRecord(AltiumBinaryWriter& writer, const AltiumSchArc& arc) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeArc(writer, arc);
}

/**
 * @brief 写入多边形记录 (RECORD=7)
 */
void AltiumSchLibWriter::writePolygonRecord(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePolygon(writer, polygon);
}

/**
 * @brief 写入椭圆记录 (RECORD=8)
 */
void AltiumSchLibWriter::writeEllipseRecord(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeEllipse(writer, ellipse);
}

/**
 * @brief 写入扇形记录 (RECORD=9)
 */
void AltiumSchLibWriter::writePieRecord(AltiumBinaryWriter& writer, const AltiumSchPie& pie) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePie(writer, pie);
}

/**
 * @brief 写入椭圆弧记录 (RECORD=11)
 */
void AltiumSchLibWriter::writeEllipticalArcRecord(AltiumBinaryWriter& writer, const AltiumSchEllipticalArc& arc) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeEllipticalArc(writer, arc);
}

/**
 * @brief 写入折线记录 (RECORD=6)
 */
void AltiumSchLibWriter::writePolylineRecord(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePolyline(writer, polyline);
}

/**
 * @brief 写入路径记录（委托给折线记录）
 * @param writer 二进制写入器
 * @param path 路径数据
 */
void AltiumSchLibWriter::writePathRecord(AltiumBinaryWriter& writer, const AltiumSchPath& path) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writePath(writer, path);
}

/**
 * @brief 写入三次 Bézier 曲线记录 (RECORD=5)
 */
void AltiumSchLibWriter::writeBezierRecord(AltiumBinaryWriter& writer, const AltiumSchBezier& bezier) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeBezier(writer, bezier);
}

/**
 * @brief 写入 IEEE 图形记录 (RECORD=3)
 */
void AltiumSchLibWriter::writeIeeeRecord(AltiumBinaryWriter& writer, const AltiumSchIeee& ieee) {
    AltiumSchPrimitiveRecordWriter primitiveWriter(*this);
    primitiveWriter.writeIeee(writer, ieee);
}

/** @brief 将文本记录委托给专用协作者，保持写入器对外行为不变。 */
void AltiumSchLibWriter::writeTextRecord(AltiumBinaryWriter& writer, const AltiumSchText& text) {
    AltiumSchTextRecordWriter textWriter(*this);
    textWriter.writeText(writer, text);
}

/** @brief 将文本框记录委托给专用协作者，保持写入器对外行为不变。 */
void AltiumSchLibWriter::writeTextFrameRecord(AltiumBinaryWriter& writer, const AltiumSchTextFrame& frame) {
    AltiumSchTextRecordWriter textWriter(*this);
    textWriter.writeTextFrame(writer, frame);
}

/** @brief 将图片记录委托给专用协作者，保持主写入器的状态边界不变。 */
void AltiumSchLibWriter::writeImageRecord(AltiumBinaryWriter& writer, const AltiumSchImage& image) {
    AltiumSchImageRecordWriter imageWriter(*this);
    imageWriter.write(writer, image);
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
            if (embeddedName.toLocal8Bit().size() > 255) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入文件名超过 255 字节，已跳过 Storage")
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
    const QByteArray storageData =
        AltiumSchImageStorageEncoder::encode(components, m_embeddedImageNames, m_diagnostics);
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
            fontId = m_fontRegistry.getOrAdd(QStringLiteral("Times New Roman"),
                                             qMax(1, qRound(field.fontSizeMm / MILLIMETERS_PER_POINT)));
        } else if (fontId < 1 || fontId > m_fontRegistry.size()) {
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

// 将非法角度归一化为回退值，并向调用方报告诊断信息。
double AltiumSchLibWriter::normalizeFiniteAngle(double angle, double fallback, const QString& context) {
    if (std::isfinite(angle))
        return angle;

    const QString diagnostic =
        QStringLiteral("Altium SchLib %1无效，已规范化为 %2 度").arg(context).arg(fallback, 0, 'f', 3);
    m_diagnostics.append(diagnostic);
    qWarning() << "AltiumSchLibWriter:" << diagnostic;
    return fallback;
}

// 写入图元通用的可见性、内容索引、显示模式和部件归属参数。
void AltiumSchLibWriter::addOwnerParams(QMap<QString, QString>& params, int ownerPartId) {
    params["ISNOTACCESIBLE"] = "T";
    addContentIndex(params);
    // 当前每个符号只有一个显示模式；真实 SchLib 样本使用从 1 开始的显示模式编号。
    params["OWNERPARTDISPLAYMODE"] = "1";
    params["OWNERPARTID"] = QString::number(normalizeOwnerPartId(ownerPartId, QStringLiteral("图元")));
}

}  // namespace EasyKiConverter
