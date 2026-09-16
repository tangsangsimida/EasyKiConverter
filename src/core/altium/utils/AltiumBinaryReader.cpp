#include "AltiumBinaryReader.h"

#include <QSet>

#include <cstring>

namespace EasyKiConverter {

// 以小端序读取 Altium 二进制记录，并统一维护边界与错误状态。
AltiumBinaryReader::AltiumBinaryReader(const QByteArray& buffer) : m_buffer(buffer) {}

// 检查即将读取的字节数，防止解析位置越过输入缓冲区。
bool AltiumBinaryReader::ensure(int size, const QString& operation) {
    if (size < 0 || size > remaining())
        return fail(QStringLiteral("读取 %1 时超出数据边界").arg(operation));
    return true;
}

// 只保留首次解析失败的错误信息，避免后续错误覆盖根因。
bool AltiumBinaryReader::fail(const QString& message) {
    if (m_errorMessage.isEmpty())
        m_errorMessage = message;
    return false;
}

// 读取有符号八位整数，并复用无符号读取逻辑保证位置一致。
bool AltiumBinaryReader::readInt8(int8_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 i8 时输出指针为空"));
    uint8_t raw = 0;
    if (!readUInt8(&raw))
        return false;
    *value = static_cast<int8_t>(raw);
    return true;
}

// 读取一个无符号八位整数。
bool AltiumBinaryReader::readUInt8(uint8_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 u8 时输出指针为空"));
    if (!ensure(1, QStringLiteral("u8")))
        return false;
    *value = static_cast<uint8_t>(m_buffer.at(m_position++));
    return true;
}

// 读取有符号十六位整数，并按位模式转换结果。
bool AltiumBinaryReader::readInt16(int16_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 i16 时输出指针为空"));
    uint16_t raw = 0;
    if (!readUInt16(&raw))
        return false;
    *value = static_cast<int16_t>(raw);
    return true;
}

// 读取一个小端序无符号十六位整数。
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

// 读取有符号三十二位整数，并按位模式转换结果。
bool AltiumBinaryReader::readInt32(int32_t* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 i32 时输出指针为空"));
    uint32_t raw = 0;
    if (!readUInt32(&raw))
        return false;
    *value = static_cast<int32_t>(raw);
    return true;
}

// 执行小端序无符号三十二位整数的实际读取。
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

// 读取一个无符号三十二位整数。
bool AltiumBinaryReader::readUInt32(uint32_t* value) {
    return readUInt32Internal(value);
}

// 读取 IEEE 754 单精度浮点数。
bool AltiumBinaryReader::readFloat(float* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取 float 时输出指针为空"));
    uint32_t raw = 0;
    if (!readUInt32(&raw))
        return false;
    std::memcpy(value, &raw, sizeof(raw));
    return true;
}

// 读取 IEEE 754 双精度浮点数。
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

// 读取指定长度的原始字节，并推进当前解析位置。
bool AltiumBinaryReader::readBytes(int size, QByteArray* value) {
    if (value == nullptr)
        return fail(QStringLiteral("读取字节时输出指针为空"));
    if (!ensure(size, QStringLiteral("字节")))
        return false;
    *value = m_buffer.mid(m_position, size);
    m_position += size;
    return true;
}

// 读取带有长度和标志位头部的二进制块。
bool AltiumBinaryReader::readBlock(QByteArray* payload, uint8_t* flags) {
    uint32_t header = 0;
    if (!readUInt32(&header))
        return false;
    const int size = static_cast<int>(header & 0x00FFFFFFU);
    if (flags != nullptr)
        *flags = static_cast<uint8_t>(header >> 24);
    return readBytes(size, payload);
}

// 读取单字节长度前缀的 Pascal 短字符串。
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

// 读取带块长度的字符串，并跳过块内未使用的尾部空间。
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

// 读取包含终止字节的 Pascal 字符串块。
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

// 读取并解析一个 C 字符串参数块。
bool AltiumBinaryReader::readCStringParameterBlock(QMap<QString, QString>* params) {
    if (params == nullptr)
        return fail(QStringLiteral("读取参数块时输出指针为空"));
    QByteArray block;
    if (!readBlock(&block))
        return false;
    return parseCStringParameterData(block, params);
}

// 解析以竖线分隔的键值参数，并校验重复键和 UTF-8 前缀。
bool AltiumBinaryReader::parseCStringParameterData(const QByteArray& data, QMap<QString, QString>* params) {
    if (params == nullptr)
        return fail(QStringLiteral("解析参数数据时输出指针为空"));
    params->clear();
    QByteArray block = data;
    if (!block.isEmpty() && block.endsWith('\0'))
        block.chop(1);
    const QList<QByteArray> fields = block.split('|');
    QSet<QString> rawKeys;
    QSet<QString> utf8Keys;
    for (const QByteArray& field : fields) {
        if (field.isEmpty())
            continue;
        const int separator = field.indexOf('=');
        if (separator <= 0)
            return fail(QStringLiteral("参数块字段缺少键值分隔符"));
        const QByteArray key = field.left(separator);
        const QByteArray rawValue = field.mid(separator + 1);
        if (key.startsWith("%UTF8%")) {
            const QString baseKey = QString::fromUtf8(key.mid(6));
            if (baseKey.isEmpty() || utf8Keys.contains(baseKey))
                return fail(QStringLiteral("参数块包含重复或空的 UTF-8 键"));
            utf8Keys.insert(baseKey);
            (*params)[baseKey] = QString::fromUtf8(rawValue);
        } else {
            const QString baseKey = QString::fromLatin1(key);
            if (rawKeys.contains(baseKey) || utf8Keys.contains(baseKey))
                return fail(QStringLiteral("参数块包含重复键: %1").arg(baseKey));
            rawKeys.insert(baseKey);
            (*params)[baseKey] = QString::fromLatin1(rawValue);
        }
    }
    return true;
}

// 返回当前读取位置。
int AltiumBinaryReader::position() const {
    return m_position;
}

// 返回当前读取位置之后尚未消费的字节数。
int AltiumBinaryReader::remaining() const {
    return m_buffer.size() - m_position;
}

// 判断读取过程中是否记录过错误。
bool AltiumBinaryReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

// 返回首次解析失败时记录的错误信息。
QString AltiumBinaryReader::errorString() const {
    return m_errorMessage;
}

}  // namespace EasyKiConverter
