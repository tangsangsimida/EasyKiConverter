#include "AltiumPcbLibWriter.h"

#include "AltiumPcbInputValidator.h"
#include "AltiumPcbPrimitiveWriter.h"
#include "utils/AltiumConstants.h"
#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"
#include "utils/AltiumWriterUtils.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QtEndian>

#include <cmath>

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

// 将非有限浮点值回退为可写入数值，并记录诊断信息。
double AltiumPcbLibWriter::normalizeFiniteValue(double value, double fallback, const QString& context) {
    if (std::isfinite(value))
        return value;

    const QString diagnostic =
        QStringLiteral("Altium PcbLib %1无效，已规范化为 %2").arg(context).arg(fallback, 0, 'f', 6);
    m_diagnostics.append(diagnostic);
    qWarning() << "AltiumPcbLibWriter:" << diagnostic;
    return fallback;
}

// 委托独立校验器检查封装输入和输出路径。
bool AltiumPcbLibWriter::validateComponents(const QList<AltiumPcbComponent>& components, const QString& filePath) {
    return AltiumPcbInputValidator::validate(*this, components, filePath);
}

/**
 * @brief 写入 PcbLib 文件
 */
bool AltiumPcbLibWriter::write(const QList<AltiumPcbComponent>& components,
                               const QString& filePath,
                               const QString& libraryName) {
    m_diagnostics.clear();
    if (!validateComponents(components, filePath)) {
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

    if (ole.hasError()) {
        qWarning() << "AltiumPcbLibWriter: Failed to construct OLE document:" << ole.errorString();
        return false;
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

// 构建 Library/Data 流使用的参数元数据，并清理参数分隔符。
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
    const auto safeMetadataValue = [](QString value) {
        value.replace('|', ' ');
        value.replace('\0', ' ');
        value.replace('\r', ' ');
        value.replace('\n', ' ');
        return value;
    };
    for (const AltiumPcbComponent::Model3D& model : allModels) {
        const QString id = safeMetadataValue(model.id.isEmpty() ? model.name : model.id);
        const QString metadata =
            QString("EMBED=TRUE|MODELSOURCE=Undefined|ID=%1|ROTX=%2|ROTY=%3|ROTZ=%4|DZ=%5|CHECKSUM=0|NAME=%6")
                .arg(id,
                     QString::number(normalizeFiniteValue(model.rotX, 0.0, QStringLiteral("3D 模型 X 旋转")), 'f', 6),
                     QString::number(normalizeFiniteValue(model.rotY, 0.0, QStringLiteral("3D 模型 Y 旋转")), 'f', 6),
                     QString::number(normalizeFiniteValue(model.rotZ, 0.0, QStringLiteral("3D 模型 Z 旋转")), 'f', 6),
                     QString::number(
                         AltiumCoord::mmToRaw(normalizeFiniteValue(model.dz, 0.0, QStringLiteral("3D 模型 Z 偏移")))),
                     safeMetadataValue(model.name));
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
    AltiumPcbPrimitiveWriter primitiveWriter(*this);

    // 写入封装名称
    writer.writeStringBlock(component.name);

    // 写入所有图元
    for (const AltiumPcbPad& pad : component.pads) {
        primitiveWriter.writePad(writer, pad);
    }
    for (const AltiumPcbTrack& track : component.tracks) {
        primitiveWriter.writeTrack(writer, track, 0);
    }
    for (const AltiumPcbArc& arc : component.arcs) {
        primitiveWriter.writeArc(writer, arc);
    }
    for (const AltiumPcbText& text : component.texts) {
        primitiveWriter.writeText(writer, text);
    }
    for (const AltiumPcbFill& fill : component.fills) {
        primitiveWriter.writeFill(writer, fill);
    }
    for (const AltiumPcbRegion& region : component.regions) {
        primitiveWriter.writeRegion(writer, region);
    }
    for (const AltiumPcbComponentBody& body : component.bodies) {
        primitiveWriter.writeComponentBody(writer, body);
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

/** @brief 写入元件图元的唯一标识索引表。 */
void AltiumPcbLibWriter::writeUniqueIdPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component) {
    AltiumBinaryWriter writer(buffer);

    // 按图元顺序写入唯一标识信息，确保后续记录可以通过索引稳定关联。
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
