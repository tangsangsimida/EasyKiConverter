#include "AltiumPcbLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumConstants.h"

namespace EasyKiConverter {

namespace {

int primitiveBlockCount(quint8 objectId) {
    switch (objectId) {
        case AltiumConstants::PCB_OBJECT_ARC:
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
    for (uint32_t i = 0; i < componentCount; ++i) {
        QString name;
        if (!libraryReader.readStringBlock(&name) || name.isEmpty())
            return fail(QStringLiteral("PcbLib Library/Data 的封装名称无效"));
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
        for (uint32_t i = 0; i < keyCount; ++i) {
            QString libRef;
            QString sectionKey;
            if (!sectionKeyReader.readPascalString(&libRef) || !sectionKeyReader.readStringBlock(&sectionKey) ||
                libRef.isEmpty() || sectionKey.isEmpty())
                return fail(QStringLiteral("PcbLib SectionKeys 映射不完整"));
            sectionKeys.insert(libRef, sectionKey);
        }
        if (sectionKeyReader.hasError() || sectionKeyReader.remaining() != 0)
            return fail(QStringLiteral("PcbLib SectionKeys 末尾包含无效数据"));
    }

    for (const QString& name : names) {
        const QString sectionKey = sectionKeys.value(name, name);
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
    if (objects == nullptr)
        return false;
    objects->clear();

    QByteArray data;
    if (!readFootprintStream(componentIndexValue, QStringLiteral("Data"), &data))
        return false;

    AltiumBinaryReader reader(data);
    QString footprintName;
    if (!reader.readStringBlock(&footprintName) || footprintName.isEmpty())
        return false;

    while (reader.remaining() > 0) {
        const int objectStart = reader.position();
        uint8_t objectId = 0;
        if (!reader.readUInt8(&objectId)) {
            objects->clear();
            return false;
        }
        const int blockCount = primitiveBlockCount(objectId);
        if (blockCount < 0) {
            objects->clear();
            return false;
        }

        PrimitiveRecord object;
        object.objectId = objectId;
        object.blocks.reserve(blockCount);
        for (int blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
            const int blockStart = reader.position();
            QByteArray payload;
            uint8_t flags = 0;
            if (!reader.readBlock(&payload, &flags) || payload.isEmpty()) {
                objects->clear();
                return false;
            }
            PrimitiveBlock block;
            block.flags = flags;
            block.payload = payload;
            block.encoded = data.mid(blockStart, reader.position() - blockStart);
            object.blocks.append(block);
        }
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
