#include "AltiumPcbLibWriter.h"

#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"
#include "utils/AltiumWriterUtils.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QtEndian>

namespace EasyKiConverter {

/**
 * @brief 统计封装中的图元数量
 */
int AltiumPcbLibWriter::countPrimitives(const AltiumPcbComponent& component) const {
    return component.pads.size() + component.tracks.size() + component.arcs.size() + component.texts.size() +
           component.fills.size() + component.regions.size() + component.bodies.size();
}

/**
 * @brief 添加广字符串并返回索引
 */
int AltiumPcbLibWriter::addWideString(const QString& text) {
    const int existing = m_wideStrings.indexOf(text);
    if (existing >= 0) {
        return existing;
    }
    int idx = m_wideStrings.size();
    m_wideStrings.append(text);
    return idx;
}

/**
 * @brief V7 Layer ID 转换
 */
uint32_t AltiumPcbLibWriter::toV7LayerId(uint8_t layer) const {
    return AltiumLayerMap::toV7LayerId(layer);
}

/**
 * @brief 写入 PcbLib 文件
 */
bool AltiumPcbLibWriter::write(const QList<AltiumPcbComponent>& components,
                               const QString& filePath,
                               const QString& libraryName) {
    if (components.isEmpty()) {
        qWarning() << "AltiumPcbLibWriter: Refusing to write an empty library";
        return false;
    }
    m_wideStrings.clear();

    qDebug() << "AltiumPcbLibWriter::write: components:" << components.size() << "filePath:" << filePath;

    OLECompoundWriter ole;
    if (!ole.create()) {
        qWarning() << "AltiumPcbLibWriter::write: Failed to create OLE compound document";
        return false;
    }

    QStringList names;
    for (const AltiumPcbComponent& component : components) {
        names.append(component.name);
    }
    const QStringList sectionKeys = AltiumWriterUtils::makeUniqueSectionKeys(names);

    // 只输出 PcbLib 核心流。未知或不完整的可选 storage 比缺省更危险，
    // 因为 Altium 会尝试按其声明的版本反序列化它们。
    writeFileHeader(ole);
    writeSectionKeys(ole, components, sectionKeys);
    writeLibraryStorage(ole, components, filePath);

    // 写入每个封装
    for (int i = 0; i < components.size(); ++i) {
        writeFootprintStorage(ole, components[i], sectionKeys[i]);
    }

    bool result = ole.saveToFile(filePath);
    qDebug() << "AltiumPcbLibWriter::write: saveToFile result:" << result << "filePath:" << filePath;
    return result;
}

/**
 * @brief 写入 PcbLib v6 FileHeader 流
 */
void AltiumPcbLibWriter::writeFileHeader(OLECompoundWriter& ole) {
    QByteArray header;
    AltiumBinaryWriter writer(header);

    // 此流的第一个 i32 是版本文本本身的长度，随后是 Pascal short string。
    // 它不是通用 string block，后面也没有版本 double 或随机 ID。
    QByteArray versionStr = "PCB 6.0 Binary Library File";
    writer.writeInt32(versionStr.size());
    writer.writePascalShortString(QString::fromLatin1(versionStr));

    ole.writeStream("FileHeader", header);
}

/**
 * @brief 写入 SectionKeys 流
 */
void AltiumPcbLibWriter::writeSectionKeys(OLECompoundWriter& ole,
                                          const QList<AltiumPcbComponent>& components,
                                          const QStringList& sectionKeys) {
    QByteArray data;
    AltiumBinaryWriter writer(data);

    // 计算需要映射的条目数
    QVector<QPair<QString, QString>> mappings;
    for (int i = 0; i < components.size(); ++i) {
        const AltiumPcbComponent& comp = components[i];
        const QString& key = sectionKeys[i];
        if (key != comp.name) {
            mappings.append({comp.name, key});
        }
    }

    writer.writeUInt32(static_cast<uint32_t>(mappings.size()));
    for (const auto& mapping : mappings) {
        writer.writePascalString(mapping.first);
        writer.writeStringBlock(mapping.second);
    }

    if (!mappings.isEmpty()) {
        ole.writeStream("SectionKeys", data);
    }
}

/**
 * @brief 写入 Library 存储
 */
void AltiumPcbLibWriter::writeLibraryStorage(OLECompoundWriter& ole,
                                             const QList<AltiumPcbComponent>& components,
                                             const QString& filePath) {
    ole.addStorage("Library");

    // Header 流
    QByteArray headerData;
    AltiumBinaryWriter headerWriter(headerData);
    headerWriter.writeUInt32(1);
    ole.writeStream("Library", "Header", headerData);

    // Data 流
    QByteArray libData;
    writeLibraryData(libData, components, filePath);
    ole.writeStream("Library", "Data", libData);

    // Models 存储
    writeModelsStorage(ole, components);

    // Textures 存储（空）
    {
        ole.addStorage("Library", "Textures");
        QByteArray hdrData;
        AltiumBinaryWriter hdr(hdrData);
        hdr.writeUInt32(0);
        ole.writeStream("Library/Textures", "Header", hdrData);
        ole.writeStream("Library/Textures", "Data", QByteArray());
    }

    // ModelsNoEmbed 存储（空）
    {
        ole.addStorage("Library", "ModelsNoEmbed");
        QByteArray hdrData;
        AltiumBinaryWriter hdr(hdrData);
        hdr.writeUInt32(0);
        ole.writeStream("Library/ModelsNoEmbed", "Header", hdrData);
        ole.writeStream("Library/ModelsNoEmbed", "Data", QByteArray());
    }
}

/**
 * @brief 写入 Library/Data 流
 */
void AltiumPcbLibWriter::writeLibraryData(QByteArray& buffer,
                                          const QList<AltiumPcbComponent>& components,
                                          const QString& filePath) {
    AltiumBinaryWriter writer(buffer);

    writer.beginBlock();
    QByteArray metadata = buildLibraryMetadata(filePath).toLatin1();
    metadata.append('\0');
    writer.writeBytes(metadata);
    writer.endBlock();

    // 元件数量
    writer.writeUInt32(static_cast<uint32_t>(components.size()));

    // 每个元件名称
    for (const AltiumPcbComponent& comp : components) {
        writer.writeStringBlock(comp.name);
    }
}

QString AltiumPcbLibWriter::buildLibraryMetadata(const QString& filePath) const {
    auto safeValue = [](QString value) {
        value.replace('|', ' ');
        value.replace('\0', ' ');
        return value;
    };
    auto append = [&safeValue](QStringList& fields, const QString& key, const QString& value) {
        fields.append(key + "=" + safeValue(value));
    };

    QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    absolutePath.replace('/', '\\');
    const QDateTime now = QDateTime::currentDateTime();

    // Altium serializes library settings as a sequence of Board records. We build
    // that profile from typed defaults so it remains reviewable and evolvable
    // instead of embedding an opaque captured blob.
    QList<QStringList> records;
    records.append(QStringList{});
    QStringList& identity = records.last();
    append(identity, "FILENAME", absolutePath);
    append(identity, "KIND", "Protel_Advanced_PCB_Library");
    append(identity, "VERSION", "3.00");
    append(identity, "DATE", QLocale::c().toString(now.date(), "M/d/yyyy"));
    append(identity, "TIME", QLocale::c().toString(now.time(), "h:mm:ss AP"));
    append(identity, "TOPTYPE", "3");
    append(identity, "TOPCONST", "3.500");
    append(identity, "TOPHEIGHT", "0.4mil");
    append(identity, "TOPMATERIAL", "Solder Resist");
    append(identity, "BOTTOMTYPE", "3");
    append(identity, "BOTTOMCONST", "3.500");
    append(identity, "BOTTOMHEIGHT", "0.4mil");
    append(identity, "BOTTOMMATERIAL", "Solder Resist");
    append(identity, "LAYERSTACKSTYLE", "0");

    auto layerName = [](int layer) -> QString {
        if (layer == 1)
            return "Top Layer";
        if (layer >= 2 && layer <= 31)
            return QString("Mid-Layer %1").arg(layer - 1);
        if (layer == 32)
            return "Bottom Layer";
        static const QStringList standardNames = {
            "Top Overlay", "Bottom Overlay", "Top Paste", "Bottom Paste", "Top Solder", "Bottom Solder"};
        if (layer >= 33 && layer <= 38)
            return standardNames[layer - 33];
        if (layer >= 39 && layer <= 54)
            return QString("Internal Plane %1").arg(layer - 38);
        if (layer == 55)
            return "Drill Guide";
        if (layer == 56)
            return "Keep-Out Layer";
        if (layer >= 57 && layer <= 72)
            return QString("Mechanical %1").arg(layer - 56);
        static const QStringList auxiliaryNames = {"Drill Drawing",
                                                   "Multi-Layer",
                                                   "Connections",
                                                   "Background",
                                                   "DRC Error Markers",
                                                   "Selections",
                                                   "Visible Grid 1",
                                                   "Visible Grid 2",
                                                   "Pad Holes",
                                                   "Via Holes"};
        return auxiliaryNames.value(layer - 73, QString("Layer %1").arg(layer));
    };

    for (int layer = 1; layer <= 82; ++layer) {
        if ((layer - 1) % 5 == 0 && layer != 1) {
            records.append(QStringList{});
        }
        QStringList& fields = records.last();
        const QString prefix = QString("LAYER%1").arg(layer);
        append(fields, prefix + "NAME", layerName(layer));
        append(fields, prefix + "PREV", layer == 1 ? "0" : (layer == 32 ? "1" : "0"));
        append(fields, prefix + "NEXT", layer == 1 ? "32" : "0");
        append(fields, prefix + "MECHENABLED", layer == 57 ? "TRUE" : "FALSE");
        append(fields, prefix + "COPTHICK", "1.4mil");
        append(fields, prefix + "DIELTYPE", "0");
        append(fields, prefix + "DIELCONST", "4.800");
        append(fields, prefix + "DIELHEIGHT", "12.6mil");
        append(fields, prefix + "DIELMATERIAL", "FR-4");
    }

    records.append(QStringList{});
    QStringList& grid = records.last();
    append(grid, "BIGVISIBLEGRIDSIZE", "0.000000");
    append(grid, "VISIBLEGRIDSIZE", "0.000000");
    append(grid, "SNAPGRIDSIZE", "50000.000000");
    append(grid, "SNAPGRIDSIZEX", "50000.000000");
    append(grid, "SNAPGRIDSIZEY", "50000.000000");
    append(grid, "ELECTRICALGRIDRANGE", "8mil");
    append(grid, "ELECTRICALGRIDENABLED", "TRUE");
    append(grid, "DISPLAYUNIT", "1");
    append(grid, "CURRENT2D3DVIEWSTATE", "2D");

    QStringList serializedRecords;
    for (const QStringList& record : records) {
        if (!record.isEmpty()) {
            serializedRecords.append(record.join('|'));
        }
    }
    return serializedRecords.join("\r|RECORD=Board|");
}

/**
 * @brief 写入 Models 存储
 */
void AltiumPcbLibWriter::writeModelsStorage(OLECompoundWriter& ole, const QList<AltiumPcbComponent>& components) {
    ole.addStorage("Library", "Models");

    // 收集所有 3D 模型
    QList<AltiumPcbComponent::Model3D> allModels;
    for (const AltiumPcbComponent& comp : components) {
        for (const AltiumPcbComponent::Model3D& model : comp.models) {
            allModels.append(model);
        }
    }

    // Header 流
    QByteArray hdrData;
    AltiumBinaryWriter hdr(hdrData);
    hdr.writeUInt32(static_cast<uint32_t>(allModels.size()));
    ole.writeStream("Library/Models", "Header", hdrData);

    // Data 流（模型元数据）
    QByteArray modelData;
    AltiumBinaryWriter modelWriter(modelData);
    for (const AltiumPcbComponent::Model3D& model : allModels) {
        const QString id = model.id.isEmpty() ? model.name : model.id;
        const QString metadata =
            QString("EMBED=TRUE|MODELSOURCE=Undefined|ID=%1|ROTX=%2|ROTY=%3|ROTZ=%4|DZ=%5|CHECKSUM=0|NAME=%6")
                .arg(id,
                     QString::number(model.rotX, 'f', 6),
                     QString::number(model.rotY, 'f', 6),
                     QString::number(model.rotZ, 'f', 6),
                     QString::number(AltiumCoord::mmToRaw(model.dz)),
                     model.name);
        QByteArray encoded = metadata.toLatin1();
        encoded.append('\0');
        modelWriter.writeInt32(encoded.size());
        modelWriter.writeBytes(encoded);
    }
    ole.writeStream("Library/Models", "Data", modelData);

    // 写入压缩的 STEP 数据（去掉 qCompress 的 4 字节头，Altium 期望原始 zlib 流）
    for (int i = 0; i < allModels.size(); ++i) {
        if (!allModels[i].stepData.isEmpty()) {
            QByteArray compressed = qCompress(allModels[i].stepData, 9);
            ole.writeStream("Library/Models", QString::number(i), compressed.mid(4));
        }
    }
}

/**
 * @brief 写入封装存储
 */
void AltiumPcbLibWriter::writeFootprintStorage(OLECompoundWriter& ole,
                                               const AltiumPcbComponent& component,
                                               const QString& sectionKey) {
    ole.addStorage(sectionKey);
    QString basePath = sectionKey;

    // Header 流（图元数量）
    QByteArray hdrData;
    AltiumBinaryWriter hdr(hdrData);
    hdr.writeUInt32(static_cast<uint32_t>(countPrimitives(component)));
    ole.writeStream(basePath, "Header", hdrData);

    // Parameters 流
    QByteArray paramsData;
    writeFootprintParameters(paramsData, component);
    ole.writeStream(basePath, "Parameters", paramsData);

    // WideStrings 流
    QByteArray wsData;
    writeWideStrings(wsData, component);
    ole.writeStream(basePath, "WideStrings", wsData);

    // Data 流
    QByteArray dataData;
    writeFootprintData(dataData, component);
    ole.writeStream(basePath, "Data", dataData);

    // UniqueIdPrimitiveInformation 流
    ole.addStorage(basePath, "UniqueIdPrimitiveInformation");
    QByteArray uidHdrData;
    AltiumBinaryWriter uidHdr(uidHdrData);
    uidHdr.writeUInt32(static_cast<uint32_t>(countPrimitives(component)));
    ole.writeStream(basePath + "/UniqueIdPrimitiveInformation", "Header", uidHdrData);

    QByteArray uidData;
    writeUniqueIdPrimitiveInformation(uidData, component);
    ole.writeStream(basePath + "/UniqueIdPrimitiveInformation", "Data", uidData);

    // ExtendedPrimitiveInformation 流（仅当有扩展信息时写入）
    if (!component.extendedPrimitives.isEmpty()) {
        ole.addStorage(basePath, "ExtendedPrimitiveInformation");
        QByteArray extHdrData;
        AltiumBinaryWriter extHdr(extHdrData);
        extHdr.writeUInt32(static_cast<uint32_t>(component.extendedPrimitives.size()));
        ole.writeStream(basePath + "/ExtendedPrimitiveInformation", "Header", extHdrData);

        QByteArray extData;
        writeExtendedPrimitiveInformation(extData, component);
        ole.writeStream(basePath + "/ExtendedPrimitiveInformation", "Data", extData);
    }
}

/**
 * @brief 写入封装参数
 */
void AltiumPcbLibWriter::writeFootprintParameters(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    QMap<QString, QString> params;
    params["PATTERN"] = component.name;
    params["HEIGHT"] = QString::number(AltiumCoord::mmToRaw(component.height));
    if (!component.description.isEmpty()) {
        params["DESCRIPTION"] = component.description;
    }
    params["ITEMGUID"] = "";
    params["REVISIONGUID"] = "";

    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入封装数据（二进制图元）
 */
void AltiumPcbLibWriter::writeFootprintData(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    // 写入封装名称
    writer.writeStringBlock(component.name);

    // 写入所有图元
    for (const AltiumPcbPad& pad : component.pads) {
        writePad(writer, pad, 0);
    }
    for (const AltiumPcbTrack& track : component.tracks) {
        writeTrack(writer, track, 0);
    }
    for (const AltiumPcbArc& arc : component.arcs) {
        writeArc(writer, arc, 0);
    }
    for (const AltiumPcbText& text : component.texts) {
        writeText(writer, text, 0);
    }
    for (const AltiumPcbFill& fill : component.fills) {
        writeFill(writer, fill, 0);
    }
    for (const AltiumPcbRegion& region : component.regions) {
        writeRegion(writer, region, 0);
    }
    for (const AltiumPcbComponentBody& body : component.bodies) {
        writeComponentBody(writer, body, 0);
    }
}

/**
 * @brief 写入广字符串流
 */
void AltiumPcbLibWriter::writeWideStrings(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    // 收集所有需要广字符串的文本
    m_wideStrings.clear();
    for (const AltiumPcbText& text : component.texts) {
        addWideString(text.text);
    }

    QMap<QString, QString> params;
    for (int i = 0; i < m_wideStrings.size(); ++i) {
        QString encoded;
        const QString& text = m_wideStrings[i];
        for (int j = 0; j < text.size(); ++j) {
            if (j > 0)
                encoded += ",";
            encoded += QString::number(static_cast<int>(text.at(j).unicode()));
        }
        params[QString("ENCODEDTEXT%1").arg(i)] = encoded;
    }

    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入通用图元头部（13 字节）
 */
void AltiumPcbLibWriter::writeCommonPrimitiveHeader(AltiumBinaryWriter& writer, uint8_t layer, uint16_t flags) {
    writer.writeUInt8(layer);
    writer.writeUInt16(flags);
    // PcbLib 图元不属于已放置的 board component/net/polygon，五个索引均为 -1。
    writer.writeBytes(QByteArray(10, static_cast<char>(0xFF)));
}

/**
 * @brief 编码图元标志位
 * @details 根据图元属性生成 Altium 标志字：
 *   Bit 2 = Unlocked, Bit 3 = Saved, Bit 5 = TentingTop, Bit 6 = TentingBottom, Bit 9 = Keepout
 */
uint16_t AltiumPcbLibWriter::encodePrimitiveFlags(bool isLocked,
                                                  bool isTentingTop,
                                                  bool isTentingBottom,
                                                  bool isKeepout) {
    uint16_t flags = AltiumConstants::PCB_FLAG_SAVED;
    if (!isLocked) {
        flags |= 0x04;  // Unlocked
    }
    if (isTentingTop) {
        flags |= AltiumConstants::PCB_FLAG_TENTING_TOP;
    }
    if (isTentingBottom) {
        flags |= AltiumConstants::PCB_FLAG_TENTING_BOTTOM;
    }
    if (isKeepout) {
        flags |= AltiumConstants::PCB_FLAG_KEEPOUT;
    }
    return flags;
}

/**
 * @brief 写入焊盘记录 (Object ID = 2)
 * @details 完整写入焊盘的主记录和扩展块，包含孔类型、槽孔、圆角等全部属性。
 */
void AltiumPcbLibWriter::writePad(AltiumBinaryWriter& writer, const AltiumPcbPad& pad, int componentIndex) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_PAD);

    // 子记录 1: Designator
    writer.writeStringBlock(pad.designator);

    // 子记录 2: PadSubrecord2（空）
    writer.writeStringBlock("");

    // 子记录 3: Net 字符串
    writer.writeStringBlock("|&|0");

    // 子记录 4: 标记字节
    writer.beginBlock();
    writer.writeUInt8(0);
    writer.endBlock();

    // 子记录 5: 主焊盘数据
    writer.beginBlock();
    {
        uint8_t layer = pad.isSMD ? pad.layer : AltiumConstants::PCB_LAYER_MULTI;
        uint16_t flags = encodePrimitiveFlags(pad.isLocked, pad.isTentingTop, pad.isTentingBottom, pad.isKeepout);
        writeCommonPrimitiveHeader(writer, layer, flags);

        // 位置
        writer.writeInt32(pad.locationX);
        writer.writeInt32(pad.locationY);

        // 三层尺寸
        writer.writeInt32(pad.sizeTopX);
        writer.writeInt32(pad.sizeTopY);
        writer.writeInt32(pad.sizeMidX);
        writer.writeInt32(pad.sizeMidY);
        writer.writeInt32(pad.sizeBotX);
        writer.writeInt32(pad.sizeBotY);

        // 孔径
        writer.writeInt32(pad.holeSize);

        // 三层形状
        writer.writeUInt8(pad.shapeTop);
        writer.writeUInt8(pad.shapeMid);
        writer.writeUInt8(pad.shapeBot);

        // 旋转 + 电镀
        writer.writeDouble(pad.rotation);
        writer.writeUInt8(pad.isPlated ? 1 : 0);

        // 补齐主记录固定布局（61 字节之后的字段）
        writer.writeUInt8(0);  // stack mode = Simple
        writer.writeUInt8(pad.mode);
        writer.writeUInt8(pad.powerPlaneConnectStyle);
        writer.writeInt32(pad.reliefAirGapRaw);
        writer.writeInt32(pad.reliefConductorWidthRaw);
        writer.writeInt16(pad.reliefEntries);
        writer.writeInt32(pad.powerPlaneClearanceRaw);
        writer.writeInt32(pad.powerPlaneReliefExpansionRaw);
        writer.writeInt32(0);  // reserved
        writer.writeInt32(pad.pasteMaskExpansionRaw);
        writer.writeInt32(pad.solderMaskExpansionRaw);
        writer.writeBytes(QByteArray(7, 0));  // reserved
        writer.writeUInt8(pad.pasteMaskExpansionRaw != 0 ? 2 : 0);  // paste mask expansion mode
        writer.writeUInt8(pad.solderMaskExpansionRaw != 0 ? 2 : 1);  // solder mask expansion mode
        writer.writeUInt8(pad.drillType);
        writer.writeInt16(0);  // reserved
        writer.writeInt32(0);  // reserved
        writer.writeInt16(0);  // jumper ID
        writer.writeInt16(0);  // reserved
    }
    writer.endBlock();

    // 子记录 6: 扩展块（各层尺寸/形状覆盖 + 孔元数据）
    writer.beginBlock();
    writePadExtendedBlock(writer, pad);
    writer.endBlock();
}

/**
 * @brief 写入焊盘扩展块
 * @details 包含各层尺寸/形状覆盖、孔形状/槽孔/旋转、圆角半径等。
 *          布局：29层尺寸X(116) + 29层尺寸Y(116) + 29层形状及保留位(30) +
 *                孔元数据(13) + 两组32层保留坐标(256) +
 *                圆角标记(1) + 32层形状(32) + 32层圆角(32) = 596
 */
void AltiumPcbLibWriter::writePadExtendedBlock(AltiumBinaryWriter& writer, const AltiumPcbPad& pad) {
    // 尺寸 X 覆盖：29 层 × 4 字节 = 116 字节
    for (int i = 0; i < AltiumConstants::PCB_PAD_LAYER_COUNT; ++i) {
        writer.writeInt32(pad.sizeMidX);
    }
    // 尺寸 Y 覆盖：29 层 × 4 字节 = 116 字节
    for (int i = 0; i < AltiumConstants::PCB_PAD_LAYER_COUNT; ++i) {
        writer.writeInt32(pad.sizeMidY);
    }
    // 形状覆盖：29 个铜层，随后一个保留字节
    for (int i = 0; i < AltiumConstants::PCB_PAD_LAYER_COUNT; ++i) {
        writer.writeUInt8(pad.shapeMid);
    }
    writer.writeUInt8(0);

    // 孔元数据
    writer.writeUInt8(pad.holeType);
    writer.writeInt32(pad.holeSlotLengthRaw);
    writer.writeDouble(pad.holeRotation);

    // 保留区域
    writer.writeBytes(QByteArray(32 * 4, 0));
    writer.writeBytes(QByteArray(32 * 4, 0));

    // 圆角矩形标记
    bool hasRoundedRect = (pad.shapeTop == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT) ||
                          (pad.shapeMid == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT) ||
                          (pad.shapeBot == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT);
    writer.writeUInt8(hasRoundedRect ? 1 : 0);

    // 各层形状列表（32 字节：top + 30*mid + bot）
    writer.writeUInt8(pad.shapeTop);
    for (int i = 0; i < 30; ++i) {
        writer.writeUInt8(pad.shapeMid);
    }
    writer.writeUInt8(pad.shapeBot);

    // 各层圆角半径百分比（32 字节）
    for (int i = 0; i < 32; ++i) {
        writer.writeUInt8(pad.cornerRadiusPercentage);
    }
}

/**
 * @brief 写入走线记录 (Object ID = 4)
 */
void AltiumPcbLibWriter::writeTrack(AltiumBinaryWriter& writer, const AltiumPcbTrack& track, int componentIndex) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);

    writer.beginBlock();
    {
        uint16_t flags = encodePrimitiveFlags(false, false, false, false);
        writeCommonPrimitiveHeader(writer, track.layer, flags);

        writer.writeInt32(track.startX);
        writer.writeInt32(track.startY);
        writer.writeInt32(track.endX);
        writer.writeInt32(track.endY);
        writer.writeInt32(track.width);

        writer.writeUInt16(track.netIndex);
        writer.writeUInt8(static_cast<uint8_t>(qBound(0, componentIndex, 255)));
    }
    writer.endBlock();
}

/**
 * @brief 写入弧线记录 (Object ID = 1)
 */
void AltiumPcbLibWriter::writeArc(AltiumBinaryWriter& writer, const AltiumPcbArc& arc, int componentIndex) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_ARC);

    writer.beginBlock();
    {
        uint16_t flags = encodePrimitiveFlags(false, false, false, false);
        writeCommonPrimitiveHeader(writer, arc.layer, flags);

        writer.writeInt32(arc.centerX);
        writer.writeInt32(arc.centerY);
        writer.writeInt32(arc.radius);
        writer.writeDouble(arc.startAngle);
        writer.writeDouble(arc.endAngle);
        writer.writeInt32(arc.width);
    }
    writer.endBlock();
}

/**
 * @brief 写入文本记录 (Object ID = 5)
 */
void AltiumPcbLibWriter::writeText(AltiumBinaryWriter& writer, const AltiumPcbText& text, int componentIndex) {
    writer.writeUInt8(5);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;
        writeCommonPrimitiveHeader(writer, text.layer, flags);

        writer.writeInt32(text.locationX);
        writer.writeInt32(text.locationY);
        writer.writeInt32(text.height);
        writer.writeInt16(0);  // font ID
        writer.writeDouble(text.rotation);
        writer.writeUInt8(text.isMirrored ? 1 : 0);
        writer.writeInt32(text.strokeWidth);
        writer.writeUInt8(0);  // is comment
        writer.writeUInt8(0);  // is designator
        writer.writeUInt8(0);  // char set
        writer.writeUInt8(0);  // base font type (Stroke)

        // 剩余填充到文本记录总大小（已写字段精确占 44 字节）。
        constexpr int WRITTEN_TEXT_HEADER_SIZE = 44;
        QByteArray padding(AltiumConstants::PCB_TEXT_RECORD_SIZE - WRITTEN_TEXT_HEADER_SIZE, 0);
        // wide string index
        int wsIdx = addWideString(text.text);
        int wsBase = AltiumConstants::PCB_TEXT_WS_INDEX_FIELD_OFFSET - WRITTEN_TEXT_HEADER_SIZE;
        padding[wsBase] = static_cast<char>(wsIdx & 0xFF);
        padding[wsBase + 1] = static_cast<char>((wsIdx >> 8) & 0xFF);
        padding[wsBase + 2] = static_cast<char>((wsIdx >> 16) & 0xFF);
        padding[wsBase + 3] = static_cast<char>((wsIdx >> 24) & 0xFF);
        // text kind = Stroke
        padding[AltiumConstants::PCB_TEXT_KIND_FIELD_OFFSET - WRITTEN_TEXT_HEADER_SIZE] = 0;
        // V7 layer ID
        uint32_t v7id = toV7LayerId(text.layer);
        int v7Base = AltiumConstants::PCB_TEXT_V7LAYER_FIELD_OFFSET - WRITTEN_TEXT_HEADER_SIZE;
        padding[v7Base] = static_cast<char>(v7id & 0xFF);
        padding[v7Base + 1] = static_cast<char>((v7id >> 8) & 0xFF);
        padding[v7Base + 2] = static_cast<char>((v7id >> 16) & 0xFF);
        padding[v7Base + 3] = static_cast<char>((v7id >> 24) & 0xFF);

        writer.writeBytes(padding);
    }
    writer.endBlock();

    // 文本字符串块
    writer.writeStringBlock(text.text);
}

/**
 * @brief 写入填充记录 (Object ID = 6, 50 字节)
 */
void AltiumPcbLibWriter::writeFill(AltiumBinaryWriter& writer, const AltiumPcbFill& fill, int componentIndex) {
    writer.writeUInt8(6);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;
        writeCommonPrimitiveHeader(writer, fill.layer, flags);

        writer.writeInt32(fill.corner1X);
        writer.writeInt32(fill.corner1Y);
        writer.writeInt32(fill.corner2X);
        writer.writeInt32(fill.corner2Y);
        writer.writeDouble(fill.rotation);
        writer.writeInt32(0);  // solder mask expansion
        writer.writeUInt8(0);  // paste mask expansion
        writer.writeUInt32(toV7LayerId(fill.layer));
        writer.writeUInt8(0);  // keepout restrictions
        writer.writeBytes(QByteArray(3, 0));  // reserved
    }
    writer.endBlock();
}

/**
 * @brief 写入区域记录 (Object ID = 11)
 */
void AltiumPcbLibWriter::writeRegion(AltiumBinaryWriter& writer, const AltiumPcbRegion& region, int componentIndex) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_REGION);

    writer.beginBlock();
    {
        uint16_t flags = encodePrimitiveFlags(false, false, false, false);
        writeCommonPrimitiveHeader(writer, region.layer, flags);

        writer.writeUInt32(0);
        writer.writeUInt8(0);

        // 嵌套参数块
        QMap<QString, QString> params;
        QString v7Name = region.v7LayerName.isEmpty() ? AltiumLayerMap::toLayerName(region.layer) : region.v7LayerName;
        params["V7_LAYER"] = v7Name;
        params["KIND"] = QString::number(region.kind);
        params["SUBPOLYINDEX"] = "0";
        params["UNIONINDEX"] = "0";
        params["ARCRESOLUTION"] = "0mil";
        if (region.isBoardCutout)
            params["ISBOARDCUTOUT"] = "TRUE";
        if (!region.net.isEmpty())
            params["NET"] = region.net;
        if (!region.uniqueId.isEmpty())
            params["UNIQUEID"] = region.uniqueId;
        if (!region.name.isEmpty())
            params["NAME"] = region.name;
        writer.writeCStringParameterBlock(params);

        // 轮廓顶点
        writer.writeUInt32(static_cast<uint32_t>(region.vertices.size()));
        for (const QPointF& v : region.vertices) {
            writer.writeDouble(v.x());
            writer.writeDouble(v.y());
        }
    }
    writer.endBlock();
}

/**
 * @brief 写入 3D 元件体记录 (Object ID = 12)
 */
void AltiumPcbLibWriter::writeComponentBody(AltiumBinaryWriter& writer,
                                            const AltiumPcbComponentBody& body,
                                            int componentIndex) {
    writer.writeUInt8(AltiumConstants::PCB_OBJECT_COMPONENT_BODY);

    writer.beginBlock();
    {
        uint16_t flags = encodePrimitiveFlags(false, false, false, false);
        uint8_t layer = 57;
        const QString normalizedLayer = body.layerName.trimmed().toUpper();
        if (normalizedLayer.startsWith("MECHANICAL")) {
            bool ok = false;
            const int number = normalizedLayer.mid(10).toInt(&ok);
            if (ok && number >= 1 && number <= 16) {
                layer = static_cast<uint8_t>(56 + number);
            }
        }
        writeCommonPrimitiveHeader(writer, layer, flags);

        writer.writeUInt32(0);  // reserved
        writer.writeUInt8(0);  // reserved

        QMap<QString, QString> params;
        params["V7_LAYER"] = body.layerName;
        params["NAME"] = body.name;
        params["KIND"] = QString::number(body.kind);
        params["SUBPOLYINDEX"] = QString::number(body.subpolyIndex);
        params["UNIONINDEX"] = QString::number(body.unionIndex);
        params["ARCRESOLUTION"] = QString::number(body.arcResolutionRaw / 10000.0, 'f', 4) + "mil";
        params["ISSHAPEBASED"] = body.isShapeBased ? "TRUE" : "FALSE";
        params["CAVITYHEIGHT"] = QString::number(body.cavityHeightRaw / 10000.0, 'f', 4) + "mil";
        params["STANDOFFHEIGHT"] = QString::number(body.standoffHeightRaw / 10000.0, 'f', 4) + "mil";
        params["OVERALLHEIGHT"] = QString::number(body.overallHeightRaw / 10000.0, 'f', 4) + "mil";
        params["BODYCOLOR3D"] = QString::number(body.bodyColor3d);
        params["BODYOPACITY3D"] = QString::number(body.bodyOpacity3d, 'f', 3);
        params["BODYPROJECTION"] = QString::number(body.bodyProjection);
        params["IDENTIFIER"] = "";
        params["TEXTURE"] = "";
        params["TEXTURECENTERX"] = "0mil";
        params["TEXTURECENTERY"] = "0mil";
        params["TEXTURESIZEX"] = "0mil";
        params["TEXTURESIZEY"] = "0mil";
        params["TEXTUREROTATION"] = "0.000000";
        params["MODELID"] = body.modelId;
        params["MODEL.CHECKSUM"] = QString::number(body.modelChecksum);
        params["MODEL.EMBED"] = body.modelEmbed ? "TRUE" : "FALSE";
        params["MODEL.NAME"] = body.modelName;
        params["MODEL.2D.X"] = QString::number(body.model2dRotX / 10000.0, 'f', 4) + "mil";
        params["MODEL.2D.Y"] = QString::number(body.model2dRotY / 10000.0, 'f', 4) + "mil";
        params["MODEL.2D.ROTATION"] = QString::number(body.model2dRotation, 'f', 3);
        params["MODEL.3D.ROTX"] = QString::number(body.model3dRotX, 'f', 3);
        params["MODEL.3D.ROTY"] = QString::number(body.model3dRotY, 'f', 3);
        params["MODEL.3D.ROTZ"] = QString::number(body.model3dRotZ, 'f', 3);
        params["MODEL.3D.DZ"] = QString::number(body.model3dDzRaw / 10000.0, 'f', 4) + "mil";
        params["MODEL.MODELTYPE"] = QString::number(body.modelType);
        params["MODEL.MODELSOURCE"] = body.modelSource;
        writer.writeCStringParameterBlock(params);

        // 轮廓顶点
        writer.writeUInt32(static_cast<uint32_t>(body.outline.size()));
        for (const QPointF& v : body.outline) {
            writer.writeDouble(v.x());
            writer.writeDouble(v.y());
        }
    }
    writer.endBlock();
}

/**
 * @brief 写入图元唯一标识信息
 * @details 为每个图元生成 PRIMITIVEOBJECTID 条目，用于 Altium 内部引用追踪。
 */
void AltiumPcbLibWriter::writeUniqueIdPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    auto writeEntry = [&](const char* objectName, int index) {
        QMap<QString, QString> params;
        if (index > 0) {
            params["PRIMITIVEINDEX"] = QString::number(index);
        }
        params["PRIMITIVEOBJECTID"] = QString::fromLatin1(objectName);
        writer.writeCStringParameterBlock(params);
    };

    int idx = 0;
    for (int i = 0; i < component.pads.size(); ++i, ++idx)
        writeEntry("Pad", idx);
    for (int i = 0; i < component.tracks.size(); ++i, ++idx)
        writeEntry("Track", idx);
    for (int i = 0; i < component.arcs.size(); ++i, ++idx)
        writeEntry("Arc", idx);
    for (int i = 0; i < component.texts.size(); ++i, ++idx)
        writeEntry("Text", idx);
    for (int i = 0; i < component.fills.size(); ++i, ++idx)
        writeEntry("Fill", idx);
    for (int i = 0; i < component.regions.size(); ++i, ++idx)
        writeEntry("Region", idx);
    for (int i = 0; i < component.bodies.size(); ++i, ++idx)
        writeEntry("ComponentBody", idx);
}

/**
 * @brief 写入图元扩展信息
 * @details 为需要额外属性的图元（如自定义焊盘遮罩扩展）写入参数块。
 */
void AltiumPcbLibWriter::writeExtendedPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    for (const AltiumPcbExtendedPrimitiveInfo& info : component.extendedPrimitives) {
        QMap<QString, QString> params;
        params["PRIMITIVEINDEX"] = QString::number(info.primitiveIndex);
        params["PRIMITIVEOBJECTID"] = info.objectName;
        for (auto it = info.params.constBegin(); it != info.params.constEnd(); ++it) {
            params[it.key()] = it.value();
        }
        writer.writeCStringParameterBlock(params);
    }
}

}  // namespace EasyKiConverter
