#include "AltiumBinaryReader.h"

#include <cstring>

namespace EasyKiConverter {

AltiumBinaryReader::AltiumBinaryReader(const QByteArray& buffer) : m_buffer(buffer) {}

bool AltiumBinaryReader::ensure(int size, const QString& operation) {
    if (size < 0 || size > remaining())
        return fail(QStringLiteral("读取 %1 时超出数据边界").arg(operation));
    return true;
}

bool AltiumBinaryReader::fail(const QString& message) {
    if (m_errorMessage.isEmpty())
        m_errorMessage = message;
    return false;
}

bool AltiumBinaryReader::readInt8(int8_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 i8 时输出指针为空"));
    uint8_t raw = 0;
    if (!readUInt8(&raw))
        return false;
    *value = static_cast<int8_t>(raw);
    return true;
}

bool AltiumBinaryReader::readUInt8(uint8_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 u8 时输出指针为空"));
    if (!ensure(1, QStringLiteral("u8")))
        return false;
    *value = static_cast<uint8_t>(m_buffer.at(m_position++));
    return true;
}

bool AltiumBinaryReader::readInt16(int16_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 i16 时输出指针为空"));
    uint16_t raw = 0;
    if (!readUInt16(&raw))
        return false;
    *value = static_cast<int16_t>(raw);
    return true;
}

bool AltiumBinaryReader::readUInt16(uint16_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 u16 时输出指针为空"));
    if (!ensure(2, QStringLiteral("u16")))
        return false;
    const auto byte = [this](int offset) {
        return static_cast<uint16_t>(static_cast<unsigned char>(m_buffer.at(offset)));
    };
    *value = byte(m_position) | static_cast<uint16_t>(byte(m_position + 1) << 8);
    m_position += 2;
    return true;
}

bool AltiumBinaryReader::readInt32(int32_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 i32 时输出指针为空"));
    uint32_t raw = 0;
    if (!readUInt32(&raw))
        return false;
    *value = static_cast<int32_t>(raw);
    return true;
}

bool AltiumBinaryReader::readUInt32Internal(uint32_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 u32 时输出指针为空"));
    if (!ensure(4, QStringLiteral("u32")))
        return false;
    *value = static_cast<uint32_t>(static_cast<unsigned char>(m_buffer.at(m_position))) |
             (static_cast<uint32_t>(static_cast<unsigned char>(m_buffer.at(m_position + 1))) << 8) |
             (static_cast<uint32_t>(static_cast<unsigned char>(m_buffer.at(m_position + 2))) << 16) |
             (static_cast<uint32_t>(static_cast<unsigned char>(m_buffer.at(m_position + 3))) << 24);
    m_position += 4;
    return true;
}

bool AltiumBinaryReader::readUInt32(uint32_t* value) {
    return readUInt32Internal(value);
}

bool AltiumBinaryReader::readFloat(float* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 float 时输出指针为空"));
    uint32_t raw = 0;
    if (!readUInt32(&raw))
        return false;
    std::memcpy(value, &raw, sizeof(raw));
    return true;
}

bool AltiumBinaryReader::readDouble(double* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 double 时输出指针为空"));
    if (!ensure(8, QStringLiteral("double")))
        return false;
    uint64_t raw = 0;
    for (int i = 0; i < 8; ++i)
        raw |= static_cast<uint64_t>(static_cast<unsigned char>(m_buffer.at(m_position + i))) << (i * 8);
    m_position += 8;
    std::memcpy(value, &raw, sizeof(raw));
    return true;
}

bool AltiumBinaryReader::readBytes(int size, QByteArray* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取字节时输出指针为空"));
    if (!ensure(size, QStringLiteral("字节")))
        return false;
    *value = m_buffer.mid(m_position, size);
    m_position += size;
    return true;
}

bool AltiumBinaryReader::readBlock(QByteArray* payload, uint8_t* flags) {
    uint32_t header = 0;
    if (!readUInt32(&header))
        return false;
    const int size = static_cast<int>(header & 0x00FFFFFFU);
    if (flags != nullptr)
        *flags = static_cast<uint8_t>(header >> 24);
    return readBytes(size, payload);
}

bool AltiumBinaryReader::readPascalShortString(QString* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 Pascal 短字符串时输出指针为空"));
    uint8_t size = 0;
    if (!readUInt8(&size))
        return false;
    QByteArray data;
    if (!readBytes(size, &data))
        return false;
    *value = QString::fromLatin1(data);
    return true;
}

bool AltiumBinaryReader::readStringBlock(QString* value) {
    uint32_t blockSize = 0;
    if (!readUInt32(&blockSize) || blockSize < 1 || blockSize > static_cast<uint32_t>(remaining()))
        return fail(QStringLiteral("字符串块长度无效"));
    const int payloadStart = m_position;
    uint8_t stringSize = 0;
    if (!readUInt8(&stringSize) || stringSize > blockSize - 1)
        return fail(QStringLiteral("字符串块内容长度无效"));
    QByteArray data;
    if (!readBytes(stringSize, &data))
        return false;
    m_position = payloadStart + static_cast<int>(blockSize);
    if (value == nullptr)
        return fail(QStringLiteral("读取字符串块时输出指针为空"));
    *value = QString::fromLatin1(data);
    return true;
}

bool AltiumBinaryReader::readPascalString(QString* value) {
    QByteArray block;
    if (!readBlock(&block))
        return false;
    AltiumBinaryReader blockReader(block);
    if (!blockReader.readPascalShortString(value))
        return fail(blockReader.errorString());
    uint8_t terminator = 0;
    if (!blockReader.readUInt8(&terminator) || terminator != 0 || blockReader.remaining() != 0)
        return fail(QStringLiteral("Pascal 字符串块终止符无效"));
    return true;
}

bool AltiumBinaryReader::readCStringParameterBlock(QMap<QString, QString>* params) {
    if (params == nullptr)
        return fail(QStringLiteral("读取参数块时输出指针为空"));
    QByteArray block;
    if (!readBlock(&block))
        return false;
    return parseCStringParameterData(block, params);
}

bool AltiumBinaryReader::parseCStringParameterData(const QByteArray& data, QMap<QString, QString>* params) {
    if (params == nullptr)
        return fail(QStringLiteral("解析参数数据时输出指针为空"));
    params->clear();
    QByteArray block = data;
    if (!block.isEmpty() && block.endsWith('\0'))
        block.chop(1);
    const QList<QByteArray> fields = block.split('|');
    for (const QByteArray& field : fields) {
        if (field.isEmpty())
            continue;
        const int separator = field.indexOf('=');
        if (separator <= 0)
            return fail(QStringLiteral("参数块字段缺少键值分隔符"));
        const QByteArray key = field.left(separator);
        const QByteArray rawValue = field.mid(separator + 1);
        if (key.startsWith("%UTF8%")) {
            const QString baseKey = QString::fromLatin1(key.mid(6));
            (*params)[baseKey] = QString::fromUtf8(rawValue);
        } else {
            (*params)[QString::fromLatin1(key)] = QString::fromLatin1(rawValue);
        }
    }
    return true;
}

int AltiumBinaryReader::position() const {
    return m_position;
}

int AltiumBinaryReader::remaining() const {
    return m_buffer.size() - m_position;
}

bool AltiumBinaryReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

QString AltiumBinaryReader::errorString() const {
    return m_errorMessage;
}

}  // namespace EasyKiConverter
