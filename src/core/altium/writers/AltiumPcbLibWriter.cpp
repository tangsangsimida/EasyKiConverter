#include "AltiumPcbLibWriter.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

#include <QRandomGenerator>
#include <QtEndian>

namespace EasyKiConverter {

/**
 * @brief 获取封装的 Section Key
 */
QString AltiumPcbLibWriter::getSectionKey(const QString& name) const {
    if (name.isEmpty()) return "_";
    QString key = name.left(31);
    key.replace('/', '_');
    return key;
}

/**
 * @brief 统计封装中的图元数量
 */
int AltiumPcbLibWriter::countPrimitives(const AltiumPcbComponent& component) const {
    return component.pads.size() + component.tracks.size() + component.arcs.size()
           + component.texts.size() + component.fills.size() + component.regions.size();
}

/**
 * @brief 添加广字符串并返回索引
 */
int AltiumPcbLibWriter::addWideString(const QString& text) {
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
    m_wideStrings.clear();

    OLECompoundWriter ole;
    if (!ole.create()) {
        return false;
    }

    // 写入各流
    writeFileHeader(ole);
    writeSectionKeys(ole, components);
    writeFileVersionInfo(ole);
    writeLibraryStorage(ole, components);

    // 写入每个封装
    for (const AltiumPcbComponent& component : components) {
        writeFootprintStorage(ole, component);
    }

    return ole.saveToFile(filePath);
}

/**
 * @brief 写入 FileHeader 流（53 字节）
 */
void AltiumPcbLibWriter::writeFileHeader(OLECompoundWriter& ole) {
    QByteArray header;
    AltiumBinaryWriter writer(header);

    // 版本字符串块
    QByteArray versionStr = "PCB 6.0 Binary Library File";
    writer.writeUInt32(static_cast<uint32_t>(1 + 1 + versionStr.size()));
    writer.writeUInt8(static_cast<uint8_t>(versionStr.size()));
    writer.writeBytes(versionStr);

    // 版本号 double
    writer.writeDouble(5.01);

    // UniqueId 块
    QString uid;
    for (int i = 0; i < 8; ++i) {
        uid += QChar('A' + QRandomGenerator::global()->bounded(26));
    }
    QByteArray uidBytes = uid.toLatin1();
    writer.writeUInt32(static_cast<uint32_t>(1 + 1 + uidBytes.size()));
    writer.writeUInt8(static_cast<uint8_t>(uidBytes.size()));
    writer.writeBytes(uidBytes);

    ole.writeStream("FileHeader", header);
}

/**
 * @brief 写入 SectionKeys 流
 */
void AltiumPcbLibWriter::writeSectionKeys(OLECompoundWriter& ole,
                                           const QList<AltiumPcbComponent>& components) {
    QByteArray data;
    AltiumBinaryWriter writer(data);

    // 计算需要映射的条目数
    QVector<QPair<QString, QString>> mappings;
    for (const AltiumPcbComponent& comp : components) {
        QString key = getSectionKey(comp.name);
        if (key != comp.name) {
            mappings.append({comp.name, key});
        }
    }

    writer.writeUInt32(static_cast<uint32_t>(mappings.size()));
    for (const auto& mapping : mappings) {
        writer.writeStringBlock(mapping.first);
        writer.writeStringBlock(mapping.second);
    }

    if (!mappings.isEmpty()) {
        ole.writeStream("SectionKeys", data);
    }
}

/**
 * @brief 写入 FileVersionInfo 存储
 */
void AltiumPcbLibWriter::writeFileVersionInfo(OLECompoundWriter& ole) {
    ole.addStorage("FileVersionInfo");

    // Header 流
    QByteArray headerData;
    AltiumBinaryWriter headerWriter(headerData);
    headerWriter.writeUInt32(1);  // record count
    ole.writeStream("FileVersionInfo", "Header", headerData);

    // Data 流
    QByteArray data;
    AltiumBinaryWriter writer(data);
    QMap<QString, QString> params;
    params["UniqueID"] = "EasyKiConverter";
    writer.writeCStringParameterBlock(params);
    ole.writeStream("FileVersionInfo", "Data", data);
}

/**
 * @brief 写入 Library 存储
 */
void AltiumPcbLibWriter::writeLibraryStorage(OLECompoundWriter& ole,
                                              const QList<AltiumPcbComponent>& components) {
    ole.addStorage("Library");

    // Header 流
    QByteArray headerData;
    AltiumBinaryWriter headerWriter(headerData);
    headerWriter.writeUInt32(1);
    ole.writeStream("Library", "Header", headerData);

    // Data 流
    QByteArray libData;
    writeLibraryData(libData, components);
    ole.writeStream("Library", "Data", libData);

    // Models 存储
    writeModelsStorage(ole, components);

    // LayerKindMapping 存储
    {
        ole.addStorage("Library", "LayerKindMapping");
        QByteArray hdrData;
        AltiumBinaryWriter hdr(hdrData);
        hdr.writeUInt32(1);
        ole.writeStream("Library/LayerKindMapping", "Header", hdrData);

        QByteArray mappingData;
        writeLayerKindMapping(mappingData);
        ole.writeStream("Library/LayerKindMapping", "Data", mappingData);
    }

    // PadViaLibrary 存储
    {
        ole.addStorage("Library", "PadViaLibrary");
        QByteArray hdrData;
        AltiumBinaryWriter hdr(hdrData);
        hdr.writeUInt32(0);
        ole.writeStream("Library/PadViaLibrary", "Header", hdrData);

        QByteArray pvData;
        writePadViaLibrary(pvData);
        ole.writeStream("Library/PadViaLibrary", "Data", pvData);
    }

    // ComponentParamsTOC 存储
    {
        ole.addStorage("Library", "ComponentParamsTOC");
        QByteArray hdrData;
        AltiumBinaryWriter hdr(hdrData);
        hdr.writeUInt32(1);
        ole.writeStream("Library/ComponentParamsTOC", "Header", hdrData);

        QByteArray tocData;
        writeComponentParamsToc(tocData, components);
        ole.writeStream("Library/ComponentParamsTOC", "Data", tocData);
    }

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
                                           const QList<AltiumPcbComponent>& components) {
    AltiumBinaryWriter writer(buffer);

    // 库头参数块
    QMap<QString, QString> params;
    params["HEADER"] = "PCB 6.0 Binary Library File";
    params["WEIGHT"] = QString::number(components.size());
    writer.writeCStringParameterBlock(params);

    // 元件数量
    writer.writeUInt32(static_cast<uint32_t>(components.size()));

    // 每个元件名称
    for (const AltiumPcbComponent& comp : components) {
        writer.writeStringBlock(comp.name);
    }
}

/**
 * @brief 写入 Models 存储
 */
void AltiumPcbLibWriter::writeModelsStorage(OLECompoundWriter& ole,
                                             const QList<AltiumPcbComponent>& components) {
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
        QMap<QString, QString> params;
        params["EMBED"] = "TRUE";
        params["NAME"] = model.name;
        params["ROTX"] = QString::number(model.rotX, 'f', 3);
        params["ROTY"] = QString::number(model.rotY, 'f', 3);
        params["ROTZ"] = QString::number(model.rotZ, 'f', 3);
        params["DZ"] = QString::number(model.dz, 'f', 6) + "mil";
        params["CHECKSUM"] = "0";
        params["ID"] = model.name;
        modelWriter.writeCStringParameterBlock(params);
    }
    ole.writeStream("Library/Models", "Data", modelData);

    // 写入压缩的 STEP 数据
    for (int i = 0; i < allModels.size(); ++i) {
        if (!allModels[i].stepData.isEmpty()) {
            QByteArray compressed = qCompress(allModels[i].stepData, 9);
            ole.writeStream("Library/Models", QString::number(i), compressed);
        }
    }
}

/**
 * @brief 写入 LayerKindMapping
 */
void AltiumPcbLibWriter::writeLayerKindMapping(QByteArray& buffer) {
    AltiumBinaryWriter writer(buffer);

    // 格式版本字符串 "1.0" (UTF-16LE)
    QByteArray versionU16;
    versionU16.append('1').append('\0').append('.').append('\0').append('0').append('\0').append('\0').append('\0');
    writer.writeUInt32(static_cast<uint32_t>(versionU16.size()));
    writer.writeBytes(versionU16);

    // 签名（PcbLib 为 0）
    writer.writeUInt32(0);

    // 条目数（PcbLib 为 0）
    writer.writeUInt32(0);
}

/**
 * @brief 写入 PadViaLibrary
 */
void AltiumPcbLibWriter::writePadViaLibrary(QByteArray& buffer) {
    AltiumBinaryWriter writer(buffer);

    QMap<QString, QString> params;
    params["PADVIALIBRARY.LIBRARYID"] = "{00000000-0000-0000-0000-000000000000}";
    params["PADVIALIBRARY.LIBRARYNAME"] = "";
    params["PADVIALIBRARY.DISPLAYUNITS"] = "0";
    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入 ComponentParamsTOC
 */
void AltiumPcbLibWriter::writeComponentParamsToc(QByteArray& buffer,
                                                   const QList<AltiumPcbComponent>& components) {
    AltiumBinaryWriter writer(buffer);

    QString toc;
    for (const AltiumPcbComponent& comp : components) {
        toc += QString("Name=%1|Pad Count=%2|Height=%3|Description=%4\r\n")
                   .arg(comp.name)
                   .arg(comp.pads.size())
                   .arg(QString::number(comp.height, 'f', 6) + "mil")
                   .arg(comp.description);
    }

    QByteArray encoded = toc.toLatin1();
    writer.writeCStringParameterBlockRaw(encoded);
}

/**
 * @brief 写入封装存储
 */
void AltiumPcbLibWriter::writeFootprintStorage(OLECompoundWriter& ole,
                                                const AltiumPcbComponent& component) {
    QString sectionKey = getSectionKey(component.name);
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
}

/**
 * @brief 写入封装参数
 */
void AltiumPcbLibWriter::writeFootprintParameters(QByteArray& buffer,
                                                    const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    QMap<QString, QString> params;
    params["PATTERN"] = component.name;
    params["HEIGHT"] = QString::number(component.height, 'f', 6) + "mil";
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
void AltiumPcbLibWriter::writeFootprintData(QByteArray& buffer,
                                             const AltiumPcbComponent& component) {
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
}

/**
 * @brief 写入广字符串流
 */
void AltiumPcbLibWriter::writeWideStrings(QByteArray& buffer,
                                            const AltiumPcbComponent& component) {
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
            if (j > 0) encoded += ",";
            encoded += QString::number(static_cast<int>(text.at(j).unicode()));
        }
        params[QString("ENCODEDTEXT%1").arg(i)] = encoded;
    }

    writer.writeCStringParameterBlock(params);
}

/**
 * @brief 写入通用图元头部（13 字节）
 */
void AltiumPcbLibWriter::writeCommonPrimitiveHeader(AltiumBinaryWriter& writer,
                                                      uint8_t layer,
                                                      uint16_t flags,
                                                      uint16_t netIndex,
                                                      uint16_t componentIndex) {
    writer.writeUInt8(layer);
    writer.writeUInt16(flags);
    writer.writeUInt16(netIndex);
    writer.writeUInt16(0xFFFF);  // polygon index
    writer.writeUInt16(componentIndex);
    writer.writeUInt32(0xFFFFFFFF);  // reserved
}

/**
 * @brief 写入焊盘记录 (Object ID = 2)
 */
void AltiumPcbLibWriter::writePad(AltiumBinaryWriter& writer,
                                   const AltiumPcbPad& pad,
                                   int componentIndex) {
    // Object ID
    writer.writeUInt8(2);

    // 开始块
    writer.beginBlock();

    // 子记录 1: Designator 字符串块
    writer.writeStringBlock(pad.designator);

    // 子记录 2: PadSubrecord2（空）
    writer.writeStringBlock("");

    // 子记录 3: Net 字符串
    writer.writeStringBlock("|&|0");

    // 子记录 4: 标记字节
    writer.beginBlock();
    writer.writeUInt8(0);
    writer.endBlock();

    // 子记录 5: 主焊盘数据（202 字节）
    writer.beginBlock();
    {
        // 通用头部（13 字节）
        uint8_t layer = pad.isSMD ? pad.layer : 74;  // SMD 用指定层，TH 用 MultiLayer
        uint16_t flags = 0x08;  // FlagSaved
        writeCommonPrimitiveHeader(writer, layer, flags, pad.netIndex,
                                   static_cast<uint16_t>(componentIndex));

        // 位置
        writer.writeInt32(pad.locationX);
        writer.writeInt32(pad.locationY);

        // 顶层尺寸
        writer.writeInt32(pad.sizeTopX);
        writer.writeInt32(pad.sizeTopY);

        // 中间层尺寸
        writer.writeInt32(pad.sizeMidX);
        writer.writeInt32(pad.sizeMidY);

        // 底层尺寸
        writer.writeInt32(pad.sizeBotX);
        writer.writeInt32(pad.sizeBotY);

        // 孔径
        writer.writeInt32(pad.holeSize);

        // 形状
        writer.writeUInt8(pad.shapeTop);
        writer.writeUInt8(pad.shapeMid);
        writer.writeUInt8(pad.shapeBot);

        // 旋转
        writer.writeDouble(pad.rotation);

        // 是否电镀
        writer.writeUInt8(pad.isPlated ? 1 : 0);

        // 剩余字节填充到 202 字节
        // 当前位置应该是 13 + 4*2 + 4*6 + 4 + 3 + 8 + 1 = 69
        // 需要填充 202 - 69 = 133 字节
        QByteArray padding(133, 0);
        padding[1] = 0;  // stack mode = Simple
        padding[40] = 1;  // paste mask expansion mode = Rule
        padding[41] = 1;  // solder mask expansion mode = Rule
        writer.writeBytes(padding);
    }
    writer.endBlock();

    // 子记录 6: 尺寸/形状覆盖数据（596 字节）
    writer.beginBlock();
    {
        QByteArray sizeData(596, 0);

        // 填充顶层尺寸到所有层
        for (int i = 0; i < 29; ++i) {
            int offset = i * 4;
            sizeData[offset] = static_cast<char>(pad.sizeTopX & 0xFF);
            sizeData[offset + 1] = static_cast<char>((pad.sizeTopX >> 8) & 0xFF);
            sizeData[offset + 2] = static_cast<char>((pad.sizeTopX >> 16) & 0xFF);
            sizeData[offset + 3] = static_cast<char>((pad.sizeTopX >> 24) & 0xFF);
        }
        for (int i = 0; i < 29; ++i) {
            int offset = 116 + i * 4;
            sizeData[offset] = static_cast<char>(pad.sizeTopY & 0xFF);
            sizeData[offset + 1] = static_cast<char>((pad.sizeTopY >> 8) & 0xFF);
            sizeData[offset + 2] = static_cast<char>((pad.sizeTopY >> 16) & 0xFF);
            sizeData[offset + 3] = static_cast<char>((pad.sizeTopY >> 24) & 0xFF);
        }

        // 形状
        for (int i = 0; i < 29; ++i) {
            sizeData[232 + i] = static_cast<char>(pad.shapeTop);
        }

        // 孔形状
        sizeData[262] = 0;  // Round

        writer.writeBytes(sizeData);
    }
    writer.endBlock();

    writer.endBlock();  // 结束主块
}

/**
 * @brief 写入走线记录 (Object ID = 4, 49 字节)
 */
void AltiumPcbLibWriter::writeTrack(AltiumBinaryWriter& writer,
                                     const AltiumPcbTrack& track,
                                     int componentIndex) {
    writer.writeUInt8(4);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;  // FlagSaved
        writeCommonPrimitiveHeader(writer, track.layer, flags, track.netIndex,
                                   static_cast<uint16_t>(componentIndex));

        writer.writeInt32(track.startX);
        writer.writeInt32(track.startY);
        writer.writeInt32(track.endX);
        writer.writeInt32(track.endY);
        writer.writeInt32(track.width);

        writer.writeInt16(0);  // sub-poly index
        writer.writeInt32(0);  // solder mask expansion
        writer.writeInt16(0);  // paste mask expansion
        writer.writeUInt32(toV7LayerId(track.layer));
        writer.writeUInt8(0);  // keepout restrictions
        writer.writeBytes(QByteArray(3, 0));  // reserved
    }
    writer.endBlock();
}

/**
 * @brief 写入弧线记录 (Object ID = 1, 60 字节)
 */
void AltiumPcbLibWriter::writeArc(AltiumBinaryWriter& writer,
                                   const AltiumPcbArc& arc,
                                   int componentIndex) {
    writer.writeUInt8(1);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;
        writeCommonPrimitiveHeader(writer, arc.layer, flags, arc.netIndex,
                                   static_cast<uint16_t>(componentIndex));

        writer.writeInt32(arc.centerX);
        writer.writeInt32(arc.centerY);
        writer.writeInt32(arc.radius);
        writer.writeDouble(arc.startAngle);
        writer.writeDouble(arc.endAngle);
        writer.writeInt32(arc.width);

        writer.writeInt16(0);  // sub-poly index
        writer.writeInt32(0);  // solder mask expansion
        writer.writeUInt8(0);  // paste mask expansion
        writer.writeUInt32(toV7LayerId(arc.layer));
        writer.writeUInt8(0);  // keepout restrictions
        writer.writeBytes(QByteArray(3, 0));  // reserved
    }
    writer.endBlock();
}

/**
 * @brief 写入文本记录 (Object ID = 5)
 */
void AltiumPcbLibWriter::writeText(AltiumBinaryWriter& writer,
                                    const AltiumPcbText& text,
                                    int componentIndex) {
    writer.writeUInt8(5);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;
        writeCommonPrimitiveHeader(writer, text.layer, flags, 0xFFFF,
                                   static_cast<uint16_t>(componentIndex));

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

        // 剩余填充到 252 字节（从偏移 44 开始）
        QByteArray padding(252 - (13 + 4*2 + 4 + 2 + 8 + 1 + 4 + 3), 0);
        // wide string index
        int wsIdx = addWideString(text.text);
        padding[115 - 44] = static_cast<char>(wsIdx & 0xFF);
        padding[116 - 44] = static_cast<char>((wsIdx >> 8) & 0xFF);
        padding[117 - 44] = static_cast<char>((wsIdx >> 16) & 0xFF);
        padding[118 - 44] = static_cast<char>((wsIdx >> 24) & 0xFF);
        // text kind = Stroke
        padding[160 - 44] = 0;
        // V7 layer ID
        uint32_t v7id = toV7LayerId(text.layer);
        padding[226 - 44] = static_cast<char>(v7id & 0xFF);
        padding[227 - 44] = static_cast<char>((v7id >> 8) & 0xFF);
        padding[228 - 44] = static_cast<char>((v7id >> 16) & 0xFF);
        padding[229 - 44] = static_cast<char>((v7id >> 24) & 0xFF);

        writer.writeBytes(padding);
    }
    writer.endBlock();

    // 文本字符串块
    writer.writeStringBlock(text.text);
}

/**
 * @brief 写入填充记录 (Object ID = 6, 50 字节)
 */
void AltiumPcbLibWriter::writeFill(AltiumBinaryWriter& writer,
                                    const AltiumPcbFill& fill,
                                    int componentIndex) {
    writer.writeUInt8(6);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;
        writeCommonPrimitiveHeader(writer, fill.layer, flags, fill.netIndex,
                                   static_cast<uint16_t>(componentIndex));

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
void AltiumPcbLibWriter::writeRegion(AltiumBinaryWriter& writer,
                                      const AltiumPcbRegion& region,
                                      int componentIndex) {
    writer.writeUInt8(11);  // Object ID

    writer.beginBlock();
    {
        uint16_t flags = 0x08;
        writeCommonPrimitiveHeader(writer, region.layer, flags, 0xFFFF,
                                   static_cast<uint16_t>(componentIndex));

        writer.writeUInt8(0);  // reserved
        writer.writeUInt16(static_cast<uint16_t>(region.holes.size()));
        writer.writeUInt16(0);  // reserved

        // 嵌套参数块
        QMap<QString, QString> params;
        params["V7_LAYER"] = AltiumLayerMap::toLayerName(region.layer);
        params["KIND"] = QString::number(region.kind);
        params["SUBPOLYINDEX"] = "0";
        params["UNIONINDEX"] = "0";
        params["ARCRESOLUTION"] = "0mil";
        params["ISSHAPEBASED"] = "TRUE";
        params["CAVITYHEIGHT"] = "0mil";
        if (region.isBoardCutout) params["ISBOARDCUTOUT"] = "TRUE";
        writer.writeCStringParameterBlock(params);

        // 轮廓顶点
        writer.writeUInt32(static_cast<uint32_t>(region.vertices.size()));
        for (const QPointF& v : region.vertices) {
            writer.writeDouble(v.x());
            writer.writeDouble(v.y());
        }

        // 孔洞
        for (const QList<QPointF>& hole : region.holes) {
            writer.writeUInt32(static_cast<uint32_t>(hole.size()));
            for (const QPointF& v : hole) {
                writer.writeDouble(v.x());
                writer.writeDouble(v.y());
            }
        }
    }
    writer.endBlock();
}

}  // namespace EasyKiConverter
