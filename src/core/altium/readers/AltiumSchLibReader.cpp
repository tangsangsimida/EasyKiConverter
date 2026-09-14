#include "AltiumSchLibReader.h"

#include "core/altium/utils/AltiumBinaryReader.h"

namespace EasyKiConverter {

bool AltiumSchLibReader::fail(const QString& message) {
    m_components.clear();
    m_headerParameters.clear();
    m_errorMessage = message;
    return false;
}

bool AltiumSchLibReader::open(const QString& filePath) {
    m_components.clear();
    m_headerParameters.clear();
    m_errorMessage.clear();

    if (!m_oleReader.open(filePath))
        return fail(m_oleReader.errorString());

    QByteArray headerData;
    if (!m_oleReader.readStream(QStringLiteral("FileHeader"), &headerData))
        return fail(QStringLiteral("SchLib 缺少 FileHeader 流"));

    AltiumBinaryReader headerReader(headerData);
    if (!headerReader.readCStringParameterBlock(&m_headerParameters))
        return fail(QStringLiteral("SchLib FileHeader 参数块无效: %1").arg(headerReader.errorString()));

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

    for (const QString& name : names) {
        const QString sectionKey = sectionKeys.value(name, name);
        const QString dataPath = sectionKey + QStringLiteral("/Data");
        if (!m_oleReader.containsStream(dataPath))
            return fail(QStringLiteral("SchLib 缺少组件 Data 流: %1").arg(dataPath));
        m_components.append({name, sectionKey});
    }
    return true;
}

QVector<AltiumSchLibReader::ComponentInfo> AltiumSchLibReader::components() const {
    return m_components;
}

QMap<QString, QString> AltiumSchLibReader::headerParameters() const {
    return m_headerParameters;
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

QString AltiumSchLibReader::errorString() const {
    return m_errorMessage;
}

bool AltiumSchLibReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

}  // namespace EasyKiConverter
