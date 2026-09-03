#include "AltiumBinaryWriter.h"

#include <cstring>

namespace EasyKiConverter {

/**
 * @brief 构造函数
 */
AltiumBinaryWriter::AltiumBinaryWriter(QByteArray& buffer) : m_buffer(buffer) {}

void AltiumBinaryWriter::writeInt8(int8_t value) {
    m_buffer.append(static_cast<char>(value));
}

void AltiumBinaryWriter::writeUInt8(uint8_t value) {
    m_buffer.append(static_cast<char>(value));
}

void AltiumBinaryWriter::writeInt16(int16_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
}

void AltiumBinaryWriter::writeUInt16(uint16_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
}

void AltiumBinaryWriter::writeInt32(int32_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 16) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 24) & 0xFF));
}

void AltiumBinaryWriter::writeUInt32(uint32_t value) {
    m_buffer.append(static_cast<char>(value & 0xFF));
    m_buffer.append(static_cast<char>((value >> 8) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 16) & 0xFF));
    m_buffer.append(static_cast<char>((value >> 24) & 0xFF));
}

void AltiumBinaryWriter::writeFloat(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    writeUInt32(bits);
}

void AltiumBinaryWriter::writeDouble(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    for (int i = 0; i < 8; ++i) {
        m_buffer.append(static_cast<char>((bits >> (i * 8)) & 0xFF));
    }
}

void AltiumBinaryWriter::writeBytes(const QByteArray& data) {
    m_buffer.append(data);
}

/**
 * @brief 开始一个大小前缀块
 * @details 写入 4 字节占位符 (0)，记录当前位置
 */
void AltiumBinaryWriter::beginBlock(uint8_t flags) {
    m_blockPositions.append(m_buffer.size());
    // 写入占位符
    m_buffer.append(static_cast<char>(flags));
    m_buffer.append('\0');
    m_buffer.append('\0');
    m_buffer.append('\0');
}

/**
 * @brief 结束大小前缀块，回填块大小
 */
void AltiumBinaryWriter::endBlock() {
    if (m_blockPositions.isEmpty()) {
        return;
    }

    int blockStart = m_blockPositions.takeLast();
    int payloadSize = m_buffer.size() - blockStart - 4;
    uint8_t flags = static_cast<uint8_t>(m_buffer[blockStart]);

    // 回填大小：(flags << 24) | payload_size
    uint32_t sizeHeader = (static_cast<uint32_t>(flags) << 24) | (static_cast<uint32_t>(payloadSize) & 0x00FFFFFF);
    m_buffer[blockStart] = static_cast<char>(sizeHeader & 0xFF);
    m_buffer[blockStart + 1] = static_cast<char>((sizeHeader >> 8) & 0xFF);
    m_buffer[blockStart + 2] = static_cast<char>((sizeHeader >> 16) & 0xFF);
    m_buffer[blockStart + 3] = static_cast<char>((sizeHeader >> 24) & 0xFF);
}

/**
 * @brief 写入 Pascal 短字符串
 */
void AltiumBinaryWriter::writePascalShortString(const QString& str) {
    QByteArray encoded = str.toLatin1();
    uint8_t len = static_cast<uint8_t>(qMin(encoded.size(), 255));
    writeUInt8(len);
    m_buffer.append(encoded.left(len));
}

/**
 * @brief 写入字符串块
 */
void AltiumBinaryWriter::writeStringBlock(const QString& str) {
    QByteArray encoded = str.toLatin1();
    uint8_t strLen = static_cast<uint8_t>(qMin(encoded.size(), 255));
    uint32_t blockSize = 1 + strLen;
    writeUInt32(blockSize);
    writeUInt8(strLen);
    m_buffer.append(encoded.left(strLen));
}

void AltiumBinaryWriter::writePascalString(const QString& str) {
    beginBlock();
    writePascalShortString(str);
    writeUInt8(0);
    endBlock();
}

/**
 * @brief 写入 C 字符串参数块
 */
void AltiumBinaryWriter::writeCStringParameterBlock(const QMap<QString, QString>& params) {
    QString paramStr;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        QString value = it.value();
        value.replace('|', ' ');
        value.replace(QChar::Null, ' ');
        paramStr += "|" + it.key() + "=" + value;
    }
    paramStr += "|";
    writeCStringParameterBlockRaw(paramStr);
}

/**
 * @brief 写入 C 字符串参数块（自动处理非 ASCII 字符）
 */
void AltiumBinaryWriter::writeCStringParameterBlockUtf8(const QMap<QString, QString>& params) {
    QByteArray encoded;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QByteArray key = it.key().toLatin1();
        QString value = it.value();
        value.replace('|', ' ');
        value.replace(QChar::Null, ' ');
        const QByteArray ansiValue = value.toLatin1();
        encoded += '|';
        encoded += key;
        encoded += '=';
        encoded += ansiValue;

        // %UTF8% 参数的值必须是原始 UTF-8 字节，而不是再次 Latin-1 转码。
        if (QString::fromLatin1(ansiValue) != value) {
            encoded += "|%UTF8%";
            encoded += key;
            encoded += '=';
            encoded += value.toUtf8();
        }
    }
    encoded += '|';
    writeCStringParameterBlockRaw(encoded);
}

/**
 * @brief 写入原始 C 字符串参数块
 */
void AltiumBinaryWriter::writeCStringParameterBlockRaw(const QString& paramString) {
    writeCStringParameterBlockRaw(paramString.toLatin1());
}

void AltiumBinaryWriter::writeCStringParameterBlockRaw(const QByteArray& parameterBytes) {
    QByteArray encoded = parameterBytes;
    encoded.append('\0');  // null 终止符
    uint32_t size = static_cast<uint32_t>(encoded.size());
    writeUInt32(size);
    m_buffer.append(encoded);
}

/**
 * @brief 写入坐标点
 */
void AltiumBinaryWriter::writeCoordPoint(int32_t x, int32_t y) {
    writeInt32(x);
    writeInt32(y);
}

/**
 * @brief 写入坐标值
 */
void AltiumBinaryWriter::writeCoord(int32_t value) {
    writeInt32(value);
}

/**
 * @brief 获取当前写入位置
 */
int AltiumBinaryWriter::position() const {
    return m_buffer.size();
}

}  // namespace EasyKiConverter
