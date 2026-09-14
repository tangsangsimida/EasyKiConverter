#include "AltiumSchLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"

#include <QSet>

namespace EasyKiConverter {

namespace {

int parameterInt(const QMap<QString, QString>& parameters, const QString& name, int fallback) {
    bool ok = false;
    const int value = parameters.value(name).toInt(&ok);
    return ok ? value : fallback;
}

bool readOptionalParameterInt(const QMap<QString, QString>& parameters, const QString& name, int fallback, int* value) {
    if (value == nullptr)
        return false;
    if (!parameters.contains(name)) {
        *value = fallback;
        return true;
    }
    bool ok = false;
    const int parsed = parameters.value(name).toInt(&ok);
    if (!ok)
        return false;
    *value = parsed;
    return true;
}

bool readBinaryRecordMetadata(const QByteArray& payload, int* recordType, int* ownerPartId) {
    if (recordType == nullptr || ownerPartId == nullptr || payload.size() < 7)
        return false;
    AltiumBinaryReader reader(payload);
    qint32 type = -1;
    quint8 unknown = 0;
    qint16 owner = -1;
    if (!reader.readInt32(&type) || !reader.readUInt8(&unknown) || !reader.readInt16(&owner))
        return false;
    *recordType = type;
    *ownerPartId = owner;
    return true;
}

}  // namespace

bool AltiumSchLibReader::fail(const QString& message) {
    m_components.clear();
    m_headerParameters.clear();
    m_fonts.clear();
    m_errorMessage = message;
    return false;
}

bool AltiumSchLibReader::open(const QString& filePath) {
    m_components.clear();
    m_headerParameters.clear();
    m_fonts.clear();
    m_errorMessage.clear();

    if (!m_oleReader.open(filePath))
        return fail(m_oleReader.errorString());

    QByteArray headerData;
    if (!m_oleReader.readStream(QStringLiteral("FileHeader"), &headerData))
        return fail(QStringLiteral("SchLib 缺少 FileHeader 流"));

    AltiumBinaryReader headerReader(headerData);
    if (!headerReader.readCStringParameterBlock(&m_headerParameters))
        return fail(QStringLiteral("SchLib FileHeader 参数块无效: %1").arg(headerReader.errorString()));

    bool fontCountOk = true;
    const int fontCount = m_headerParameters.contains(QStringLiteral("FontIdCount"))
                              ? m_headerParameters.value(QStringLiteral("FontIdCount")).toInt(&fontCountOk)
                              : 0;
    if (!fontCountOk || fontCount < 0)
        return fail(QStringLiteral("SchLib FileHeader 的 FontIdCount 无效"));
    m_fonts.reserve(fontCount);
    for (int i = 1; i <= fontCount; ++i) {
        FontInfo font;
        font.name = m_headerParameters.value(QStringLiteral("FontName%1").arg(i));
        font.size = parameterInt(m_headerParameters, QStringLiteral("Size%1").arg(i), 0);
        font.bold = m_headerParameters.value(QStringLiteral("Bold%1").arg(i)) == QStringLiteral("T");
        font.italic = m_headerParameters.value(QStringLiteral("Italic%1").arg(i)) == QStringLiteral("T");
        font.underline = m_headerParameters.value(QStringLiteral("Underline%1").arg(i)) == QStringLiteral("T");
        if (font.name.isEmpty() || font.size <= 0)
            return fail(QStringLiteral("SchLib FileHeader 的字体 %1 无效").arg(i));
        m_fonts.append(font);
    }

    bool componentCountOk = false;
    const int componentCount = m_headerParameters.value(QStringLiteral("COMPCOUNT")).toInt(&componentCountOk);
    if (!componentCountOk || componentCount < 0)
        return fail(QStringLiteral("SchLib FileHeader 的 COMPCOUNT 无效"));

    int32_t serializedCount = 0;
    if (!headerReader.readInt32(&serializedCount) || serializedCount != componentCount)
        return fail(QStringLiteral("SchLib FileHeader 的组件数量不一致"));

    QVector<QString> names;
    names.reserve(componentCount);
    for (int i = 0; i < componentCount; ++i) {
        QString name;
        if (!headerReader.readStringBlock(&name) || name.isEmpty())
            return fail(QStringLiteral("SchLib FileHeader 的组件名称无效"));
        names.append(name);
    }
    if (headerReader.hasError() || headerReader.remaining() != 0)
        return fail(QStringLiteral("SchLib FileHeader 末尾包含无效数据"));

    QMap<QString, QString> sectionKeys;
    if (m_oleReader.containsStream(QStringLiteral("SectionKeys"))) {
        QByteArray sectionKeyData;
        if (!m_oleReader.readStream(QStringLiteral("SectionKeys"), &sectionKeyData))
            return fail(QStringLiteral("无法读取 SchLib SectionKeys 流"));
        AltiumBinaryReader sectionKeyReader(sectionKeyData);
        QMap<QString, QString> parameters;
        if (!sectionKeyReader.readCStringParameterBlock(&parameters))
            return fail(QStringLiteral("SchLib SectionKeys 参数块无效: %1").arg(sectionKeyReader.errorString()));
        bool keyCountOk = false;
        const int keyCount = parameters.value(QStringLiteral("KeyCount")).toInt(&keyCountOk);
        if (!keyCountOk || keyCount < 0)
            return fail(QStringLiteral("SchLib SectionKeys 的 KeyCount 无效"));
        for (int i = 0; i < keyCount; ++i) {
            const QString libRef = parameters.value(QStringLiteral("LibRef%1").arg(i));
            const QString sectionKey = parameters.value(QStringLiteral("SectionKey%1").arg(i));
            if (libRef.isEmpty() || sectionKey.isEmpty())
                return fail(QStringLiteral("SchLib SectionKeys 映射不完整"));
            sectionKeys.insert(libRef, sectionKey);
        }
        if (sectionKeyReader.hasError() || sectionKeyReader.remaining() != 0)
            return fail(QStringLiteral("SchLib SectionKeys 末尾包含无效数据"));
    }

    for (int i = 0; i < names.size(); ++i) {
        const QString& name = names.at(i);
        const QString sectionKey = sectionKeys.value(name, name);
        const QString dataPath = sectionKey + QStringLiteral("/Data");
        if (!m_oleReader.containsStream(dataPath))
            return fail(QStringLiteral("SchLib 缺少组件 Data 流: %1").arg(dataPath));
        const int serializedPartCount = parameterInt(m_headerParameters, QStringLiteral("PARTCOUNT%1").arg(i), 2);
        if (serializedPartCount < 1)
            return fail(QStringLiteral("SchLib 组件 %1 的 PARTCOUNT 无效").arg(name));
        m_components.append({name, sectionKey, qMax(1, serializedPartCount - 1)});
    }
    return true;
}

QVector<AltiumSchLibReader::ComponentInfo> AltiumSchLibReader::components() const {
    return m_components;
}

QMap<QString, QString> AltiumSchLibReader::headerParameters() const {
    return m_headerParameters;
}

QVector<AltiumSchLibReader::FontInfo> AltiumSchLibReader::fonts() const {
    return m_fonts;
}

int AltiumSchLibReader::componentIndex(const QString& componentName) const {
    for (int i = 0; i < m_components.size(); ++i) {
        if (m_components.at(i).name == componentName)
            return i;
    }
    return -1;
}

bool AltiumSchLibReader::readComponentData(int componentIndexValue, QByteArray* data) const {
    if (data == nullptr || componentIndexValue < 0 || componentIndexValue >= m_components.size())
        return false;
    return m_oleReader.readStream(m_components.at(componentIndexValue).sectionKey + QStringLiteral("/Data"), data);
}

bool AltiumSchLibReader::readComponentData(const QString& componentName, QByteArray* data) const {
    return readComponentData(componentIndex(componentName), data);
}

bool AltiumSchLibReader::readComponentRecords(int componentIndexValue, QVector<Record>* records) const {
    if (records == nullptr) {
        m_errorMessage = QStringLiteral("读取 SchLib 记录时输出容器为空");
        return false;
    }
    records->clear();
    m_errorMessage.clear();

    const auto failRead = [this, records](const QString& message) {
        records->clear();
        m_errorMessage = message;
        return false;
    };

    QByteArray data;
    if (!readComponentData(componentIndexValue, &data))
        return failRead(QStringLiteral("无法读取 SchLib 组件 Data 流"));
    AltiumBinaryReader reader(data);
    while (reader.remaining() > 0) {
        const int startPosition = reader.position();
        QByteArray payload;
        uint8_t flags = 0;
        if (!reader.readBlock(&payload, &flags))
            return failRead(QStringLiteral("读取 SchLib 组件记录失败: %1").arg(reader.errorString()));
        if (payload.isEmpty())
            return failRead(QStringLiteral("SchLib 组件记录为空，偏移量 %1").arg(startPosition));
        Record record;
        record.flags = flags;
        record.payload = payload;
        record.encoded = data.mid(startPosition, reader.position() - startPosition);
        if (payload.startsWith('|')) {
            AltiumBinaryReader parameterReader(payload);
            if (!parameterReader.parseCStringParameterData(payload, &record.parameters)) {
                return failRead(QStringLiteral("SchLib 组件参数记录无效，偏移量 %1: %2")
                                    .arg(startPosition)
                                    .arg(parameterReader.errorString()));
            }
            record.hasParameters = true;
            if (!readOptionalParameterInt(record.parameters, QStringLiteral("RECORD"), -1, &record.recordType) ||
                !readOptionalParameterInt(record.parameters, QStringLiteral("OWNERPARTID"), -1, &record.ownerPartId) ||
                !readOptionalParameterInt(
                    record.parameters, QStringLiteral("IndexInSheet"), -1, &record.indexInSheet)) {
                return failRead(QStringLiteral("SchLib 组件参数记录的数值字段无效，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartId >= 0 && record.ownerPartId > m_components.at(componentIndexValue).partCount) {
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTID 超出部件范围，偏移量 %1").arg(startPosition));
            }
        } else {
            readBinaryRecordMetadata(payload, &record.recordType, &record.ownerPartId);
            if (record.ownerPartId >= 0 && record.ownerPartId > m_components.at(componentIndexValue).partCount) {
                return failRead(
                    QStringLiteral("SchLib 二进制记录的 OWNERPARTID 超出部件范围，偏移量 %1").arg(startPosition));
            }
        }
        records->append(record);
    }
    if (records->isEmpty())
        return failRead(QStringLiteral("SchLib 组件 Data 流不包含记录"));
    return true;
}

bool AltiumSchLibReader::readComponentRecords(const QString& componentName, QVector<Record>* records) const {
    return readComponentRecords(componentIndex(componentName), records);
}

bool AltiumSchLibReader::readImageStorage(QVector<ImageStorageEntry>* entries) const {
    if (entries == nullptr) {
        m_errorMessage = QStringLiteral("读取 SchLib 图片 Storage 时输出容器为空");
        return false;
    }
    entries->clear();
    m_errorMessage.clear();

    if (!m_oleReader.containsStream(QStringLiteral("Storage")))
        return true;

    QByteArray storage;
    if (!m_oleReader.readStream(QStringLiteral("Storage"), &storage)) {
        m_errorMessage = QStringLiteral("无法读取 SchLib 图片 Storage 流");
        return false;
    }

    AltiumBinaryReader reader(storage);
    QMap<QString, QString> storageParameters;
    if (!reader.readCStringParameterBlock(&storageParameters)) {
        m_errorMessage = QStringLiteral("SchLib 图片 Storage 头部无效: %1").arg(reader.errorString());
        return false;
    }
    bool expectedCountOk = true;
    const int expectedCount = storageParameters.contains(QStringLiteral("Weight"))
                                  ? storageParameters.value(QStringLiteral("Weight")).toInt(&expectedCountOk)
                                  : -1;
    if (!expectedCountOk || expectedCount < 0) {
        m_errorMessage = QStringLiteral("SchLib 图片 Storage 的 Weight 无效");
        return false;
    }

    QSet<QString> names;
    while (reader.remaining() > 0) {
        QByteArray entryData;
        quint8 flags = 0;
        if (!reader.readBlock(&entryData, &flags)) {
            m_errorMessage = QStringLiteral("读取 SchLib 图片 Storage 条目失败: %1").arg(reader.errorString());
            entries->clear();
            return false;
        }
        if (flags != 1) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 条目标志无效: %1").arg(flags);
            entries->clear();
            return false;
        }

        AltiumBinaryReader entryReader(entryData);
        quint8 marker = 0;
        quint8 nameSize = 0;
        QByteArray nameData;
        quint32 compressedSize = 0;
        QByteArray compressedData;
        if (!entryReader.readUInt8(&marker) || marker != 0xD0 || !entryReader.readUInt8(&nameSize) || nameSize == 0 ||
            !entryReader.readBytes(nameSize, &nameData) || nameData.contains('\0') ||
            !entryReader.readUInt32(&compressedSize) || compressedSize == 0 ||
            compressedSize > static_cast<quint32>(entryReader.remaining()) ||
            !entryReader.readBytes(static_cast<int>(compressedSize), &compressedData) || entryReader.remaining() != 0) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 条目内容无效");
            entries->clear();
            return false;
        }

        const QString name = QString::fromLocal8Bit(nameData);
        const QString foldedName = name.toCaseFolded();
        if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") || name.contains('/') ||
            name.contains('\\') || name.contains('|') || names.contains(foldedName)) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 包含重复或非法文件名: %1").arg(name);
            entries->clear();
            return false;
        }
        names.insert(foldedName);
        entries->append({flags, name, compressedData});
    }

    if (expectedCount >= 0 && expectedCount != entries->size()) {
        m_errorMessage =
            QStringLiteral("SchLib 图片 Storage 数量不一致，声明 %1，实际 %2").arg(expectedCount).arg(entries->size());
        entries->clear();
        return false;
    }
    return true;
}

QString AltiumSchLibReader::errorString() const {
    return m_errorMessage;
}

bool AltiumSchLibReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

}  // namespace EasyKiConverter
