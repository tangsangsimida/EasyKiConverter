#include "AltiumSchLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumWriterUtils.h"

#include <QSet>

#include <zlib.h>

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

bool validateGraphicParameters(const QMap<QString, QString>& parameters, int recordType, QString* error) {
    const auto readInt = [&parameters](const QString& name, bool required, int fallback, int* value) {
        if (value == nullptr)
            return false;
        if (!parameters.contains(name)) {
            if (required)
                return false;
            *value = fallback;
            return true;
        }
        bool ok = false;
        const int parsed = parameters.value(name).toInt(&ok);
        if (!ok)
            return false;
        *value = parsed;
        return true;
    };
    const auto failValidation = [error](const QString& message) {
        if (error != nullptr)
            *error = message;
        return false;
    };
    const auto readCoordinate = [&readInt](const QString& name, qint64* value) {
        if (value == nullptr)
            return false;
        int integer = 0;
        int fraction = 0;
        if (!readInt(name, true, 0, &integer) || !readInt(name + QStringLiteral("_Frac"), false, 0, &fraction) ||
            fraction <= -100000 || fraction >= 100000)
            return false;
        *value = static_cast<qint64>(integer) * 100000 + fraction;
        return true;
    };
    const auto validateBounds = [&]() {
        const QStringList coordinateNames = {QStringLiteral("Location.X"),
                                             QStringLiteral("Location.Y"),
                                             QStringLiteral("Corner.X"),
                                             QStringLiteral("Corner.Y")};
        bool hasCoordinate = false;
        for (const QString& name : coordinateNames)
            hasCoordinate = hasCoordinate || parameters.contains(name);
        if (!hasCoordinate)
            return true;

        qint64 locationX = 0;
        qint64 locationY = 0;
        qint64 cornerX = 0;
        qint64 cornerY = 0;
        if (!readCoordinate(QStringLiteral("Location.X"), &locationX) ||
            !readCoordinate(QStringLiteral("Location.Y"), &locationY) ||
            !readCoordinate(QStringLiteral("Corner.X"), &cornerX) ||
            !readCoordinate(QStringLiteral("Corner.Y"), &cornerY))
            return false;
        return locationX != cornerX && locationY != cornerY;
    };

    if (recordType == 5 || recordType == 6 || recordType == 7) {
        const int minimumCount = recordType == 5 ? 4 : (recordType == 7 ? 3 : 2);
        int locationCount = 0;
        if (!readInt(QStringLiteral("LocationCount"), true, 0, &locationCount) || locationCount < minimumCount ||
            locationCount > 100000) {
            return failValidation(QStringLiteral("LocationCount 无效"));
        }
        for (int i = 1; i <= locationCount; ++i) {
            for (const QString& axis : {QStringLiteral("X"), QStringLiteral("Y")}) {
                const QString name = QStringLiteral("%1%2").arg(axis).arg(i);
                int coordinate = 0;
                if (!readInt(name, false, 0, &coordinate))
                    return failValidation(QStringLiteral("%1 坐标无效").arg(name));
            }
        }
    }

    if (recordType == 8 || recordType == 9 || recordType == 11) {
        int radius = 0;
        if (!readInt(QStringLiteral("Radius"), true, 0, &radius) || radius <= 0)
            return failValidation(QStringLiteral("Radius 无效"));
        if (recordType == 8 || recordType == 11) {
            int secondaryRadius = 0;
            const bool required = recordType == 11;
            if (!readInt(QStringLiteral("SecondaryRadius"), required, 0, &secondaryRadius) ||
                (parameters.contains(QStringLiteral("SecondaryRadius")) && secondaryRadius <= 0)) {
                return failValidation(QStringLiteral("SecondaryRadius 无效"));
            }
        }
    }

    if (recordType == 10) {
        if (!validateBounds())
            return failValidation(QStringLiteral("圆角矩形边界尺寸无效"));
        for (const QString& name : {QStringLiteral("CornerXRadius"), QStringLiteral("CornerYRadius")}) {
            int radius = 0;
            if (!readInt(name, false, 0, &radius) || radius < 0)
                return failValidation(QStringLiteral("%1 无效").arg(name));
        }
    }
    if (recordType == 13 && !validateBounds())
        return failValidation(QStringLiteral("线段边界尺寸无效"));
    if (recordType == 14 && !validateBounds())
        return failValidation(QStringLiteral("矩形边界尺寸无效"));
    if (recordType == 28) {
        if (!validateBounds())
            return failValidation(QStringLiteral("文本框边界尺寸无效"));
        int textMargin = 0;
        int textMarginFraction = 0;
        if (!readInt(QStringLiteral("TextMargin"), false, 0, &textMargin) ||
            !readInt(QStringLiteral("TextMargin_Frac"), false, 0, &textMarginFraction) ||
            textMarginFraction <= -100000 || textMarginFraction >= 100000 ||
            (textMargin < 0 || (textMargin == 0 && textMarginFraction < 0)))
            return failValidation(QStringLiteral("TextMargin 无效"));
    }
    return true;
}

bool readBinaryRecordMetadata(const QByteArray& payload,
                              int* recordType,
                              int* ownerPartId,
                              int* ownerPartDisplayMode = nullptr) {
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
    if (ownerPartDisplayMode != nullptr)
        *ownerPartDisplayMode = -1;
    if (ownerPartDisplayMode != nullptr && type == 2 && payload.size() >= 8) {
        AltiumBinaryReader displayReader(payload);
        qint32 ignoredType = -1;
        quint8 ignoredUnknown = 0;
        qint16 ignoredOwner = -1;
        quint8 displayMode = 0;
        if (!displayReader.readInt32(&ignoredType) || !displayReader.readUInt8(&ignoredUnknown) ||
            !displayReader.readInt16(&ignoredOwner) || !displayReader.readUInt8(&displayMode))
            return false;
        *ownerPartDisplayMode = displayMode;
    }
    return true;
}

constexpr qint64 kMaxImageDecompressedSize = 512LL * 1024LL * 1024LL;

bool isValidZlibPayload(const QByteArray& compressedData) {
    if (compressedData.isEmpty())
        return false;

    z_stream stream{};
    if (inflateInit(&stream) != Z_OK)
        return false;

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.constData()));
    stream.avail_in = static_cast<uInt>(compressedData.size());
    QByteArray buffer(8192, Qt::Uninitialized);
    qint64 decompressedSize = 0;
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        result = inflate(&stream, Z_NO_FLUSH);
        decompressedSize += buffer.size() - stream.avail_out;
        if (decompressedSize > kMaxImageDecompressedSize) {
            inflateEnd(&stream);
            return false;
        }
    }

    const bool valid = result == Z_STREAM_END && stream.avail_in == 0 && decompressedSize > 0;
    inflateEnd(&stream);
    return valid;
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
    QSet<QString> componentNames;
    for (int i = 0; i < componentCount; ++i) {
        QString name;
        if (!headerReader.readStringBlock(&name) || name.isEmpty())
            return fail(QStringLiteral("SchLib FileHeader 的组件名称无效"));
        const QString foldedName = name.toCaseFolded();
        if (componentNames.contains(foldedName))
            return fail(QStringLiteral("SchLib FileHeader 的组件名称重复: %1").arg(name));
        componentNames.insert(foldedName);
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
        QSet<QString> mappedComponents;
        QSet<QString> mappedSectionKeys;
        for (int i = 0; i < keyCount; ++i) {
            const QString libRef = parameters.value(QStringLiteral("LibRef%1").arg(i));
            const QString sectionKey = parameters.value(QStringLiteral("SectionKey%1").arg(i));
            if (libRef.isEmpty() || sectionKey.isEmpty())
                return fail(QStringLiteral("SchLib SectionKeys 映射不完整"));
            if (!componentNames.contains(libRef.toCaseFolded()))
                return fail(QStringLiteral("SchLib SectionKeys 包含未知组件: %1").arg(libRef));
            if (mappedComponents.contains(libRef.toCaseFolded()) ||
                mappedSectionKeys.contains(sectionKey.toCaseFolded()))
                return fail(QStringLiteral("SchLib SectionKeys 映射重复: %1").arg(libRef));
            mappedComponents.insert(libRef.toCaseFolded());
            mappedSectionKeys.insert(sectionKey.toCaseFolded());
            sectionKeys.insert(libRef, sectionKey);
        }
        if (sectionKeyReader.hasError() || sectionKeyReader.remaining() != 0)
            return fail(QStringLiteral("SchLib SectionKeys 末尾包含无效数据"));
    }

    QSet<QString> resolvedSectionKeys;
    for (int i = 0; i < names.size(); ++i) {
        const QString& name = names.at(i);
        const QString sectionKey = sectionKeys.value(name, name);
        const QString foldedSectionKey = sectionKey.toCaseFolded();
        if (resolvedSectionKeys.contains(foldedSectionKey))
            return fail(QStringLiteral("SchLib 组件存储键重复: %1").arg(sectionKey));
        resolvedSectionKeys.insert(foldedSectionKey);
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
    int lastIndexInSheet = -1;
    int nextContentIndex = -1;
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
            const auto readOptionalParameterAlias =
                [&record](const QString& primary, const QString& alias, int fallback, int* value) {
                    const bool hasPrimary = record.parameters.contains(primary);
                    const bool hasAlias = record.parameters.contains(alias);
                    if (hasPrimary && hasAlias) {
                        int primaryValue = fallback;
                        int aliasValue = fallback;
                        if (!readOptionalParameterInt(record.parameters, primary, fallback, &primaryValue) ||
                            !readOptionalParameterInt(record.parameters, alias, fallback, &aliasValue) ||
                            primaryValue != aliasValue)
                            return false;
                        *value = primaryValue;
                        return true;
                    }
                    const QString selectedName = hasPrimary ? primary : alias;
                    return readOptionalParameterInt(record.parameters, selectedName, fallback, value);
                };
            if (!readOptionalParameterInt(record.parameters, QStringLiteral("RECORD"), -1, &record.recordType) ||
                !readOptionalParameterAlias(
                    QStringLiteral("OWNERPARTID"), QStringLiteral("OwnerPartId"), -1, &record.ownerPartId) ||
                !readOptionalParameterAlias(QStringLiteral("OWNERPARTDISPLAYMODE"),
                                            QStringLiteral("OwnerPartDisplayMode"),
                                            -1,
                                            &record.ownerPartDisplayMode) ||
                !readOptionalParameterInt(
                    record.parameters, QStringLiteral("IndexInSheet"), -1, &record.indexInSheet)) {
                return failRead(QStringLiteral("SchLib 组件参数记录的数值字段无效，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartDisplayMode < -1)
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTDISPLAYMODE 无效，偏移量 %1").arg(startPosition));
            const bool hasFontId = record.parameters.contains(QStringLiteral("FontID")) ||
                                   record.parameters.contains(QStringLiteral("FONTID"));
            if (hasFontId) {
                int mixedCaseFontId = -1;
                int upperCaseFontId = -1;
                if (!readOptionalParameterInt(record.parameters, QStringLiteral("FontID"), -1, &mixedCaseFontId) ||
                    !readOptionalParameterInt(record.parameters, QStringLiteral("FONTID"), -1, &upperCaseFontId)) {
                    return failRead(QStringLiteral("SchLib 组件参数记录的 FONTID 无效，偏移量 %1").arg(startPosition));
                }
                if (mixedCaseFontId >= 0 && upperCaseFontId >= 0 && mixedCaseFontId != upperCaseFontId)
                    return failRead(
                        QStringLiteral("SchLib 组件参数记录的 FONTID 字段不一致，偏移量 %1").arg(startPosition));
                record.fontId = mixedCaseFontId >= 0 ? mixedCaseFontId : upperCaseFontId;
                if (record.fontId < 1 || record.fontId > m_fonts.size())
                    return failRead(
                        QStringLiteral("SchLib 组件参数记录的 FONTID 超出字体表范围，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartId < -1)
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTID 小于 -1，偏移量 %1").arg(startPosition));
            if (record.ownerPartId >= 0 && record.ownerPartId > m_components.at(componentIndexValue).partCount) {
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 OWNERPARTID 超出部件范围，偏移量 %1").arg(startPosition));
            }
            QString geometryError;
            if (!validateGraphicParameters(record.parameters, record.recordType, &geometryError))
                return failRead(
                    QStringLiteral("SchLib 组件几何记录无效（%1），偏移量 %2").arg(geometryError).arg(startPosition));
            if (record.indexInSheet < -1 || (record.indexInSheet >= 0 && record.indexInSheet <= lastIndexInSheet)) {
                return failRead(
                    QStringLiteral("SchLib 组件参数记录的 IndexInSheet 非递增，偏移量 %1").arg(startPosition));
            }
            if (record.indexInSheet >= 0)
                lastIndexInSheet = record.indexInSheet;
        } else {
            if (!readBinaryRecordMetadata(
                    payload, &record.recordType, &record.ownerPartId, &record.ownerPartDisplayMode)) {
                return failRead(QStringLiteral("SchLib 二进制记录元数据无效，偏移量 %1").arg(startPosition));
            }
            if (record.ownerPartId < -1)
                return failRead(
                    QStringLiteral("SchLib 二进制记录的 OWNERPARTID 小于 -1，偏移量 %1").arg(startPosition));
            if (record.ownerPartId >= 0 && record.ownerPartId > m_components.at(componentIndexValue).partCount) {
                return failRead(
                    QStringLiteral("SchLib 二进制记录的 OWNERPARTID 超出部件范围，偏移量 %1").arg(startPosition));
            }
        }

        // 图元参数记录和二进制引脚共享同一个内容序号。首条内容记录的
        // IndexInSheet=0 可以省略，但后续记录必须紧接前一条内容记录；
        // 否则写入端可能漏掉图元，Altium 的绘制顺序也会发生偏移。
        const bool hasOwnerPartField = record.parameters.contains(QStringLiteral("OWNERPARTID")) ||
                                       record.parameters.contains(QStringLiteral("OwnerPartId"));
        const bool isIndexedParameter = record.hasParameters && hasOwnerPartField && record.recordType != 1 &&
                                        record.recordType != 34 && (record.recordType != 41 || record.ownerPartId >= 1);
        const bool isContentRecord = (!record.hasParameters && record.recordType == 2) || isIndexedParameter;
        if (isContentRecord) {
            if (nextContentIndex < 0) {
                nextContentIndex = record.indexInSheet >= 0 ? record.indexInSheet + 1 : 1;
            } else if (!record.hasParameters) {
                ++nextContentIndex;
            } else if (record.indexInSheet < 0 || record.indexInSheet != nextContentIndex) {
                return failRead(
                    QStringLiteral("SchLib 组件的内容记录 IndexInSheet 不连续，偏移量 %1").arg(startPosition));
            } else {
                ++nextContentIndex;
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

        if (!isValidZlibPayload(compressedData)) {
            m_errorMessage = QStringLiteral("SchLib 图片 Storage 压缩数据无效");
            entries->clear();
            return false;
        }

        const QString name = QString::fromLocal8Bit(nameData);
        const QString foldedName = name.toCaseFolded();
        if (!AltiumWriterUtils::isValidImageStorageName(name) || names.contains(foldedName)) {
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
