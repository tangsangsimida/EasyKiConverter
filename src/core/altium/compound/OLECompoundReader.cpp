#include "OLECompoundReader.h"

#include <QFile>
#include <QSet>

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>

namespace EasyKiConverter {

namespace {

constexpr quint32 kEndOfChain = 0xFFFFFFFEU;
constexpr quint32 kFreeSector = 0xFFFFFFFFU;
constexpr quint32 kFatSector = 0xFFFFFFFDU;
constexpr quint32 kDifSector = 0xFFFFFFFCU;
constexpr quint32 kNoStream = 0xFFFFFFFFU;
constexpr quint32 kSectorSize = 512;
constexpr quint32 kMiniSectorSize = 64;
constexpr quint32 kMiniStreamCutoff = 4096;
constexpr int kDirectoryEntrySize = 128;
constexpr int kMaxDirectoryDepth = 4096;

quint16 readU16(const QByteArray& data, qsizetype offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8);
}

quint32 readU32(const QByteArray& data, qsizetype offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

quint64 readU64(const QByteArray& data, qsizetype offset) {
    quint64 value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<quint64>(static_cast<unsigned char>(data.at(offset + i))) << (i * 8);
    return value;
}

struct DirectoryEntry {
    quint8 type = 0;
    quint32 left = kNoStream;
    quint32 right = kNoStream;
    quint32 child = kNoStream;
    quint32 startSector = kEndOfChain;
    quint64 streamSize = 0;
    QString name;
};

}  // namespace

OLECompoundReader::OLECompoundReader() = default;

OLECompoundReader::~OLECompoundReader() = default;

void OLECompoundReader::clear() {
    m_opened = false;
    m_errorMessage.clear();
    m_streams.clear();
}

bool OLECompoundReader::fail(const QString& message) {
    clear();
    m_errorMessage = message;
    return false;
}

bool OLECompoundReader::open(const QString& filePath) {
    clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("无法打开 OLE 文件: %1").arg(file.errorString()));
    const QByteArray fileData = file.readAll();
    if (fileData.size() < static_cast<qsizetype>(kSectorSize * 2))
        return fail(QStringLiteral("OLE 文件大小不足"));

    static constexpr unsigned char signature[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    if (memcmp(fileData.constData(), signature, sizeof(signature)) != 0)
        return fail(QStringLiteral("不是有效的 OLE 复合文档"));
    if (readU16(fileData, 26) != 3 || readU16(fileData, 28) != 0xFFFE || readU16(fileData, 30) != 9 ||
        readU16(fileData, 32) != 6)
        return fail(QStringLiteral("仅支持 OLE V3、512 字节扇区和 64 字节迷你扇区"));

    const quint32 sectorCount = static_cast<quint32>((fileData.size() - kSectorSize) / kSectorSize);
    if (sectorCount == 0 || static_cast<qsizetype>(sectorCount + 1) * kSectorSize != fileData.size())
        return fail(QStringLiteral("OLE 文件包含不完整的扇区"));

    auto sector = [&](quint32 index, QByteArray* result) {
        if (index >= sectorCount)
            return false;
        *result = fileData.mid(static_cast<qsizetype>(index + 1) * kSectorSize, kSectorSize);
        return result->size() == static_cast<int>(kSectorSize);
    };

    const quint32 firstDirectorySector = readU32(fileData, 48);
    const quint32 firstMiniFatSector = readU32(fileData, 60);
    const quint32 miniFatSectorCount = readU32(fileData, 64);
    const quint32 firstDifatSector = readU32(fileData, 68);
    const quint32 difatSectorCount = readU32(fileData, 72);
    const quint32 fatSectorCount = readU32(fileData, 44);

    QVector<quint32> fatSectors;
    for (int i = 0; i < 109; ++i) {
        const quint32 value = readU32(fileData, 76 + i * 4);
        if (value != kFreeSector)
            fatSectors.append(value);
    }
    quint32 difatSector = firstDifatSector;
    QSet<quint32> visitedDifat;
    for (quint32 i = 0; i < difatSectorCount && difatSector != kEndOfChain; ++i) {
        if (difatSector >= sectorCount || visitedDifat.contains(difatSector))
            return fail(QStringLiteral("DIFAT 链无效"));
        visitedDifat.insert(difatSector);
        QByteArray data;
        if (!sector(difatSector, &data))
            return fail(QStringLiteral("无法读取 DIFAT 扇区"));
        for (int j = 0; j < 127; ++j) {
            const quint32 value = readU32(data, j * 4);
            if (value != kFreeSector)
                fatSectors.append(value);
        }
        difatSector = readU32(data, 508);
    }
    if (fatSectors.size() < static_cast<int>(fatSectorCount))
        return fail(QStringLiteral("FAT 扇区数量不足"));

    QVector<quint32> fat(static_cast<int>(sectorCount), kFreeSector);
    for (quint32 i = 0; i < fatSectorCount; ++i) {
        if (fatSectors.at(static_cast<int>(i)) >= sectorCount)
            return fail(QStringLiteral("FAT 扇区索引越界"));
        QByteArray data;
        sector(fatSectors.at(static_cast<int>(i)), &data);
        for (int j = 0; j < 128; ++j) {
            const quint32 index = i * 128U + static_cast<quint32>(j);
            if (index < sectorCount)
                fat[static_cast<int>(index)] = readU32(data, j * 4);
        }
    }

    auto readChain =
        [&](quint32 start, quint64 byteCount, const QVector<quint32>& table, quint32 unitSize, QByteArray* result) {
            result->clear();
            if (byteCount == 0)
                return true;
            if (byteCount > static_cast<quint64>(std::numeric_limits<int>::max()))
                return false;
            quint32 current = start;
            QSet<quint32> visited;
            while (result->size() < static_cast<qsizetype>(byteCount)) {
                if (current >= static_cast<quint32>(table.size()) || current == kEndOfChain || current == kFreeSector ||
                    visited.contains(current))
                    return false;
                visited.insert(current);
                if (unitSize == kSectorSize) {
                    QByteArray data;
                    if (!sector(current, &data))
                        return false;
                    result->append(data);
                } else {
                    const qsizetype offset = static_cast<qsizetype>(current) * unitSize;
                    if (offset < 0 || offset + unitSize > result->capacity())
                        return false;
                }
                current = table.at(static_cast<int>(current));
                if (result->size() > static_cast<qsizetype>(byteCount) + unitSize)
                    break;
            }
            if (result->size() < static_cast<qsizetype>(byteCount))
                return false;
            *result = result->left(static_cast<int>(byteCount));
            return true;
        };

    QVector<quint32> miniFat;
    if (miniFatSectorCount > 0) {
        QByteArray miniFatData;
        if (!readChain(firstMiniFatSector,
                       static_cast<quint64>(miniFatSectorCount) * kSectorSize,
                       fat,
                       kSectorSize,
                       &miniFatData))
            return fail(QStringLiteral("无法读取 Mini-FAT"));
        miniFat.reserve(miniFatData.size() / 4);
        for (qsizetype offset = 0; offset + 4 <= miniFatData.size(); offset += 4)
            miniFat.append(readU32(miniFatData, offset));
    }

    QByteArray directoryData;
    QSet<quint32> directorySectors;
    quint32 currentDirectorySector = firstDirectorySector;
    while (currentDirectorySector != kEndOfChain) {
        if (currentDirectorySector >= sectorCount || directorySectors.contains(currentDirectorySector))
            return fail(QStringLiteral("目录扇区链无效"));
        directorySectors.insert(currentDirectorySector);
        QByteArray data;
        sector(currentDirectorySector, &data);
        directoryData.append(data);
        currentDirectorySector = fat.at(static_cast<int>(currentDirectorySector));
    }
    if (directoryData.isEmpty() || directoryData.size() % kDirectoryEntrySize != 0)
        return fail(QStringLiteral("目录数据无效"));

    QVector<DirectoryEntry> entries;
    entries.reserve(directoryData.size() / kDirectoryEntrySize);
    for (qsizetype offset = 0; offset < directoryData.size(); offset += kDirectoryEntrySize) {
        DirectoryEntry entry;
        const quint16 nameSize = readU16(directoryData, offset + 64);
        if (nameSize > 64 || nameSize % 2 != 0 || (nameSize != 0 && nameSize < 2))
            return fail(QStringLiteral("目录名称长度无效"));
        const int nameChars = nameSize == 0 ? 0 : nameSize / 2 - 1;
        for (int i = 0; i < nameChars; ++i)
            entry.name.append(QChar(readU16(directoryData, offset + i * 2)));
        entry.type = static_cast<quint8>(directoryData.at(offset + 66));
        if (entry.type != 0 && entry.type != 1 && entry.type != 2 && entry.type != 5)
            return fail(QStringLiteral("目录对象类型无效"));
        entry.left = readU32(directoryData, offset + 68);
        entry.right = readU32(directoryData, offset + 72);
        entry.child = readU32(directoryData, offset + 76);
        entry.startSector = readU32(directoryData, offset + 116);
        entry.streamSize = readU64(directoryData, offset + 120);
        entries.append(entry);
    }
    if (entries.isEmpty() || entries.at(0).type != 5)
        return fail(QStringLiteral("OLE 根目录条目无效"));

    QByteArray miniStreamData;
    if (entries.at(0).streamSize > 0 &&
        !readChain(entries.at(0).startSector, entries.at(0).streamSize, fat, kSectorSize, &miniStreamData))
        return fail(QStringLiteral("无法读取 Mini-Stream"));

    auto readMiniStream = [&](const DirectoryEntry& entry, QByteArray* result) {
        result->clear();
        if (entry.streamSize == 0)
            return true;
        if (entry.streamSize > static_cast<quint64>(std::numeric_limits<int>::max()) || miniFat.isEmpty())
            return false;
        quint32 current = entry.startSector;
        QSet<quint32> visited;
        while (result->size() < static_cast<qsizetype>(entry.streamSize)) {
            if (current >= static_cast<quint32>(miniFat.size()) || visited.contains(current))
                return false;
            visited.insert(current);
            const qsizetype offset = static_cast<qsizetype>(current) * kMiniSectorSize;
            if (offset + kMiniSectorSize > miniStreamData.size())
                return false;
            result->append(miniStreamData.constData() + offset, kMiniSectorSize);
            current = miniFat.at(static_cast<int>(current));
            if (result->size() > static_cast<qsizetype>(entry.streamSize) + kMiniSectorSize)
                break;
        }
        *result = result->left(static_cast<int>(entry.streamSize));
        return result->size() == static_cast<int>(entry.streamSize);
    };

    QSet<quint32> visitedEntries;
    std::function<bool(quint32, const QString&, int)> visitSiblings;
    visitSiblings = [&](quint32 index, const QString& parentPath, int depth) {
        if (index == kNoStream)
            return true;
        if (depth > kMaxDirectoryDepth || index >= static_cast<quint32>(entries.size()) ||
            visitedEntries.contains(index))
            return false;
        const DirectoryEntry& entry = entries.at(static_cast<int>(index));
        if (!visitSiblings(entry.left, parentPath, depth + 1))
            return false;
        if (entry.type != 1 && entry.type != 2)
            return false;
        const QString path = parentPath.isEmpty() ? entry.name : parentPath + "/" + entry.name;
        if (entry.type == 1) {
            if (!visitSiblings(entry.child, path, depth + 1))
                return false;
        } else {
            QByteArray streamData;
            const bool regular = entry.streamSize >= kMiniStreamCutoff;
            const bool valid = regular ? readChain(entry.startSector, entry.streamSize, fat, kSectorSize, &streamData)
                                       : readMiniStream(entry, &streamData);
            if (!valid || m_streams.contains(path))
                return false;
            m_streams.insert(path, streamData);
        }
        visitedEntries.insert(index);
        return visitSiblings(entry.right, parentPath, depth + 1);
    };
    if (!visitSiblings(entries.at(0).child, QString(), 0))
        return fail(QStringLiteral("OLE 目录树无效"));

    m_opened = true;
    return true;
}

bool OLECompoundReader::hasError() const {
    return !m_errorMessage.isEmpty();
}

QString OLECompoundReader::errorString() const {
    return m_errorMessage;
}

QStringList OLECompoundReader::streamPaths() const {
    QStringList paths = m_streams.keys();
    paths.sort(Qt::CaseSensitive);
    return paths;
}

bool OLECompoundReader::containsStream(const QString& streamPath) const {
    return m_streams.contains(streamPath);
}

bool OLECompoundReader::readStream(const QString& streamPath, QByteArray* data) const {
    if (data == nullptr)
        return false;
    const auto iterator = m_streams.constFind(streamPath);
    if (iterator == m_streams.cend())
        return false;
    *data = iterator.value();
    return true;
}

}  // namespace EasyKiConverter
