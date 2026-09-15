#include "AltiumPcbLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumConstants.h"

#include <QSet>

#include <cmath>

namespace EasyKiConverter {

namespace {

int primitiveBlockCount(quint8 objectId) {
    switch (objectId) {
        case AltiumConstants::PCB_OBJECT_ARC:
        case AltiumConstants::PCB_OBJECT_VIA:
        case AltiumConstants::PCB_OBJECT_TRACK:
        case AltiumConstants::PCB_OBJECT_FILL:
        case AltiumConstants::PCB_OBJECT_REGION:
        case AltiumConstants::PCB_OBJECT_COMPONENT_BODY:
            return 1;
        case AltiumConstants::PCB_OBJECT_PAD:
            return 6;
        case AltiumConstants::PCB_OBJECT_TEXT:
            return 2;
        default:
            return -1;
    }
}

bool readStringBlockPayload(const QByteArray& payload, QString* value) {
    AltiumBinaryReader reader(payload);
    uint8_t stringSize = 0;
    QByteArray stringData;
    if (!reader.readUInt8(&stringSize) || !reader.readBytes(stringSize, &stringData) || reader.remaining() != 0)
        return false;
    if (value != nullptr)
        *value = QString::fromLatin1(stringData);
    return true;
}

bool isStringBlockPayload(const QByteArray& payload) {
    return readStringBlockPayload(payload, nullptr);
}

bool readCommonPrimitiveHeader(AltiumBinaryReader& reader, quint8* layer, quint16* flags) {
    if (layer == nullptr || flags == nullptr)
        return false;

    QByteArray objectIndexes;
    return reader.readUInt8(layer) && reader.readUInt16(flags) && reader.readBytes(10, &objectIndexes);
}

bool readCommonPrimitiveHeader(const QByteArray& payload, quint8* layer, quint16* flags) {
    AltiumBinaryReader reader(payload);
    return readCommonPrimitiveHeader(reader, layer, flags);
}

bool parseTrackFields(const QByteArray& payload, AltiumPcbLibReader::TrackFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->startX) &&
           reader.readInt32(&fields->startY) && reader.readInt32(&fields->endX) && reader.readInt32(&fields->endY) &&
           reader.readInt32(&fields->width) && reader.readUInt16(&fields->netIndex) &&
           reader.readUInt8(&fields->componentIndex);
}

bool parseArcFields(const QByteArray& payload, AltiumPcbLibReader::ArcFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->centerX) &&
           reader.readInt32(&fields->centerY) && reader.readInt32(&fields->radius) &&
           reader.readDouble(&fields->startAngle) && reader.readDouble(&fields->endAngle) &&
           reader.readInt32(&fields->width);
}

bool parsePadMainFields(const QByteArray& payload, AltiumPcbLibReader::PadFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    QByteArray reserved;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->locationX) &&
           reader.readInt32(&fields->locationY) && reader.readInt32(&fields->sizeTopX) &&
           reader.readInt32(&fields->sizeTopY) && reader.readInt32(&fields->sizeMidX) &&
           reader.readInt32(&fields->sizeMidY) && reader.readInt32(&fields->sizeBotX) &&
           reader.readInt32(&fields->sizeBotY) && reader.readInt32(&fields->holeSize) &&
           reader.readUInt8(&fields->shapeTop) && reader.readUInt8(&fields->shapeMid) &&
           reader.readUInt8(&fields->shapeBot) && reader.readDouble(&fields->rotation) &&
           reader.readUInt8(&fields->isPlated) && reader.readUInt8(&fields->stackMode) &&
           reader.readUInt8(&fields->mode) && reader.readUInt8(&fields->powerPlaneConnectStyle) &&
           reader.readInt32(&fields->reliefAirGapRaw) && reader.readInt32(&fields->reliefConductorWidthRaw) &&
           reader.readInt16(&fields->reliefEntries) && reader.readInt32(&fields->powerPlaneClearanceRaw) &&
           reader.readInt32(&fields->powerPlaneReliefExpansionRaw) &&
           reader.readInt32(&fields->pasteMaskExpansionRaw) && reader.readInt32(&fields->solderMaskExpansionRaw) &&
           reader.readBytes(7, &reserved) && reader.readUInt8(&fields->pasteMaskExpansionMode) &&
           reader.readUInt8(&fields->solderMaskExpansionMode) && reader.readUInt8(&fields->drillType) &&
           reader.readBytes(2, &reserved) && reader.readBytes(4, &reserved) && reader.readBytes(2, &reserved) &&
           reader.readBytes(2, &reserved);
}

bool parsePadExtendedFields(const QByteArray& payload, AltiumPcbLibReader::PadFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    QByteArray reserved;
    qint32 reservedInt32 = 0;
    quint8 reservedByte = 0;
    for (int i = 0; i < 29; ++i) {
        if (!reader.readInt32(&reservedInt32))
            return false;
    }
    for (int i = 0; i < 29; ++i) {
        if (!reader.readInt32(&reservedInt32))
            return false;
    }
    for (int i = 0; i < 29; ++i) {
        if (!reader.readUInt8(&fields->extendedShapeMid))
            return false;
    }
    if (!reader.readUInt8(&reservedByte))
        return false;
    if (!reader.readUInt8(&fields->holeType) || !reader.readInt32(&fields->holeSlotLengthRaw) ||
        !reader.readDouble(&fields->holeRotation) || !reader.readBytes(32 * 4, &reserved) ||
        !reader.readBytes(32 * 4, &reserved) || !reader.readUInt8(&fields->hasRoundedRect) ||
        !reader.readUInt8(&fields->extendedShapeTop))
        return false;
    for (int i = 0; i < 30; ++i) {
        if (!reader.readUInt8(&fields->extendedShapeMid))
            return false;
    }
    if (!reader.readUInt8(&fields->extendedShapeBottom))
        return false;
    for (int i = 0; i < 32; ++i) {
        if (!reader.readUInt8(&fields->cornerRadiusPercentage))
            return false;
    }
    return true;
}

bool readVertexList(AltiumBinaryReader& reader, QVector<QPointF>* vertices) {
    if (vertices == nullptr)
        return false;
    quint32 count = 0;
    if (!reader.readUInt32(&count) || count > static_cast<quint32>(reader.remaining() / 16))
        return false;
    vertices->clear();
    vertices->reserve(static_cast<int>(count));
    for (quint32 i = 0; i < count; ++i) {
        double x = 0.0;
        double y = 0.0;
        if (!reader.readDouble(&x) || !reader.readDouble(&y))
            return false;
        vertices->append(QPointF(x, y));
    }
    return reader.remaining() == 0;
}

bool parseFillFields(const QByteArray& payload, AltiumPcbLibReader::FillFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    QByteArray reserved;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->corner1X) &&
           reader.readInt32(&fields->corner1Y) && reader.readInt32(&fields->corner2X) &&
           reader.readInt32(&fields->corner2Y) && reader.readDouble(&fields->rotation) &&
           reader.readInt32(&fields->solderMaskExpansionRaw) && reader.readUInt8(&fields->pasteMaskExpansion) &&
           reader.readUInt32(&fields->v7LayerId) && reader.readUInt8(&fields->keepoutRestrictions) &&
           reader.readBytes(3, &reserved) && reader.remaining() == 0;
}

bool parseRegionFields(const QByteArray& payload, AltiumPcbLibReader::RegionFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readUInt32(&fields->reserved) &&
           reader.readUInt8(&fields->reservedByte) && reader.readCStringParameterBlock(&fields->parameters) &&
           readVertexList(reader, &fields->vertices);
}

bool parseComponentBodyFields(const QByteArray& payload, AltiumPcbLibReader::ComponentBodyFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readUInt32(&fields->reserved) &&
           reader.readUInt8(&fields->reservedByte) && reader.readCStringParameterBlock(&fields->parameters) &&
           readVertexList(reader, &fields->outline);
}

bool parseTextFields(const QByteArray& payload, AltiumPcbLibReader::TextFields* fields) {
    if (fields == nullptr)
        return false;
    AltiumBinaryReader reader(payload);
    quint8 layer = 0;
    quint16 flags = 0;
    QByteArray padding;
    return readCommonPrimitiveHeader(reader, &layer, &flags) && reader.readInt32(&fields->locationX) &&
           reader.readInt32(&fields->locationY) && reader.readInt32(&fields->height) &&
           reader.readInt16(&fields->fontId) && reader.readDouble(&fields->rotation) &&
           reader.readUInt8(&fields->mirrored) && reader.readInt32(&fields->strokeWidth) &&
           reader.readUInt8(&fields->isComment) && reader.readUInt8(&fields->isDesignator) &&
           reader.readUInt8(&fields->characterSet) && reader.readUInt8(&fields->baseFontType) &&
           reader.readBytes(71, &padding) && reader.readUInt32(&fields->wideStringIndex) &&
           reader.readBytes(41, &padding) && reader.readUInt8(&fields->kind) && reader.readBytes(65, &padding) &&
           reader.readUInt32(&fields->v7LayerId);
}

bool validatePrimitiveFields(const AltiumPcbLibReader::PrimitiveRecord& object, QString* error) {
    auto reject = [error](const QString& message) {
        if (error != nullptr)
            *error = message;
        return false;
    };

    if (object.hasTrackFields || object.hasArcFields || object.hasPadFields || object.hasFillFields ||
        object.hasRegionFields || object.hasComponentBodyFields || object.hasTextFields) {
        if (object.layer < 1 || object.layer > 74)
            return reject(QStringLiteral("PcbLib 图元层号必须在 1..74 范围内"));
    }

    if (object.hasArcFields) {
        if (object.arc.radius <= 0)
            return reject(QStringLiteral("PcbLib 弧线半径必须为正"));
        if (object.arc.width <= 0)
            return reject(QStringLiteral("PcbLib 弧线宽度必须为正"));
        if (!std::isfinite(object.arc.startAngle) || !std::isfinite(object.arc.endAngle))
            return reject(QStringLiteral("PcbLib 弧线角度必须为有限值"));
    }
    if (object.hasTrackFields && object.track.width <= 0)
        return reject(QStringLiteral("PcbLib 走线宽度必须为正"));
    if (object.hasPadFields) {
        const auto& pad = object.pad;
        const auto isValidPadShape = [](quint8 shape) {
            return shape == AltiumConstants::PCB_PAD_SHAPE_ROUND ||
                   shape == AltiumConstants::PCB_PAD_SHAPE_RECTANGULAR ||
                   shape == AltiumConstants::PCB_PAD_SHAPE_OCTAGONAL ||
                   shape == AltiumConstants::PCB_PAD_SHAPE_ROUNDED_RECT;
        };
        if (pad.sizeTopX <= 0 || pad.sizeTopY <= 0 || pad.sizeMidX <= 0 || pad.sizeMidY <= 0 || pad.sizeBotX <= 0 ||
            pad.sizeBotY <= 0)
            return reject(QStringLiteral("PcbLib 焊盘尺寸必须为正"));
        if (!isValidPadShape(pad.shapeTop) || !isValidPadShape(pad.shapeMid) || !isValidPadShape(pad.shapeBot))
            return reject(QStringLiteral("PcbLib 焊盘形状无效"));
        if (pad.holeSize < 0 || pad.holeSlotLengthRaw < 0)
            return reject(QStringLiteral("PcbLib 焊盘孔尺寸不能为负"));
        if (object.layer == AltiumConstants::PCB_LAYER_MULTI && pad.holeSize <= 0)
            return reject(QStringLiteral("PcbLib 通孔焊盘孔径必须为正"));
        if (pad.cornerRadiusPercentage > 100 || pad.mode > 3 || pad.powerPlaneConnectStyle > 2 ||
            (pad.reliefEntries != 2 && pad.reliefEntries != 4) || pad.drillType > 2 || pad.holeType > 2)
            return reject(QStringLiteral("PcbLib 焊盘扩展属性无效"));
        if (pad.holeType == 2 && pad.holeSlotLengthRaw <= 0)
            return reject(QStringLiteral("PcbLib 焊盘槽孔长度必须为正"));
        if (!std::isfinite(pad.rotation) || !std::isfinite(pad.holeRotation))
            return reject(QStringLiteral("PcbLib 焊盘旋转角度必须为有限值"));
    }
    if (object.hasFillFields && !std::isfinite(object.fill.rotation))
        return reject(QStringLiteral("PcbLib 填充旋转角度必须为有限值"));
    if (object.hasTextFields) {
        if (object.textFields.height <= 0 || object.textFields.strokeWidth < 0)
            return reject(QStringLiteral("PcbLib 文本尺寸必须有效"));
        if (!std::isfinite(object.textFields.rotation))
            return reject(QStringLiteral("PcbLib 文本旋转角度必须为有限值"));
    }
    if (object.hasRegionFields) {
        if (object.region.vertices.size() < 3)
            return reject(QStringLiteral("PcbLib 区域至少需要三个顶点"));
        for (const QPointF& vertex : object.region.vertices) {
            if (!std::isfinite(vertex.x()) || !std::isfinite(vertex.y()))
                return reject(QStringLiteral("PcbLib 区域顶点必须为有限值"));
        }
    }
    if (object.hasComponentBodyFields) {
        for (const QPointF& vertex : object.componentBody.outline) {
            if (!std::isfinite(vertex.x()) || !std::isfinite(vertex.y()))
                return reject(QStringLiteral("PcbLib 三维元件体轮廓顶点必须为有限值"));
        }
    }
    return true;
}

}  // namespace

bool AltiumPcbLibReader::fail(const QString& message) {
    m_components.clear();
    m_fileVersion.clear();
    m_libraryMetadata.clear();
    m_errorMessage = message;
    return false;
}

bool AltiumPcbLibReader::open(const QString& filePath) {
    m_components.clear();
    m_fileVersion.clear();
    m_libraryMetadata.clear();
    m_errorMessage.clear();

    if (!m_oleReader.open(filePath))
        return fail(m_oleReader.errorString());

    QByteArray fileHeaderData;
    if (!m_oleReader.readStream(QStringLiteral("FileHeader"), &fileHeaderData))
        return fail(QStringLiteral("PcbLib 缺少 FileHeader 流"));
    AltiumBinaryReader fileHeaderReader(fileHeaderData);
    int32_t versionLength = 0;
    if (!fileHeaderReader.readInt32(&versionLength) || versionLength < 0 || versionLength > 255 ||
        !fileHeaderReader.readPascalShortString(&m_fileVersion) || m_fileVersion.toLatin1().size() != versionLength ||
        fileHeaderReader.remaining() != 0)
        return fail(QStringLiteral("PcbLib FileHeader 无效"));

    QByteArray libraryData;
    if (!m_oleReader.readStream(QStringLiteral("Library/Data"), &libraryData))
        return fail(QStringLiteral("PcbLib 缺少 Library/Data 流"));
    AltiumBinaryReader libraryReader(libraryData);
    if (!libraryReader.readBlock(&m_libraryMetadata))
        return fail(QStringLiteral("PcbLib Library/Data 元数据块无效: %1").arg(libraryReader.errorString()));
    if (!m_libraryMetadata.isEmpty() && m_libraryMetadata.endsWith('\0'))
        m_libraryMetadata.chop(1);

    uint32_t componentCount = 0;
    if (!libraryReader.readUInt32(&componentCount) ||
        componentCount > static_cast<uint32_t>(libraryReader.remaining() / 5))
        return fail(QStringLiteral("PcbLib Library/Data 的封装数量无效"));

    QVector<QString> names;
    names.reserve(static_cast<int>(componentCount));
    QSet<QString> componentNames;
    for (uint32_t i = 0; i < componentCount; ++i) {
        QString name;
        if (!libraryReader.readStringBlock(&name) || name.isEmpty())
            return fail(QStringLiteral("PcbLib Library/Data 的封装名称无效"));
        const QString foldedName = name.toCaseFolded();
        if (componentNames.contains(foldedName))
            return fail(QStringLiteral("PcbLib Library/Data 的封装名称重复: %1").arg(name));
        componentNames.insert(foldedName);
        names.append(name);
    }
    if (libraryReader.hasError() || libraryReader.remaining() != 0)
        return fail(QStringLiteral("PcbLib Library/Data 末尾包含无效数据"));

    QMap<QString, QString> sectionKeys;
    if (m_oleReader.containsStream(QStringLiteral("SectionKeys"))) {
        QByteArray sectionKeyData;
        if (!m_oleReader.readStream(QStringLiteral("SectionKeys"), &sectionKeyData))
            return fail(QStringLiteral("无法读取 PcbLib SectionKeys 流"));
        AltiumBinaryReader sectionKeyReader(sectionKeyData);
        uint32_t keyCount = 0;
        if (!sectionKeyReader.readUInt32(&keyCount) ||
            keyCount > static_cast<uint32_t>(sectionKeyReader.remaining() / 9))
            return fail(QStringLiteral("PcbLib SectionKeys 的数量无效"));
        QSet<QString> mappedComponents;
        QSet<QString> mappedSectionKeys;
        for (uint32_t i = 0; i < keyCount; ++i) {
            QString libRef;
            QString sectionKey;
            if (!sectionKeyReader.readPascalString(&libRef) || !sectionKeyReader.readStringBlock(&sectionKey) ||
                libRef.isEmpty() || sectionKey.isEmpty())
                return fail(QStringLiteral("PcbLib SectionKeys 映射不完整"));
            if (!names.contains(libRef))
                return fail(QStringLiteral("PcbLib SectionKeys 包含未知封装: %1").arg(libRef));
            if (mappedComponents.contains(libRef.toCaseFolded()) ||
                mappedSectionKeys.contains(sectionKey.toCaseFolded()))
                return fail(QStringLiteral("PcbLib SectionKeys 映射重复: %1").arg(libRef));
            mappedComponents.insert(libRef.toCaseFolded());
            mappedSectionKeys.insert(sectionKey.toCaseFolded());
            sectionKeys.insert(libRef, sectionKey);
        }
        if (sectionKeyReader.hasError() || sectionKeyReader.remaining() != 0)
            return fail(QStringLiteral("PcbLib SectionKeys 末尾包含无效数据"));
    }

    QSet<QString> resolvedSectionKeys;
    for (const QString& name : names) {
        const QString sectionKey = sectionKeys.value(name, name);
        if (resolvedSectionKeys.contains(sectionKey.toCaseFolded()))
            return fail(QStringLiteral("PcbLib 封装存储键重复: %1").arg(sectionKey));
        resolvedSectionKeys.insert(sectionKey.toCaseFolded());
        const QString dataPath = sectionKey + QStringLiteral("/Data");
        if (!m_oleReader.containsStream(dataPath))
            return fail(QStringLiteral("PcbLib 缺少封装 Data 流: %1").arg(dataPath));
        m_components.append({name, sectionKey});
    }
    return true;
}

QVector<AltiumPcbLibReader::ComponentInfo> AltiumPcbLibReader::components() const {
    return m_components;
}

QString AltiumPcbLibReader::fileVersion() const {
    return m_fileVersion;
}

QByteArray AltiumPcbLibReader::libraryMetadata() const {
    return m_libraryMetadata;
}

int AltiumPcbLibReader::componentIndex(const QString& componentName) const {
    for (int i = 0; i < m_components.size(); ++i) {
        if (m_components.at(i).name == componentName)
            return i;
    }
    return -1;
}

bool AltiumPcbLibReader::readFootprintStream(int componentIndexValue,
                                             const QString& streamName,
                                             QByteArray* data) const {
    if (data == nullptr || componentIndexValue < 0 || componentIndexValue >= m_components.size() ||
        streamName.isEmpty())
        return false;
    return m_oleReader.readStream(m_components.at(componentIndexValue).sectionKey + "/" + streamName, data);
}

bool AltiumPcbLibReader::readFootprintStream(const QString& componentName,
                                             const QString& streamName,
                                             QByteArray* data) const {
    return readFootprintStream(componentIndex(componentName), streamName, data);
}

bool AltiumPcbLibReader::readFootprintObjects(int componentIndexValue, QVector<PrimitiveRecord>* objects) const {
    if (objects == nullptr) {
        m_errorMessage = QStringLiteral("读取 PcbLib 图元时输出容器为空");
        return false;
    }
    objects->clear();
    m_errorMessage.clear();

    const auto failRead = [this, objects](const QString& message) {
        objects->clear();
        m_errorMessage = message;
        return false;
    };

    QByteArray data;
    if (!readFootprintStream(componentIndexValue, QStringLiteral("Data"), &data))
        return failRead(QStringLiteral("无法读取 PcbLib 封装 Data 流"));

    AltiumBinaryReader reader(data);
    QString footprintName;
    if (!reader.readStringBlock(&footprintName) || footprintName.isEmpty())
        return failRead(QStringLiteral("PcbLib 封装 Data 首部无效: %1").arg(reader.errorString()));

    while (reader.remaining() > 0) {
        const int objectStart = reader.position();
        uint8_t objectId = 0;
        if (!reader.readUInt8(&objectId)) {
            return failRead(QStringLiteral("读取 PcbLib 图元 ID 失败: %1").arg(reader.errorString()));
        }
        const int blockCount = primitiveBlockCount(objectId);
        if (blockCount < 0)
            return failRead(QStringLiteral("PcbLib 图元 ID %1 不受支持").arg(objectId));

        PrimitiveRecord object;
        object.objectId = objectId;
        object.blocks.reserve(blockCount);
        for (int blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
            const int blockStart = reader.position();
            QByteArray payload;
            uint8_t flags = 0;
            if (!reader.readBlock(&payload, &flags))
                return failRead(QStringLiteral("读取 PcbLib 图元 %1 的子块 %2 失败: %3")
                                    .arg(objectId)
                                    .arg(blockIndex)
                                    .arg(reader.errorString()));
            if (payload.isEmpty())
                return failRead(QStringLiteral("PcbLib 图元 %1 的子块 %2 为空").arg(objectId).arg(blockIndex));
            const bool isPadStringBlock = objectId == AltiumConstants::PCB_OBJECT_PAD && blockIndex < 3;
            const bool isTextStringBlock = objectId == AltiumConstants::PCB_OBJECT_TEXT && blockIndex == 1;
            if ((isPadStringBlock || isTextStringBlock) && !isStringBlockPayload(payload)) {
                return failRead(QStringLiteral("PcbLib 图元 %1 的字符串子块 %2 无效").arg(objectId).arg(blockIndex));
            }
            const bool isMainBlock = (objectId == AltiumConstants::PCB_OBJECT_PAD && blockIndex == 4) ||
                                     (objectId != AltiumConstants::PCB_OBJECT_PAD && blockIndex == 0);
            if (isMainBlock && !readCommonPrimitiveHeader(payload, &object.layer, &object.primitiveFlags))
                return failRead(QStringLiteral("PcbLib 图元 %1 的公共头部不完整").arg(objectId));
            if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_TRACK) {
                if (!parseTrackFields(payload, &object.track))
                    return failRead(QStringLiteral("PcbLib 走线图元字段不完整"));
                object.hasTrackFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_ARC) {
                if (!parseArcFields(payload, &object.arc))
                    return failRead(QStringLiteral("PcbLib 弧线图元字段不完整"));
                object.hasArcFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_PAD) {
                if (!parsePadMainFields(payload, &object.pad))
                    return failRead(QStringLiteral("PcbLib 焊盘图元主块字段不完整"));
                object.hasPadFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_FILL) {
                if (!parseFillFields(payload, &object.fill))
                    return failRead(QStringLiteral("PcbLib 填充图元字段不完整"));
                object.hasFillFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_REGION) {
                if (!parseRegionFields(payload, &object.region))
                    return failRead(QStringLiteral("PcbLib 区域图元字段不完整"));
                object.hasRegionFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_COMPONENT_BODY) {
                if (!parseComponentBodyFields(payload, &object.componentBody))
                    return failRead(QStringLiteral("PcbLib 三维元件体图元字段不完整"));
                object.hasComponentBodyFields = true;
            } else if (isMainBlock && objectId == AltiumConstants::PCB_OBJECT_TEXT) {
                if (!parseTextFields(payload, &object.textFields))
                    return failRead(QStringLiteral("PcbLib 文本图元字段不完整"));
                object.hasTextFields = true;
            }
            PrimitiveBlock block;
            block.flags = flags;
            block.payload = payload;
            block.encoded = data.mid(blockStart, reader.position() - blockStart);
            object.blocks.append(block);
        }
        if (objectId == AltiumConstants::PCB_OBJECT_PAD) {
            if (!readStringBlockPayload(object.blocks.at(0).payload, &object.designator) ||
                !parsePadExtendedFields(object.blocks.at(5).payload, &object.pad))
                return failRead(QStringLiteral("PcbLib 焊盘图元扩展字段不完整"));
        }
        if (objectId == AltiumConstants::PCB_OBJECT_TEXT &&
            !readStringBlockPayload(object.blocks.at(1).payload, &object.text))
            return failRead(QStringLiteral("PcbLib 文本图元字符串内容无效"));
        QString semanticError;
        if (!validatePrimitiveFields(object, &semanticError))
            return failRead(semanticError);
        object.encoded = data.mid(objectStart, reader.position() - objectStart);
        objects->append(object);
    }
    return true;
}

bool AltiumPcbLibReader::readFootprintObjects(const QString& componentName, QVector<PrimitiveRecord>* objects) const {
    return readFootprintObjects(componentIndex(componentName), objects);
}

QString AltiumPcbLibReader::errorString() const {
    return m_errorMessage;
}

bool AltiumPcbLibReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

}  // namespace EasyKiConverter
