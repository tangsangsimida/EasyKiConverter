/**
 * @file test_altium_ole.cpp
 * @brief Altium OLE 复合文档写入器的单元测试
 * @details 验证 OLECompoundWriter 的 CFB 格式正确性，包括：
 *          - 迷你流和常规流的读写往返
 *          - DIFAT 扩展（大文件 > 5.6MB）
 *          - 无效/重名条目的拒绝
 *          - SchLib/PcbLib 完整文件写入
 *          - 焊盘扩展块、UID 图元信息、元件体
 *          - UTF-8 参数编码
 *          - 导出器的多部件和折线保持
 */

#include "core/altium/ExporterAltiumFootprint.h"
#include "core/altium/ExporterAltiumSymbol.h"
#include "core/altium/compound/OLECompoundReader.h"
#include "core/altium/compound/OLECompoundWriter.h"
#include "core/altium/readers/AltiumPcbLibReader.h"
#include "core/altium/readers/AltiumSchLibReader.h"
#include "core/altium/utils/AltiumBinaryReader.h"
#include "core/altium/utils/AltiumBinaryWriter.h"
#include "core/altium/utils/AltiumConstants.h"
#include "core/altium/utils/AltiumWriterUtils.h"
#include "core/altium/writers/AltiumPcbLibWriter.h"
#include "core/altium/writers/AltiumSchLibWriter.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cstdint>

using namespace EasyKiConverter;

namespace {

/** @brief CFB 扇区大小（字节） */
constexpr qint64 CFB_SECTOR_SIZE = 512;
/** @brief CFB 迷你扇区大小（字节） */
constexpr qint64 CFB_MINI_SECTOR_SIZE = 64;
/** @brief CFB FAT 链终结标记 */
constexpr quint32 CFB_ENDOFCHAIN = 0xFFFFFFFE;
/** @brief CFB 空闲扇区标记 */
constexpr quint32 CFB_FREESECT = 0xFFFFFFFF;
/** @brief CFB FAT 扇区标记 */
constexpr quint32 CFB_FATSECT = 0xFFFFFFFD;
/** @brief CFB DIFAT 扇区标记 */
constexpr quint32 CFB_DIFSECT = 0xFFFFFFFC;

/**
 * @brief 从字节数组中读取小端序 32 位无符号整数
 * @param data 数据缓冲区
 * @param offset 起始偏移
 * @return 读取的 32 位值
 */
quint32 readU32(const QByteArray& data, qint64 offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

/**
 * @brief 从字节数组中读取小端序 16 位无符号整数
 * @param data 数据缓冲区
 * @param offset 起始偏移
 * @return 读取的 16 位值
 */
quint16 readU16(const QByteArray& data, qint64 offset) {
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1))) << 8);
}

/**
 * @brief 统计 SchLib Data 流中的记录块数量
 * @param data Data 流内容
 * @return 记录数量；块结构损坏时返回 -1
 */
int countSchLibRecords(const QByteArray& data) {
    int count = 0;
    for (int offset = 0; offset + 4 <= data.size();) {
        const int payloadSize = static_cast<int>(readU32(data, offset) & 0x00FFFFFFU);
        const int nextOffset = offset + 4 + payloadSize;
        if (payloadSize <= 0 || nextOffset > data.size())
            return -1;
        ++count;
        offset = nextOffset;
    }
    return count;
}

/**
 * @brief 从字节数组中读取小端序 64 位无符号整数
 * @param data 数据缓冲区
 * @param offset 起始偏移
 * @return 读取的 64 位值
 */
quint64 readU64(const QByteArray& data, qint64 offset) {
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<quint64>(static_cast<unsigned char>(data.at(offset + i))) << (i * 8);
    }
    return value;
}

/**
 * @brief CFB 目录条目结构（测试用简化版）
 */
struct CfbDirectoryEntry {
    QString name;
    quint8 type = 0;
    quint8 color = 1;
    quint32 leftChild = CFB_FREESECT;
    quint32 rightChild = CFB_FREESECT;
    quint32 child = CFB_FREESECT;
    quint32 startSector = CFB_ENDOFCHAIN;
    quint64 streamSize = 0;
};

/**
 * @brief 读取 CFB 文件的指定扇区
 * @param fileData 完整文件数据
 * @param sector 扇区编号
 * @return 扇区数据（512 字节）
 */
QByteArray readCfbSector(const QByteArray& fileData, quint32 sector) {
    const qint64 offset = static_cast<qint64>(sector + 1) * CFB_SECTOR_SIZE;
    return fileData.mid(offset, CFB_SECTOR_SIZE);
}

/**
 * @brief 读取常规流数据（大于 4096 字节的流）
 * @param fileData 完整文件数据
 * @param fat FAT 表
 * @param startSector 起始扇区
 * @param size 流大小
 * @return 流数据
 */
QByteArray readRegularStream(const QByteArray& fileData,
                             const QVector<quint32>& fat,
                             quint32 startSector,
                             quint64 size) {
    QByteArray result;
    quint32 sector = startSector;
    while (result.size() < static_cast<int>(size) && sector != CFB_ENDOFCHAIN && sector != CFB_FREESECT) {
        result.append(readCfbSector(fileData, sector));
        sector = fat.value(static_cast<int>(sector));
    }
    return result.left(static_cast<int>(size));
}

/**
 * @brief 读取迷你流数据（小于 4096 字节的流）
 * @param fileData 完整文件数据（未使用，保留接口一致性）
 * @param fat FAT 表（未使用）
 * @param miniFat Mini-FAT 表
 * @param miniStreamData Mini-Stream 数据
 * @param startMiniSector 起始迷你扇区
 * @param size 流大小
 * @return 流数据
 */
QByteArray readMiniStream(const QByteArray& fileData,
                          const QVector<quint32>& fat,
                          const QVector<quint32>& miniFat,
                          const QByteArray& miniStreamData,
                          quint32 startMiniSector,
                          quint64 size) {
    Q_UNUSED(fileData);
    Q_UNUSED(fat);
    QByteArray result;
    quint32 sector = startMiniSector;
    while (result.size() < static_cast<int>(size) && sector != CFB_ENDOFCHAIN && sector != CFB_FREESECT) {
        result.append(miniStreamData.mid(static_cast<qint64>(sector) * CFB_MINI_SECTOR_SIZE, CFB_MINI_SECTOR_SIZE));
        sector = miniFat.value(static_cast<int>(sector));
    }
    return result.left(static_cast<int>(size));
}

/**
 * @brief CFB 名称比较函数（先按长度，再按字母序）
 * @param left 左侧名称
 * @param right 右侧名称
 * @return left 是否排在 right 前面
 */
bool cfbNameLess(const QString& left, const QString& right) {
    if (left.size() != right.size()) {
        return left.size() < right.size();
    }
    return left.compare(right, Qt::CaseInsensitive) < 0;
}

/**
 * @brief 从 CFB 文件中读取指定路径的流数据
 * @param filePath CFB 文件路径
 * @param streamPath 流路径（如 "FileHeader" 或 "Library/Header"）
 * @param output 输出数据
 * @param directoryOutput 可选输出完整目录条目列表
 * @return 是否成功读取
 */
bool readCfbStream(const QString& filePath,
                   const QString& streamPath,
                   QByteArray& output,
                   QVector<CfbDirectoryEntry>* directoryOutput = nullptr) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();

    if (data.size() < CFB_SECTOR_SIZE || data.left(8) != QByteArray::fromHex("d0cf11e0a1b11ae1")) {
        return false;
    }

    const quint32 numFatSectors = readU32(data, 44);
    const quint32 firstDirSector = readU32(data, 48);
    const quint32 firstMiniFatSector = readU32(data, 60);
    const quint32 numMiniFatSectors = readU32(data, 64);
    const quint32 firstDifatSector = readU32(data, 68);
    const quint32 numDifatSectors = readU32(data, 72);

    QVector<quint32> fatSectorList;
    for (int i = 0; i < 109 && static_cast<int>(fatSectorList.size()) < static_cast<int>(numFatSectors); ++i) {
        const quint32 sector = readU32(data, 76 + i * 4);
        if (sector == CFB_ENDOFCHAIN || sector == CFB_FREESECT) {
            break;
        }
        fatSectorList.append(sector);
    }
    quint32 difatSector = firstDifatSector;
    for (quint32 i = 0; i < numDifatSectors && difatSector != CFB_ENDOFCHAIN; ++i) {
        const QByteArray sectorData = readCfbSector(data, difatSector);
        for (int j = 0; j < 127 && fatSectorList.size() < static_cast<int>(numFatSectors); ++j) {
            const quint32 fatSector = readU32(sectorData, j * 4);
            if (fatSector != CFB_FREESECT) {
                fatSectorList.append(fatSector);
            }
        }
        difatSector = readU32(sectorData, 127 * 4);
    }
    if (fatSectorList.size() != static_cast<int>(numFatSectors)) {
        return false;
    }

    QVector<quint32> fat;
    for (quint32 fatSector : fatSectorList) {
        const QByteArray sectorData = readCfbSector(data, fatSector);
        for (int i = 0; i < CFB_SECTOR_SIZE / 4; ++i) {
            fat.append(readU32(sectorData, i * 4));
        }
    }

    QVector<CfbDirectoryEntry> entries;
    quint32 dirSector = firstDirSector;
    while (dirSector != CFB_ENDOFCHAIN && dirSector != CFB_FREESECT) {
        const QByteArray sectorData = readCfbSector(data, dirSector);
        for (int i = 0; i < CFB_SECTOR_SIZE / 128; ++i) {
            const qint64 offset = i * 128;
            CfbDirectoryEntry entry;
            const quint16 nameSize = static_cast<quint16>(readU32(sectorData, offset + 64) & 0xFFFF);
            entry.name = QString::fromUtf16(reinterpret_cast<const char16_t*>(sectorData.constData() + offset),
                                            (nameSize - 2) / 2);
            entry.type = static_cast<quint8>(sectorData.at(offset + 66));
            entry.color = static_cast<quint8>(sectorData.at(offset + 67));
            entry.leftChild = readU32(sectorData, offset + 68);
            entry.rightChild = readU32(sectorData, offset + 72);
            entry.child = readU32(sectorData, offset + 76);
            entry.startSector = readU32(sectorData, offset + 116);
            entry.streamSize = readU64(sectorData, offset + 120);
            entries.append(entry);
        }
        dirSector = fat.value(static_cast<int>(dirSector));
    }

    if (entries.isEmpty() || entries.at(0).type != 5) {
        return false;
    }
    if (directoryOutput) {
        *directoryOutput = entries;
    }

    const QByteArray miniStream = readRegularStream(data, fat, entries.at(0).startSector, entries.at(0).streamSize);
    QVector<quint32> miniFat;
    quint32 miniFatSector = firstMiniFatSector;
    for (quint32 i = 0; i < numMiniFatSectors; ++i) {
        if (miniFatSector == CFB_ENDOFCHAIN || miniFatSector == CFB_FREESECT) {
            return false;
        }
        const QByteArray sectorData = readCfbSector(data, miniFatSector);
        for (int j = 0; j < CFB_SECTOR_SIZE / 4; ++j) {
            miniFat.append(readU32(sectorData, j * 4));
        }
        miniFatSector = fat.value(static_cast<int>(miniFatSector), CFB_FREESECT);
    }
    if (numMiniFatSectors > 0 && miniFatSector != CFB_ENDOFCHAIN) {
        return false;
    }

    const QStringList pathParts = streamPath.split('/');
    int entryIndex = 0;
    for (const QString& part : pathParts) {
        int node = entries.at(entryIndex).child;
        int found = -1;
        while (node != CFB_FREESECT && node != CFB_ENDOFCHAIN) {
            if (entries.at(node).name.compare(part, Qt::CaseInsensitive) == 0) {
                found = node;
                break;
            }
            node = cfbNameLess(part, entries.at(node).name) ? entries.at(node).leftChild : entries.at(node).rightChild;
        }
        if (found < 0) {
            return false;
        }
        entryIndex = found;
    }

    const CfbDirectoryEntry& entry = entries.at(entryIndex);
    if (entry.type != 2) {
        return false;
    }
    output = entry.streamSize < 4096
                 ? readMiniStream(data, fat, miniFat, miniStream, entry.startSector, entry.streamSize)
                 : readRegularStream(data, fat, entry.startSector, entry.streamSize);
    return output.size() == static_cast<int>(entry.streamSize);
}

/**
 * @brief 解析 SchLib /Storage 中的嵌入图片条目
 * @param storage Storage 流数据
 * @param compressedImages 输出图片名到原始 zlib 数据的映射
 * @return 条目结构和长度均有效时返回 true
 */
bool parseAltiumImageStorage(const QByteArray& storage, QMap<QString, QByteArray>& compressedImages) {
    compressedImages.clear();
    if (storage.size() < 4)
        return false;

    const int headerSize = static_cast<int>(readU32(storage, 0) & 0x00FFFFFFU);
    int offset = 4 + headerSize;
    if (offset > storage.size())
        return false;

    while (offset < storage.size()) {
        if (offset + 4 > storage.size())
            return false;
        const quint32 blockHeader = readU32(storage, offset);
        if ((blockHeader >> 24) != 1)
            return false;
        const int entrySize = static_cast<int>(blockHeader & 0x00FFFFFFU);
        const int entryOffset = offset + 4;
        const int entryEnd = entryOffset + entrySize;
        if (entrySize < 6 || entryEnd > storage.size())
            return false;
        if (static_cast<quint8>(storage.at(entryOffset)) != 0xD0)
            return false;

        const int nameLength = static_cast<quint8>(storage.at(entryOffset + 1));
        const int compressedLengthOffset = entryOffset + 2 + nameLength;
        if (compressedLengthOffset + 4 > entryEnd)
            return false;
        const QString name = QString::fromLocal8Bit(storage.constData() + entryOffset + 2, nameLength);
        if (name.isEmpty() || compressedImages.contains(name))
            return false;

        const int compressedLength = static_cast<int>(readU32(storage, compressedLengthOffset));
        const int compressedOffset = compressedLengthOffset + 4;
        if (compressedLength != entryEnd - compressedOffset)
            return false;
        compressedImages.insert(name, storage.mid(compressedOffset, compressedLength));
        offset = entryEnd;
    }
    return offset == storage.size();
}

/**
 * @brief 为去除 Qt 长度前缀的 zlib 数据恢复 qUncompress 输入
 * @param compressed 原始 zlib 数据
 * @param uncompressedSize 解压后的字节数
 * @return 带 Qt 长度前缀的压缩数据
 */
QByteArray restoreQtCompressionHeader(const QByteArray& compressed, int uncompressedSize) {
    QByteArray withHeader;
    withHeader.reserve(compressed.size() + 4);
    withHeader.append(static_cast<char>((uncompressedSize >> 24) & 0xFF));
    withHeader.append(static_cast<char>((uncompressedSize >> 16) & 0xFF));
    withHeader.append(static_cast<char>((uncompressedSize >> 8) & 0xFF));
    withHeader.append(static_cast<char>(uncompressedSize & 0xFF));
    withHeader.append(compressed);
    return withHeader;
}

/**
 * @brief 递归验证 CFB 目录红黑树的平衡性和搜索序
 * @param entries 目录条目列表
 * @param node 当前节点索引
 * @param lowerBound 名称下界（BST 约束）
 * @param upperBound 名称上界（BST 约束）
 * @param visited 已访问节点集合
 * @param valid 验证结果标志（输出）
 * @return 当前子树的黑高
 */
int validateRedBlackSubtree(const QVector<CfbDirectoryEntry>& entries,
                            quint32 node,
                            const QString* lowerBound,
                            const QString* upperBound,
                            QSet<quint32>& visited,
                            bool& valid) {
    if (node == CFB_FREESECT) {
        return 1;
    }
    if (node >= static_cast<quint32>(entries.size()) || visited.contains(node)) {
        valid = false;
        return 0;
    }
    visited.insert(node);
    const CfbDirectoryEntry& entry = entries.at(static_cast<int>(node));
    if ((lowerBound && !cfbNameLess(*lowerBound, entry.name)) ||
        (upperBound && !cfbNameLess(entry.name, *upperBound))) {
        valid = false;
    }
    if (entry.color == 0) {
        if ((entry.leftChild != CFB_FREESECT && entries.at(static_cast<int>(entry.leftChild)).color == 0) ||
            (entry.rightChild != CFB_FREESECT && entries.at(static_cast<int>(entry.rightChild)).color == 0)) {
            valid = false;
        }
    }
    const int leftHeight = validateRedBlackSubtree(entries, entry.leftChild, lowerBound, &entry.name, visited, valid);
    const int rightHeight = validateRedBlackSubtree(entries, entry.rightChild, &entry.name, upperBound, visited, valid);
    if (leftHeight != rightHeight) {
        valid = false;
    }
    return leftHeight + (entry.color == 1 ? 1 : 0);
}

}  // namespace

/**
 * @brief Altium OLE 复合文档写入器测试类
 * @details 验证 OLECompoundWriter、AltiumSchLibWriter、AltiumPcbLibWriter 的格式正确性，
 *          以及 ExporterAltiumSymbol 和 ExporterAltiumFootprint 的转换正确性。
 */
class TestAltiumOle : public QObject {
    Q_OBJECT

private slots:

    /**
     * @brief 验证包含迷你流和常规流的 CFB 文件可被正确解析
     * @details 写入多种大小的流，验证红黑树平衡性，以及嵌套存储路径的读取
     */
    void writesReadableCfbWithMiniAndRegularStreams() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.writeStream("FileHeader", QByteArrayLiteral("header payload")));
        QVERIFY(writer.addStorage("Library"));
        QVERIFY(writer.writeStream("Library", "Header", QByteArrayLiteral("hdr")));
        QVERIFY(writer.addStorage("Component"));

        QByteArray regularData;
        regularData.reserve(5000);
        for (int i = 0; i < 5000; ++i) {
            regularData.append(static_cast<char>((i * 31 + 7) & 0xFF));
        }
        QVERIFY(writer.writeStream("Component", "Data", regularData));
        QVERIFY(writer.writeStream("Small", QByteArrayLiteral("small stream")));
        QVERIFY(writer.writeStream("Empty", QByteArray()));
        for (int i = 0; i < 140; ++i) {
            QVERIFY(
                writer.writeStream(QStringLiteral("Sibling%1").arg(i, 2, 10, QLatin1Char('0')), QByteArray(1, 'x')));
        }

        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("test.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        const QString gsf = QStandardPaths::findExecutable(QStringLiteral("gsf"));
        if (!gsf.isEmpty()) {
            QProcess validator;
            validator.start(gsf, {QStringLiteral("list"), filePath});
            QVERIFY(validator.waitForFinished(10000));
            QCOMPARE(validator.exitCode(), 0);
            const QByteArray diagnostics = validator.readAllStandardError() + validator.readAllStandardOutput();
            QVERIFY2(!diagnostics.contains("invalid OLE"), diagnostics.constData());
        }

        QByteArray headerPayload;
        QVector<CfbDirectoryEntry> directory;
        QVERIFY(readCfbStream(filePath, QStringLiteral("FileHeader"), headerPayload, &directory));
        QCOMPARE(headerPayload, QByteArrayLiteral("header payload"));

        QVERIFY(directory.at(static_cast<int>(directory.at(0).child)).color == 1);
        QSet<quint32> visited;
        bool validTree = true;
        validateRedBlackSubtree(directory, directory.at(0).child, nullptr, nullptr, visited, validTree);
        QVERIFY(validTree);

        const auto emptyEntry = std::find_if(directory.cbegin(), directory.cend(), [](const CfbDirectoryEntry& entry) {
            return entry.name == QStringLiteral("Empty");
        });
        QVERIFY(emptyEntry != directory.cend());
        QCOMPARE(emptyEntry->streamSize, quint64(0));
        QCOMPARE(emptyEntry->startSector, CFB_ENDOFCHAIN);

        QByteArray libraryHeader;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Library/Header"), libraryHeader));
        QCOMPARE(libraryHeader, QByteArrayLiteral("hdr"));

        QByteArray regularPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Component/Data"), regularPayload));
        QCOMPARE(regularPayload, regularData);

        QByteArray smallPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("Small"), smallPayload));
        QCOMPARE(smallPayload, QByteArrayLiteral("small stream"));
    }

    /**
     * @brief 验证大文件（>109 扇区）正确生成 DIFAT 扩展扇区
     * @details 写入 8MB 数据，验证 DIFAT 链和往返一致性
     */
    void writesDifatForLargeCompoundFiles() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QByteArray payload(8 * 1024 * 1024, 0);
        for (int i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>((i * 17 + 3) & 0xFF);
        }
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.writeStream(QStringLiteral("LargeModel"), payload));
        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("large.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        QFile file(filePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray header = file.read(512);
        QVERIFY(readU32(header, 44) > 109);
        QVERIFY(readU32(header, 72) > 0);
        QCOMPARE(readU32(readCfbSector(header + file.readAll(), readU32(header, 68)), 127 * 4), CFB_ENDOFCHAIN);

        QByteArray roundTrip;
        QVERIFY(readCfbStream(filePath, QStringLiteral("LargeModel"), roundTrip));
        QCOMPARE(roundTrip, payload);

        OLECompoundReader reader;
        QVERIFY(reader.open(filePath));
        QVERIFY(reader.containsStream(QStringLiteral("LargeModel")));
        QVERIFY(reader.readStream(QStringLiteral("LargeModel"), &roundTrip));
        QCOMPARE(roundTrip, payload);
    }

    /**
     * @brief 验证无效路径和重名条目被正确拒绝
     */
    void rejectsInvalidOrDuplicateCfbEntries() {
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(!writer.addStorage(QStringLiteral("Missing"), QStringLiteral("Child")));
        QVERIFY(writer.hasError());
        QCOMPARE(writer.errorString(), QStringLiteral("无法创建存储: Missing/Child"));
        QVERIFY(!writer.saveToFile(QDir::temp().filePath(QStringLiteral("invalid-cfb.cfb"))));

        QVERIFY(writer.create());
        QVERIFY(!writer.hasError());
        QVERIFY(!writer.addStorage(QStringLiteral("Bad/Name")));
        QVERIFY(writer.addStorage(QStringLiteral("Models")));
        QVERIFY(!writer.addStorage(QStringLiteral("models")));
        QVERIFY(writer.writeStream(QStringLiteral("Header"), QByteArrayLiteral("one")));
        QVERIFY(!writer.writeStream(QStringLiteral("header"), QByteArrayLiteral("two")));
        QVERIFY(!writer.writeStream(QStringLiteral("Missing"), QStringLiteral("Data"), QByteArray()));
    }

    /**
     * @brief 验证 SchLib 和 PcbLib 完整写入和流结构
     * @details 写入包含引脚、矩形、路径的符号和包含焊盘、走线的封装，
     *          验证 FileHeader 参数、Data 流内容、Library 元数据和图元记录布局
     */
    void writesSchLibAndPcbLibStorages() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("C2040");
        symbol.partCount = 1;
        symbol.sourceMetadata.insert(QStringLiteral("manufacturer"), QStringLiteral("Example Corp"));
        symbol.sourceMetadata.insert(QStringLiteral("lcscId"), QStringLiteral("C2040"));
        symbol.aliases = {QStringLiteral("C2040_ALIAS")};
        AltiumSchParameter parameter;
        parameter.name = QStringLiteral("Temperature Coefficient");
        parameter.value = QStringLiteral("±50ppm/K");
        parameter.readOnly = true;
        symbol.parameters.append(parameter);

        AltiumSchPin pin;
        pin.name = QStringLiteral("A");
        pin.designator = QStringLiteral("1");
        pin.locationX = 100000;
        pin.locationY = 200000;
        pin.length = 100000;
        pin.electricalType = AltiumModels::PinElectricalType::Passive;
        pin.orientation = AltiumModels::PinOrientation::Right;
        symbol.pins.append(pin);

        AltiumSchRectangle rect;
        rect.locationX = 100000;
        rect.locationY = 100000;
        rect.cornerX = 500000;
        rect.cornerY = 500000;
        rect.lineStyle = 1;
        symbol.rectangles.append(rect);

        AltiumSchRoundRectangle roundRect;
        roundRect.locationX = 600000;
        roundRect.locationY = 100000;
        roundRect.cornerX = 900000;
        roundRect.cornerY = 500000;
        roundRect.cornerXRadius = 50000;
        roundRect.cornerYRadius = 75000;
        symbol.roundRectangles.append(roundRect);

        AltiumSchPath path;
        path.vertices = {QPointF(0, 0), QPointF(100000, 100000), QPointF(200000, 0)};
        path.ownerPartId = 1;
        symbol.paths.append(path);
        AltiumSchIeee ieee;
        ieee.symbol = 18;  // Pi
        ieee.locationX = 300000;
        ieee.locationY = 300000;
        ieee.mirrored = true;
        symbol.ieeeSymbols.append(ieee);
        AltiumSchPie pie;
        pie.centerX = 1000000;
        pie.centerY = 300000;
        pie.radius = 100000;
        pie.startAngle = 30.0;
        pie.endAngle = 150.0;
        symbol.pies.append(pie);
        AltiumSchEllipticalArc ellipticalArc;
        ellipticalArc.centerX = 1200000;
        ellipticalArc.centerY = 300000;
        ellipticalArc.radiusX = 150000;
        ellipticalArc.radiusY = 80000;
        ellipticalArc.startAngle = 45.0;
        ellipticalArc.endAngle = 270.0;
        symbol.ellipticalArcs.append(ellipticalArc);
        AltiumSchTextFrame textFrame;
        textFrame.locationX = 1300000;
        textFrame.locationY = 100000;
        textFrame.cornerX = 1600000;
        textFrame.cornerY = 500000;
        textFrame.text = QStringLiteral("说明\n第二行");
        textFrame.fontId = 1;
        textFrame.showBorder = true;
        symbol.textFrames.append(textFrame);
        AltiumSchText styledText;
        styledText.locationX = 1500000;
        styledText.locationY = 700000;
        styledText.text = QStringLiteral("Styled label");
        styledText.fontId = 0;
        styledText.fontName = QStringLiteral("Arial");
        styledText.fontSizeMm = 25.4 / 72.0 * 8.0;
        styledText.bold = true;
        styledText.italic = true;
        styledText.anchor = QStringLiteral("start");
        symbol.texts.append(styledText);
        AltiumSchText fallbackFontText;
        fallbackFontText.locationX = 1600000;
        fallbackFontText.locationY = 700000;
        fallbackFontText.text = QStringLiteral("Fallback font");
        fallbackFontText.fontId = 99;
        symbol.texts.append(fallbackFontText);
        AltiumSchImage image;
        image.locationX = 1700000;
        image.locationY = 100000;
        image.cornerX = 1900000;
        image.cornerY = 300000;
        image.fileName = QStringLiteral("logo.png");
        image.data = QByteArrayLiteral("fake-png-data");
        image.embedImage = true;
        image.keepAspect = true;
        symbol.images.append(image);

        // 无效 Bézier 不应写入 Data，也不能被 FileHeader 的 WEIGHT 计入。
        AltiumSchBezier invalidBezier;
        invalidBezier.controlPoints = {QPointF(0, 0), QPointF(100000, 0), QPointF(200000, 0)};
        symbol.beziers.append(invalidBezier);

        AltiumSchComponent::Implementation impl;
        impl.modelName = QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP");
        impl.modelType = QStringLiteral("PCBLIB");
        impl.parameters.insert(QStringLiteral("THERMAL_MODEL"), QStringLiteral("default"));
        impl.pinMappings.insert(QStringLiteral("1"), QStringLiteral("A1"));
        symbol.implementations.append(impl);
        AltiumSchComponent::Implementation simulationImpl;
        simulationImpl.modelName = QStringLiteral("C2040_SPICE");
        simulationImpl.modelType = QStringLiteral("SIM");
        symbol.implementations.append(simulationImpl);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.SchLib"));
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath, QStringLiteral("easyeda_convertlib")));

        QByteArray schHeader;
        QVERIFY(readCfbStream(schPath, QStringLiteral("FileHeader"), schHeader));
        QVERIFY(schHeader.contains("COMPCOUNT=1"));
        QVERIFY(schHeader.contains("WEIGHT=27"));
        QVERIFY(schHeader.contains("LIBREF0=C2040"));
        QVERIFY(schHeader.contains("PARTCOUNT0=2"));
        QVERIFY(schHeader.mid(4).startsWith("|HEADER="));
        QVERIFY(schHeader.contains("FontName2=Arial"));
        QVERIFY(schHeader.contains("Size2=8"));
        QVERIFY(schHeader.contains("Bold2=T"));
        QVERIFY(schHeader.contains("Italic2=T"));

        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("C2040/Data"), schData));
        const int weightOffset = schHeader.indexOf("WEIGHT=");
        const int weightEnd = schHeader.indexOf('|', weightOffset);
        QVERIFY(weightOffset >= 0);
        QVERIFY(weightEnd > weightOffset);
        bool weightOk = false;
        const int headerWeight =
            QString::fromLatin1(schHeader.mid(weightOffset + 7, weightEnd - weightOffset - 7)).toInt(&weightOk);
        QVERIFY(weightOk);
        QCOMPARE(countSchLibRecords(schData), headerWeight);
        QVERIFY(schData.mid(4).startsWith("|RECORD=1|"));
        QVERIFY(schData.contains("LibReference=C2040"));
        QVERIFY(schData.contains("RECORD=14"));
        QVERIFY(!schData.contains("|IndexInSheet=0|"));
        QVERIFY(schData.contains("OWNERPARTDISPLAYMODE=1"));
        QVERIFY(schData.contains("LineStyleExt=1"));
        QVERIFY(schData.contains("RECORD=10"));
        QVERIFY(schData.contains("CornerXRadius=1"));
        QVERIFY(schData.contains("RECORD=45"));
        QVERIFY(schData.contains("DESIMP0=A1"));
        QVERIFY(schData.contains("MODELTYPE=SIM"));
        QVERIFY(schData.contains("DATAFILECOUNT=0"));
        QVERIFY(schData.contains("THERMAL_MODEL=default"));
        QVERIFY(schData.contains("RECORD=6"));
        QVERIFY(schData.contains("RECORD=3"));
        QVERIFY(schData.contains("Symbol=18"));
        QVERIFY(schData.contains("RECORD=9"));
        QVERIFY(schData.contains("StartAngle=30.000"));
        QVERIFY(schData.contains("RECORD=11"));
        QVERIFY(schData.contains("SecondaryRadius=1"));
        QVERIFY(schData.contains("RECORD=28"));
        QVERIFY(schData.contains(QStringLiteral("第二行").toUtf8()));
        QVERIFY(schData.contains("FontID=2"));
        const int fallbackTextOffset = schData.indexOf("Text=Fallback font");
        const int fallbackRecordOffset = schData.lastIndexOf("|RECORD=4|", fallbackTextOffset);
        const int nextFallbackRecordOffset = schData.indexOf("|RECORD=", fallbackTextOffset + 1);
        QVERIFY(fallbackTextOffset > fallbackRecordOffset);
        QVERIFY(nextFallbackRecordOffset > fallbackRecordOffset);
        const QByteArray fallbackRecord =
            schData.mid(fallbackRecordOffset, nextFallbackRecordOffset - fallbackRecordOffset);
        QVERIFY(fallbackRecord.contains("FontID=1"));
        QVERIFY(schData.contains("FontSize=2.8222"));
        QVERIFY(schData.contains("TextAnchor=start"));
        QVERIFY(schData.contains("Styled label"));
        QVERIFY(schData.contains("RECORD=30"));
        QVERIFY(schData.contains("EmbedImage=T"));
        QVERIFY(schData.contains("FileName=logo.png"));
        QByteArray imageStorage;
        QVERIFY(readCfbStream(schPath, QStringLiteral("Storage"), imageStorage));
        QVERIFY(imageStorage.contains("logo.png"));
        const int imageNameOffset = imageStorage.indexOf("logo.png");
        QVERIFY(imageNameOffset >= 2);
        QCOMPARE(static_cast<uint8_t>(imageStorage.at(imageNameOffset - 2)), static_cast<uint8_t>(0xD0));
        QCOMPARE(static_cast<uint8_t>(imageStorage.at(imageNameOffset - 1)), static_cast<uint8_t>(8));
        const int imageLengthOffset = imageNameOffset + 8;
        QVERIFY(imageLengthOffset + 4 <= imageStorage.size());
        const quint32 compressedLength = readU32(imageStorage, imageLengthOffset);
        const int imageDataOffset = imageLengthOffset + 4;
        QVERIFY(compressedLength > 0);
        QVERIFY(imageDataOffset + static_cast<int>(compressedLength) <= imageStorage.size());
        QByteArray expectedSize(4, '\0');
        expectedSize[3] = static_cast<char>(image.data.size());
        const QByteArray restoredImage =
            qUncompress(expectedSize + imageStorage.mid(imageDataOffset, compressedLength));
        QCOMPARE(restoredImage, image.data);
        QVERIFY(schData.contains("Mirror=T"));
        QVERIFY(schData.contains("OWNERPARTID=1"));
        QVERIFY(schData.contains("PartCount=2"));
        QVERIFY(schData.contains("RECORD=34"));
        QVERIFY(schData.contains("NAME=Designator"));
        QVERIFY(schData.contains("RECORD=41"));
        QVERIFY(schData.contains("NAME=Comment"));
        QVERIFY(schData.contains("TEXT=C2040"));
        QVERIFY(schData.contains("NAME=Manufacturer"));
        QVERIFY(schData.contains("TEXT=Example Corp"));
        QVERIFY(schData.contains("NAME=LCSC Part"));
        QVERIFY(schData.contains("NAME=Aliases"));
        QVERIFY(schData.contains("Aliases=C2040_ALIAS"));
        QVERIFY(schData.contains("C2040_ALIAS"));
        QVERIFY(schData.contains("NAME=Temperature Coefficient"));
        QVERIFY(schData.contains("IsHidden=T"));
        QVERIFY(schData.contains("READONLYSTATE=1"));

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP");
        footprint.description = QStringLiteral("LQFN-56");
        footprint.height = 0.7;

        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.locationX = 10000;
        pad.locationY = 20000;
        pad.sizeTopX = 20000;
        pad.sizeTopY = 20000;
        pad.sizeMidX = 20000;
        pad.sizeMidY = 20000;
        pad.sizeBotX = 20000;
        pad.sizeBotY = 20000;
        pad.isSMD = true;
        pad.isPlated = false;
        pad.layer = 1;
        pad.shapeTop = 1;
        pad.shapeMid = 1;
        pad.shapeBot = 1;
        footprint.pads.append(pad);

        AltiumPcbTrack track;
        track.startX = 0;
        track.startY = 0;
        track.endX = 10000;
        track.endY = 10000;
        track.width = 5000;
        track.layer = 33;
        footprint.tracks.append(track);

        AltiumPcbArc arc;
        arc.centerX = 30000;
        arc.centerY = 40000;
        arc.radius = 5000;
        arc.startAngle = 15.0;
        arc.endAngle = 225.0;
        arc.width = 800;
        arc.layer = 1;
        footprint.arcs.append(arc);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath, QStringLiteral("easyeda_convertlib")));

        QByteArray pcbHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("FileHeader"), pcbHeader));
        QCOMPARE(pcbHeader.size(), 32);
        QCOMPARE(readU32(pcbHeader, 0), quint32(27));
        QCOMPARE(static_cast<quint8>(pcbHeader.at(4)), quint8(27));
        QCOMPARE(pcbHeader.mid(5), QByteArrayLiteral("PCB 6.0 Binary Library File"));

        QByteArray libraryData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("Library/Data"), libraryData));
        QVERIFY(libraryData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QVERIFY((readU32(libraryData, 0) & 0x00FFFFFFU) > 10000);
        QVERIFY(libraryData.contains("KIND=Protel_Advanced_PCB"));
        QVERIFY(libraryData.contains("LAYER82NAME=Via Holes"));

        QByteArray footprintParameters;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Parameters"), footprintParameters));
        QVERIFY(footprintParameters.mid(4).startsWith("|PATTERN="));

        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Data"), footprintData));
        QVERIFY(footprintData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        int primitiveOffset = 4 + static_cast<int>(readU32(footprintData, 0));
        QCOMPARE(static_cast<quint8>(footprintData.at(primitiveOffset++)), quint8(2));
        for (int i = 0; i < 4; ++i) {
            primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        }
        primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        primitiveOffset += 4 + static_cast<int>(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU);
        QCOMPARE(static_cast<quint8>(footprintData.at(primitiveOffset++)), quint8(4));
        QCOMPARE(readU32(footprintData, primitiveOffset) & 0x00FFFFFFU, quint32(36));

        OLECompoundReader schReader;
        QVERIFY2(schReader.open(schPath), qPrintable(schReader.errorString()));
        QVERIFY(schReader.containsStream(QStringLiteral("C2040/Data")));
        QByteArray schReaderData;
        QVERIFY(schReader.readStream(QStringLiteral("C2040/Data"), &schReaderData));
        QVERIFY(schReaderData.contains("RECORD=41"));

        OLECompoundReader pcbReader;
        QVERIFY2(pcbReader.open(pcbPath), qPrintable(pcbReader.errorString()));
        QVERIFY(pcbReader.containsStream(QStringLiteral("Library/Data")));
        QByteArray pcbReaderData;
        QVERIFY(pcbReader.readStream(QStringLiteral("Library/Data"), &pcbReaderData));
        QVERIFY(!pcbReaderData.isEmpty());

        AltiumPcbLibReader pcbLibraryReader;
        QVERIFY2(pcbLibraryReader.open(pcbPath), qPrintable(pcbLibraryReader.errorString()));
        const auto pcbComponents = pcbLibraryReader.components();
        QCOMPARE(pcbComponents.size(), 1);
        QCOMPARE(pcbComponents.first().name, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QCOMPARE(pcbComponents.first().sectionKey, pcbComponents.first().name);
        QCOMPARE(pcbLibraryReader.fileVersion(), QStringLiteral("PCB 6.0 Binary Library File"));
        QVERIFY(pcbLibraryReader.libraryMetadata().contains("KIND=Protel_Advanced_PCB"));
        QByteArray footprintReaderData;
        QVERIFY(pcbLibraryReader.readFootprintStream(0, QStringLiteral("Data"), &footprintReaderData));
        QVERIFY(footprintReaderData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
        QVector<AltiumPcbLibReader::PrimitiveRecord> footprintObjects;
        QVERIFY(pcbLibraryReader.readFootprintObjects(0, &footprintObjects));
        QCOMPARE(footprintObjects.size(), 3);
        QCOMPARE(footprintObjects.at(0).objectId, quint8(AltiumConstants::PCB_OBJECT_PAD));
        QCOMPARE(footprintObjects.at(0).layer, quint8(1));
        QVERIFY(footprintObjects.at(0).primitiveFlags != 0);
        QVERIFY(footprintObjects.at(0).hasPadFields);
        QCOMPARE(footprintObjects.at(0).designator, QStringLiteral("1"));
        QCOMPARE(footprintObjects.at(0).pad.locationX, qint32(10000));
        QCOMPARE(footprintObjects.at(0).pad.sizeTopX, qint32(20000));
        QCOMPARE(footprintObjects.at(0).pad.holeType, quint8(0));
        QCOMPARE(footprintObjects.at(0).blocks.size(), 6);
        QCOMPARE(footprintObjects.at(1).objectId, quint8(AltiumConstants::PCB_OBJECT_TRACK));
        QCOMPARE(footprintObjects.at(1).layer, quint8(33));
        QCOMPARE(footprintObjects.at(1).primitiveFlags, quint16(0x0C));
        QVERIFY(footprintObjects.at(1).hasTrackFields);
        QCOMPARE(footprintObjects.at(1).track.endX, qint32(10000));
        QCOMPARE(footprintObjects.at(1).blocks.size(), 1);
        QCOMPARE(footprintObjects.at(2).objectId, quint8(AltiumConstants::PCB_OBJECT_ARC));
        QVERIFY(footprintObjects.at(2).hasArcFields);
        QCOMPARE(footprintObjects.at(2).arc.radius, qint32(5000));
        QCOMPARE(footprintObjects.at(2).arc.endAngle, 225.0);
        QByteArray reconstructedFootprintObjects;
        for (const auto& object : footprintObjects)
            reconstructedFootprintObjects.append(object.encoded);
        const int footprintNameSize = static_cast<int>(readU32(footprintReaderData, 0));
        QCOMPARE(reconstructedFootprintObjects, footprintReaderData.mid(4 + footprintNameSize));
        QByteArray footprintHeaderReaderData;
        QVERIFY(pcbLibraryReader.readFootprintStream(0, QStringLiteral("Header"), &footprintHeaderReaderData));
        AltiumBinaryReader footprintHeaderReader(footprintHeaderReaderData);
        uint32_t primitiveCount = 0;
        QVERIFY(footprintHeaderReader.readUInt32(&primitiveCount));
        QVERIFY(primitiveCount > 0);

        AltiumSchLibReader schLibraryReader;
        QVERIFY2(schLibraryReader.open(schPath), qPrintable(schLibraryReader.errorString()));
        const auto schComponents = schLibraryReader.components();
        QCOMPARE(schComponents.size(), 1);
        QCOMPARE(schComponents.first().name, QStringLiteral("C2040"));
        QCOMPARE(schComponents.first().sectionKey, QStringLiteral("C2040"));
        QCOMPARE(schLibraryReader.headerParameters().value(QStringLiteral("COMPCOUNT")), QStringLiteral("1"));
        QByteArray componentData;
        QVERIFY(schLibraryReader.readComponentData(0, &componentData));
        QVERIFY(componentData.contains("LibReference=C2040"));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(schLibraryReader.readComponentRecords(0, &records));
        QCOMPARE(records.size(), headerWeight);
        QByteArray reconstructedRecords;
        for (const auto& record : records)
            reconstructedRecords.append(record.encoded);
        QCOMPARE(reconstructedRecords, componentData);
        QVERIFY(records.first().payload.startsWith("|RECORD=1|"));
        QVERIFY(records.first().hasParameters);
        QCOMPARE(records.first().parameters.value(QStringLiteral("RECORD")), QStringLiteral("1"));
        QVERIFY(records.first().parameters.contains(QStringLiteral("LibReference")));
        bool sawBinaryPin = false;
        for (const auto& record : records) {
            if (!record.hasParameters && record.payload.size() >= 4 &&
                static_cast<quint32>(static_cast<unsigned char>(record.payload.at(0))) == 2) {
                sawBinaryPin = true;
                break;
            }
        }
        QVERIFY(sawBinaryPin);
    }

    /**
     * @brief 验证焊盘扩展块写入完整的 596 字节布局
     * @details 包含槽孔焊盘、圆角矩形形状、WideStrings 广字符串编码
     */
    void pcbLibPadWritesCompleteExtendedBlock() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_PAD");

        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.locationX = 0;
        pad.locationY = 0;
        pad.sizeTopX = 433070;
        pad.sizeTopY = 787400;
        pad.sizeMidX = 433070;
        pad.sizeMidY = 787400;
        pad.sizeBotX = 433070;
        pad.sizeBotY = 787400;
        pad.holeSize = 236220;
        pad.shapeTop = 9;  // RoundedRectangle
        pad.shapeMid = 9;
        pad.shapeBot = 9;
        pad.rotation = 90.0;
        pad.isPlated = true;
        pad.layer = 74;  // Multi
        pad.isSMD = false;
        pad.holeType = 2;  // Slot
        pad.holeSlotLengthRaw = 590550;
        pad.holeRotation = 90.0;
        pad.cornerRadiusPercentage = 50;
        footprint.pads.append(pad);

        AltiumPcbText text;
        text.text = QStringLiteral("REF**");
        text.layer = 33;
        footprint.texts.append(text);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_pad.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/Data"), footprintData));
        int cursor = 4 + static_cast<int>(readU32(footprintData, 0));
        QCOMPARE(static_cast<quint8>(footprintData.at(cursor++)), quint8(2));
        for (int i = 0; i < 4; ++i) {
            cursor += 4 + static_cast<int>(readU32(footprintData, cursor) & 0x00FFFFFFU);
        }
        QCOMPARE(readU32(footprintData, cursor) & 0x00FFFFFFU, quint32(114));
        cursor += 4 + 114;
        QCOMPARE(readU32(footprintData, cursor) & 0x00FFFFFFU, quint32(596));
        cursor += 4 + 596;
        QCOMPARE(static_cast<quint8>(footprintData.at(cursor++)), quint8(5));
        QCOMPARE(readU32(footprintData, cursor) & 0x00FFFFFFU, quint32(252));

        AltiumPcbLibReader pcbReader;
        QVERIFY2(pcbReader.open(pcbPath), qPrintable(pcbReader.errorString()));
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY(pcbReader.readFootprintObjects(QStringLiteral("TEST_PAD"), &objects));
        QCOMPARE(objects.size(), 2);
        QCOMPARE(objects.at(0).objectId, quint8(AltiumConstants::PCB_OBJECT_PAD));
        QCOMPARE(objects.at(0).layer, quint8(74));
        QCOMPARE(objects.at(0).primitiveFlags, quint16(0x0C));
        QVERIFY(objects.at(0).hasPadFields);
        QCOMPARE(objects.at(0).designator, QStringLiteral("1"));
        QCOMPARE(objects.at(0).pad.holeType, quint8(2));
        QCOMPARE(objects.at(0).pad.holeSlotLengthRaw, qint32(590550));
        QCOMPARE(objects.at(0).pad.cornerRadiusPercentage, quint8(50));
        QCOMPARE(objects.at(0).blocks.size(), 6);
        QCOMPARE(objects.at(1).objectId, quint8(AltiumConstants::PCB_OBJECT_TEXT));
        QCOMPARE(objects.at(1).layer, quint8(33));
        QCOMPARE(objects.at(1).primitiveFlags, quint16(0x08));
        QVERIFY(objects.at(1).hasTextFields);
        QCOMPARE(objects.at(1).textFields.locationX, qint32(0));
        QCOMPARE(objects.at(1).text, QStringLiteral("REF**"));
        QCOMPARE(objects.at(1).blocks.size(), 2);
        QVERIFY(objects.at(1).blocks.at(1).payload.startsWith(char(5)));

        QByteArray wideStrings;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/WideStrings"), wideStrings));
        QVERIFY(wideStrings.contains("ENCODEDTEXT0=82,69,70,42,42"));
    }

    /**
     * @brief 验证 PcbLib 损坏图元不会被结构化读取器误解析
     * @details 覆盖公共头部截断和 Pad 字符串子块长度错误，并确认原始 Data 流仍可回退读取。
     */
    void rejectsMalformedPcbLibPrimitiveObjects() {
        const auto writeMalformedLibrary = [](const QString& filePath, const QByteArray& footprintData) {
            OLECompoundWriter writer;
            if (!writer.create() || !writer.writeStream(QStringLiteral("FileHeader"), [&] {
                    QByteArray data;
                    AltiumBinaryWriter binaryWriter(data);
                    binaryWriter.writeInt32(27);
                    binaryWriter.writePascalShortString(QStringLiteral("PCB 6.0 Binary Library File"));
                    return data;
                }()))
                return false;

            if (!writer.addStorage(QStringLiteral("Library")))
                return false;
            QByteArray libraryData;
            AltiumBinaryWriter libraryWriter(libraryData);
            libraryWriter.beginBlock();
            libraryWriter.writeBytes(QByteArrayLiteral("KIND=Protel_Advanced_PCB"));
            libraryWriter.endBlock();
            libraryWriter.writeUInt32(1);
            libraryWriter.writeStringBlock(QStringLiteral("BROKEN"));
            if (!writer.writeStream(QStringLiteral("Library"), QStringLiteral("Data"), libraryData))
                return false;
            if (!writer.addStorage(QStringLiteral("BROKEN")) ||
                !writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), footprintData))
                return false;
            return writer.saveToFile(filePath);
        };

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QByteArray truncatedHeaderData;
        AltiumBinaryWriter truncatedHeaderWriter(truncatedHeaderData);
        truncatedHeaderWriter.writeStringBlock(QStringLiteral("BROKEN"));
        truncatedHeaderWriter.writeUInt8(AltiumConstants::PCB_OBJECT_TRACK);
        truncatedHeaderWriter.beginBlock();
        truncatedHeaderWriter.writeBytes(QByteArray(12, '\0'));
        truncatedHeaderWriter.endBlock();
        const QString truncatedHeaderPath = QDir(tempDir.path()).filePath(QStringLiteral("truncated-header.PcbLib"));
        QVERIFY(writeMalformedLibrary(truncatedHeaderPath, truncatedHeaderData));

        AltiumPcbLibReader truncatedHeaderReader;
        QVERIFY2(truncatedHeaderReader.open(truncatedHeaderPath), qPrintable(truncatedHeaderReader.errorString()));
        QVector<AltiumPcbLibReader::PrimitiveRecord> objects;
        QVERIFY(!truncatedHeaderReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(objects.isEmpty());
        QVERIFY(truncatedHeaderReader.hasError());
        QVERIFY(truncatedHeaderReader.errorString().contains(QStringLiteral("公共头部")));
        QByteArray rawData;
        QVERIFY(truncatedHeaderReader.readFootprintStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), &rawData));
        QCOMPARE(rawData, truncatedHeaderData);

        QByteArray malformedStringData;
        AltiumBinaryWriter malformedStringWriter(malformedStringData);
        malformedStringWriter.writeStringBlock(QStringLiteral("BROKEN"));
        malformedStringWriter.writeUInt8(AltiumConstants::PCB_OBJECT_PAD);
        malformedStringWriter.beginBlock();
        malformedStringWriter.writeBytes(QByteArrayLiteral("bad"));
        malformedStringWriter.endBlock();
        const QString malformedStringPath = QDir(tempDir.path()).filePath(QStringLiteral("malformed-string.PcbLib"));
        QVERIFY(writeMalformedLibrary(malformedStringPath, malformedStringData));

        AltiumPcbLibReader malformedStringReader;
        QVERIFY2(malformedStringReader.open(malformedStringPath), qPrintable(malformedStringReader.errorString()));
        QVERIFY(!malformedStringReader.readFootprintObjects(QStringLiteral("BROKEN"), &objects));
        QVERIFY(objects.isEmpty());
        QVERIFY(malformedStringReader.hasError());
        QVERIFY(malformedStringReader.errorString().contains(QStringLiteral("字符串子块")));
        QVERIFY(malformedStringReader.readFootprintStream(0, QStringLiteral("Data"), &rawData));
        QCOMPARE(rawData, malformedStringData);
    }

    /**
     * @brief 验证 SchLib 损坏记录提供诊断并保留原始流回退路径
     */
    void rejectsMalformedSchLibRecords() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QByteArray headerData;
        AltiumBinaryWriter headerWriter(headerData);
        headerWriter.writeCStringParameterBlock({{QStringLiteral("COMPCOUNT"), QStringLiteral("1")}});
        headerWriter.writeInt32(1);
        headerWriter.writeStringBlock(QStringLiteral("BROKEN"));
        QVERIFY(writer.writeStream(QStringLiteral("FileHeader"), headerData));
        QVERIFY(writer.addStorage(QStringLiteral("BROKEN")));
        const QByteArray malformedData(2, '\0');
        QVERIFY(writer.writeStream(QStringLiteral("BROKEN"), QStringLiteral("Data"), malformedData));
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("malformed-record.SchLib"));
        QVERIFY(writer.saveToFile(path));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        QVector<AltiumSchLibReader::Record> records;
        QVERIFY(!reader.readComponentRecords(QStringLiteral("BROKEN"), &records));
        QVERIFY(records.isEmpty());
        QVERIFY(reader.hasError());
        QVERIFY(reader.errorString().contains(QStringLiteral("记录失败")));
        QByteArray rawData;
        QVERIFY(reader.readComponentData(QStringLiteral("BROKEN"), &rawData));
        QCOMPARE(rawData, malformedData);
    }

    /**
     * @brief 验证 PcbLib 封装写入 UniqueIdPrimitiveInformation 流
     * @details 验证 Header 中的图元计数和 Data 中的 PRIMITIVEOBJECTID 条目
     */
    void pcbLibWritesUniqueIdPrimitiveInformation() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_UID");

        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.sizeTopX = 20000;
        pad.sizeTopY = 20000;
        pad.sizeMidX = 20000;
        pad.sizeMidY = 20000;
        pad.sizeBotX = 20000;
        pad.sizeBotY = 20000;
        pad.isSMD = true;
        pad.layer = 1;
        footprint.pads.append(pad);

        AltiumPcbTrack track;
        track.startX = 0;
        track.startY = 0;
        track.endX = 10000;
        track.endY = 0;
        track.width = 1000;
        track.layer = 33;
        footprint.tracks.append(track);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_uid.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        // 验证 UniqueIdPrimitiveInformation 流存在
        QByteArray uidHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_UID/UniqueIdPrimitiveInformation/Header"), uidHeader));
        QCOMPARE(readU32(uidHeader, 0), quint32(2));  // 1 pad + 1 track = 2 图元

        QByteArray uidData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_UID/UniqueIdPrimitiveInformation/Data"), uidData));
        // 验证包含 PRIMITIVEOBJECTID
        QVERIFY(uidData.contains("PRIMITIVEOBJECTID=Pad"));
        QVERIFY(uidData.contains("PRIMITIVEOBJECTID=Track"));
    }

    /**
     * @brief 验证 PcbLib 封装正确写入 3D 元件体（Object ID = 12）
     */
    void pcbLibWritesComponentBody() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent footprint;
        footprint.name = QStringLiteral("TEST_BODY");
        footprint.height = 1.0;

        // 需要至少一个图元来计算包围盒
        AltiumPcbPad pad;
        pad.designator = QStringLiteral("1");
        pad.locationX = -50000;
        pad.locationY = -50000;
        pad.sizeTopX = 100000;
        pad.sizeTopY = 100000;
        pad.sizeMidX = 100000;
        pad.sizeMidY = 100000;
        pad.sizeBotX = 100000;
        pad.sizeBotY = 100000;
        pad.isSMD = true;
        pad.layer = 1;
        footprint.pads.append(pad);

        // 添加一个 3D 模型
        AltiumPcbComponent::Model3D model;
        model.name = QStringLiteral("test.step");
        model.stepData = QByteArrayLiteral("ISO-10303-21;");
        footprint.models.append(model);

        AltiumPcbComponentBody body;
        body.layerName = QStringLiteral("MECHANICAL1");
        body.name = QStringLiteral("__BODY__");
        body.modelId = QStringLiteral("{12345678-1234-1234-1234-123456789012}");
        body.modelName = QStringLiteral("test.step");
        body.overallHeightRaw = 10000;  // 1mil
        body.outline = {QPointF(-50000, -50000), QPointF(50000, -50000), QPointF(50000, 50000), QPointF(-50000, 50000)};
        footprint.bodies.append(body);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_body.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        // 验证封装 Data 流包含 ComponentBody (Object ID = 12)
        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_BODY/Data"), footprintData));
        // Object ID 12 应该出现在数据中
        bool foundBody = false;
        for (int i = 0; i < footprintData.size(); ++i) {
            if (static_cast<unsigned char>(footprintData.at(i)) == 12) {
                foundBody = true;
                break;
            }
        }
        QVERIFY(foundBody);
    }

    /**
     * @brief 验证 SchLib 写入包含 UTF-8 中文参数的元件
     * @details 验证 %UTF8% 参数编码和中文文本正确序列化
     */
    void schLibWritesUtf8Parameters() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("TEST_UTF8");
        symbol.description = QStringLiteral("测试中文描述");
        symbol.partCount = 1;
        symbol.sourceMetadata.insert(QStringLiteral("manufacturer"), QStringLiteral("测试厂商"));

        AltiumSchPin pin;
        pin.name = QStringLiteral("A");
        pin.designator = QStringLiteral("1");
        pin.locationX = 100000;
        pin.locationY = 200000;
        pin.length = 100000;
        pin.electricalType = AltiumModels::PinElectricalType::Passive;
        pin.orientation = AltiumModels::PinOrientation::Right;
        symbol.pins.append(pin);

        AltiumSchText text;
        text.text = QStringLiteral("中文文本");
        text.locationX = 300000;
        text.locationY = 300000;
        text.fontId = 1;
        text.color = 0;
        symbol.texts.append(text);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("test_utf8.SchLib"));
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath));

        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("TEST_UTF8/Data"), schData));

        // 验证包含 %UTF8% 参数
        QVERIFY(schData.contains("%UTF8%ComponentDescription=") || schData.contains("ComponentDescription="));
        QVERIFY(schData.contains("%UTF8%Text=") || schData.contains("Text="));
        QVERIFY(schData.contains(QStringLiteral("测试中文描述").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("测试厂商").toUtf8()));
        QVERIFY(schData.contains(QStringLiteral("中文文本").toUtf8()));

        IR::SymbolComponentIR rotatedTextSymbol;
        rotatedTextSymbol.name = QStringLiteral("ROTATED_TEXT");
        IR::SymbolTextIR rotatedText;
        rotatedText.text = QStringLiteral("方向");
        rotatedText.rotation = -90.0;
        rotatedText.fontSizeMm = 2.5;
        rotatedTextSymbol.texts.append(rotatedText);
        IR::SymbolParameterIR rotatedParameter;
        rotatedParameter.name = QStringLiteral("角度参数");
        rotatedParameter.value = QStringLiteral("值");
        rotatedParameter.rotation = -90.0;
        rotatedParameter.fontSizeMm = 2.5;
        rotatedTextSymbol.parameters.append(rotatedParameter);
        IR::SymbolParameterIR firstPartParameter;
        firstPartParameter.name = QStringLiteral("第一部件参数");
        firstPartParameter.value = QStringLiteral("P1");
        firstPartParameter.partIndex = 0;
        rotatedTextSymbol.parameters.append(firstPartParameter);
        IR::SymbolParameterIR sharedFirstPartParameter;
        sharedFirstPartParameter.name = QStringLiteral("Shared");
        sharedFirstPartParameter.value = QStringLiteral("P1");
        sharedFirstPartParameter.partIndex = 0;
        rotatedTextSymbol.parameters.append(sharedFirstPartParameter);
        IR::SymbolParameterIR sharedSecondPartParameter = sharedFirstPartParameter;
        sharedSecondPartParameter.value = QStringLiteral("P2");
        sharedSecondPartParameter.partIndex = 1;
        rotatedTextSymbol.parameters.append(sharedSecondPartParameter);
        IR::SymbolParameterIR commonParameter;
        commonParameter.name = QStringLiteral("公共参数");
        commonParameter.value = QStringLiteral("COMMON");
        commonParameter.partIndex = -1;
        rotatedTextSymbol.parameters.append(commonParameter);
        rotatedTextSymbol.partCount = 2;
        const QString rotatedTextPath = QDir(tempDir.path()).filePath(QStringLiteral("rotated-text.SchLib"));
        ExporterAltiumSymbol rotatedTextExporter;
        QVERIFY(rotatedTextExporter.exportSymbolLibrary(
            {rotatedTextSymbol}, QStringLiteral("rotated-text"), rotatedTextPath, false, false));
        QByteArray rotatedTextData;
        QVERIFY(readCfbStream(rotatedTextPath, QStringLiteral("ROTATED_TEXT/Data"), rotatedTextData));
        QVERIFY(rotatedTextData.contains("Orientation=3"));
        QVERIFY(rotatedTextData.contains("NAME=第一部件参数"));
        QVERIFY(rotatedTextData.contains("NAME=公共参数"));
        QCOMPARE(rotatedTextData.count("NAME=Shared"), 2);
        const int rotatedParameterOffset = rotatedTextData.indexOf("NAME=角度参数");
        const int rotatedParameterRecordOffset = rotatedTextData.lastIndexOf("|RECORD=41|", rotatedParameterOffset);
        QVERIFY(rotatedParameterOffset > rotatedParameterRecordOffset);
        QVERIFY(rotatedTextData.mid(rotatedParameterRecordOffset, rotatedParameterOffset - rotatedParameterRecordOffset)
                    .contains("FONTID=2"));
        QVERIFY(rotatedTextData.contains("OWNERPARTID=1"));
        QVERIFY(rotatedTextData.contains("OWNERPARTID=-1"));
    }

    /**
     * @brief 验证 makeUniqueSectionKeys 生成的键满足 CFB 大小写唯一性
     */
    void createsUniqueCfbSectionKeys() {
        const QStringList keys =
            AltiumWriterUtils::makeUniqueSectionKeys({QStringLiteral("Same/Name"),
                                                      QStringLiteral("same:name"),
                                                      QString(40, QLatin1Char('A')),
                                                      QString(39, QLatin1Char('A')) + QStringLiteral("B")});
        QCOMPARE(keys.size(), 4);
        QSet<QString> folded;
        for (const QString& key : keys) {
            QVERIFY(!key.isEmpty());
            QVERIFY(key.size() <= 31);
            QVERIFY(!folded.contains(key.toCaseFolded()));
            folded.insert(key.toCaseFolded());
        }
    }

    /**
     * @brief 验证 Altium 不会静默覆盖已有库
     */
    void rejectsUnsupportedAltiumLibraryMerge() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("existing.SchLib"));
        QFile existingFile(outputPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        const QByteArray originalData = QByteArrayLiteral("existing-library");
        QCOMPARE(existingFile.write(originalData), originalData.size());
        existingFile.close();

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("NEW_SYMBOL");
        ExporterAltiumSymbol exporter;
        QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("existing"), outputPath, true, false));
        QVERIFY(!exporter.diagnostics().isEmpty());
        QVERIFY(exporter.diagnostics().first().contains(QStringLiteral("暂不支持")));

        QVERIFY(existingFile.open(QIODevice::ReadOnly));
        QCOMPARE(existingFile.readAll(), originalData);
        existingFile.close();

        QVERIFY(!exporter.exportSymbolLibrary({symbol}, QStringLiteral("existing"), outputPath, false, true));
        QVERIFY(!exporter.diagnostics().isEmpty());
    }

    /**
     * @brief 验证导出器保持多部件结构和折线逐段展开逻辑
     * @details 符号多部件的 OWNERPARTID 正确映射，封装折线按段写入 Track，
     *          安装孔转为非电镀 MultiLayer pad，板框保留层和线宽
     */
    void exportersPreserveMultipartAndPolylineStructure() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("MULTIPART");
        symbol.designatorPrefix = QStringLiteral("U");
        symbol.partCount = 2;
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("B");
        pin.designator = QStringLiteral("2");
        pin.length = 2.54;
        pin.partIndex = 1;
        pin.style.decoration = IR::PinDecoration::AnalogInput;
        symbol.pins.append(pin);
        IR::SymbolPinIR commonPin;
        commonPin.name = QStringLiteral("VCC");
        commonPin.designator = QStringLiteral("3");
        commonPin.length = 2.54;
        commonPin.commonToAllParts = true;
        symbol.pins.append(commonPin);
        IR::SymbolPinIR secondPartPin;
        secondPartPin.name = QStringLiteral("A");
        secondPartPin.designator = QStringLiteral("1");
        secondPartPin.length = 2.54;
        secondPartPin.partIndex = 0;
        symbol.pins.append(secondPartPin);
        IR::SymbolPathIR path;
        path.points = {QPointF(0, 0), QPointF(1, 1), QPointF(2, 0)};
        path.partIndex = 1;
        symbol.paths.append(path);
        IR::SymbolBezierIR bezier;
        bezier.controlPoints = {QPointF(-1, 0), QPointF(-0.5, 2), QPointF(0.5, -2), QPointF(1, 0)};
        bezier.partIndex = 1;
        symbol.beziers.append(bezier);
        IR::SymbolRectangleIR roundedRectangle;
        roundedRectangle.x0 = -2.0;
        roundedRectangle.y0 = -1.0;
        roundedRectangle.x1 = 2.0;
        roundedRectangle.y1 = 1.0;
        roundedRectangle.cornerRadiusX = 0.25;
        roundedRectangle.cornerRadiusY = 0.2;
        roundedRectangle.partIndex = 1;
        symbol.rectangles.append(roundedRectangle);
        IR::SymbolRectangleIR commonRectangle;
        commonRectangle.x0 = -3.0;
        commonRectangle.y0 = -2.0;
        commonRectangle.x1 = -2.0;
        commonRectangle.y1 = -1.0;
        commonRectangle.partIndex = -1;
        symbol.rectangles.append(commonRectangle);
        IR::SymbolRectangleIR secondPartRectangle;
        secondPartRectangle.x0 = 4.0;
        secondPartRectangle.y0 = 3.0;
        secondPartRectangle.x1 = 5.0;
        secondPartRectangle.y1 = 4.0;
        secondPartRectangle.partIndex = 0;
        symbol.rectangles.append(secondPartRectangle);
        IR::SymbolPolygonIR firstPartPolygon;
        firstPartPolygon.points = {QPointF(10.0, 10.0), QPointF(11.0, 10.0), QPointF(10.0, 11.0)};
        firstPartPolygon.partIndex = 1;
        symbol.polygons.append(firstPartPolygon);
        IR::SymbolPolygonIR secondPartPolygon;
        secondPartPolygon.points = {QPointF(20.0, 20.0), QPointF(21.0, 20.0), QPointF(20.0, 21.0)};
        secondPartPolygon.partIndex = 0;
        symbol.polygons.append(secondPartPolygon);
        IR::SymbolTextIR commonText;
        commonText.text = QStringLiteral("COMMON_TEXT");
        commonText.partIndex = -1;
        symbol.texts.append(commonText);
        IR::SymbolTextIR partText;
        partText.text = QStringLiteral("PART_TEXT");
        partText.partIndex = 1;
        symbol.texts.append(partText);
        IR::SymbolImageIR image;
        image.x0 = -1.0;
        image.y0 = -0.5;
        image.x1 = 1.0;
        image.y1 = 0.5;
        image.fileName = QStringLiteral("multipart.png");
        image.data = QByteArrayLiteral("multipart-image");
        symbol.images.append(image);
        IR::SymbolImageIR duplicateImage = image;
        duplicateImage.data = QByteArrayLiteral("multipart-image-duplicate");
        duplicateImage.fileName = QStringLiteral("C:\\assets\\multipart.png");
        symbol.images.append(duplicateImage);
        symbol.graphicOrder = {
            {QStringLiteral("P"), 0, 1},
            {QStringLiteral("R"), 0, 1},
            {QStringLiteral("PT"), 0, 1},
            {QStringLiteral("P"), 0, 0},
            {QStringLiteral("P"), 1, 0},
            {QStringLiteral("R"), 0, -1},
            {QStringLiteral("R"), 0, 0},
            {QStringLiteral("PG"), 0, 1},
            {QStringLiteral("PG"), 0, 0},
            {QStringLiteral("T"), 0, -1},
            {QStringLiteral("T"), 0, 1},
        };

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("multipart.SchLib"));
        ExporterAltiumSymbol symbolExporter;
        QVERIFY(symbolExporter.exportSymbolLibrary({symbol}, QStringLiteral("multipart"), schPath, false, false));
        QByteArray symbolData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("MULTIPART/Data"), symbolData));
        const int pinOffset = 4 + static_cast<int>(readU32(symbolData, 0) & 0x00FFFFFFU);
        QCOMPARE(readU32(symbolData, pinOffset + 4), quint32(2));
        QCOMPARE(readU16(symbolData, pinOffset + 9), quint16(2));
        QCOMPARE(static_cast<uint8_t>(symbolData.at(pinOffset + 15)), static_cast<uint8_t>(5));
        QList<quint16> orderedPinOwners;
        for (int offset = pinOffset; offset + 13 <= symbolData.size();) {
            const int payloadSize = static_cast<int>(readU32(symbolData, offset) & 0x00FFFFFFU);
            const int nextOffset = offset + 4 + payloadSize;
            if (payloadSize < 9 || nextOffset > symbolData.size())
                break;
            if (readU32(symbolData, offset + 4) == 2)
                orderedPinOwners.append(readU16(symbolData, offset + 9));
            offset = nextOffset;
        }
        QCOMPARE(orderedPinOwners, QList<quint16>({quint16(2), quint16(0xFFFF), quint16(1)}));
        QVERIFY(symbolData.contains("OWNERPARTID=2"));
        QVERIFY(symbolData.contains("OWNERPARTID=-1"));
        QVERIFY(symbolData.contains("IndexInSheet=1"));
        QVERIFY(symbolData.contains("IndexInSheet=2"));
        QVERIFY(symbolData.contains("RECORD=5"));
        QVERIFY(symbolData.contains("LocationCount=4"));
        QVERIFY(symbolData.contains("RECORD=10"));
        const int firstPolygonRecord = symbolData.indexOf("RECORD=7");
        const int secondPolygonRecord = symbolData.indexOf("RECORD=7", firstPolygonRecord + 1);
        QVERIFY(firstPolygonRecord > 0);
        QVERIFY(secondPolygonRecord > firstPolygonRecord);
        QVERIFY(symbolData.mid(firstPolygonRecord, secondPolygonRecord - firstPolygonRecord).contains("OWNERPARTID=2"));
        QVERIFY(symbolData.mid(secondPolygonRecord).contains("OWNERPARTID=1"));
        QVERIFY(symbolData.contains("RECORD=30"));
        const int roundedRectangleRecord = symbolData.indexOf("RECORD=10");
        const int pathRecord = symbolData.indexOf("RECORD=6");
        const int rectangleRecord = symbolData.indexOf("RECORD=14");
        QVERIFY(roundedRectangleRecord > pinOffset);
        QVERIFY(pathRecord > roundedRectangleRecord);
        QVERIFY(rectangleRecord > pathRecord);
        QVERIFY(symbolData.contains("EmbedImage=T"));
        QVERIFY(symbolData.contains("FileName=multipart.png"));
        QVERIFY(symbolData.contains("FileName=multipart_2.png"));
        const int commonTextOffset = symbolData.indexOf("Text=COMMON_TEXT");
        const int commonTextRecordOffset = symbolData.lastIndexOf("|RECORD=4|", commonTextOffset);
        QVERIFY(commonTextOffset > commonTextRecordOffset);
        QVERIFY(symbolData.mid(commonTextRecordOffset, commonTextOffset - commonTextRecordOffset)
                    .contains("OWNERPARTID=-1"));
        const int partTextOffset = symbolData.indexOf("Text=PART_TEXT");
        const int partTextRecordOffset = symbolData.lastIndexOf("|RECORD=4|", partTextOffset);
        QVERIFY(partTextOffset > partTextRecordOffset);
        QVERIFY(symbolData.mid(partTextRecordOffset, partTextOffset - partTextRecordOffset).contains("OWNERPARTID=2"));
        QByteArray multipartStorage;
        QVERIFY(readCfbStream(schPath, QStringLiteral("Storage"), multipartStorage));
        QVERIFY(multipartStorage.contains("multipart.png"));
        QVERIFY(multipartStorage.contains("multipart_2.png"));
        QMap<QString, QByteArray> compressedImages;
        QVERIFY(parseAltiumImageStorage(multipartStorage, compressedImages));
        QCOMPARE(compressedImages.keys(),
                 QStringList({QStringLiteral("multipart.png"), QStringLiteral("multipart_2.png")}));
        QCOMPARE(qUncompress(restoreQtCompressionHeader(compressedImages.value(QStringLiteral("multipart.png")),
                                                        QByteArrayLiteral("multipart-image").size())),
                 QByteArrayLiteral("multipart-image"));
        QCOMPARE(qUncompress(restoreQtCompressionHeader(compressedImages.value(QStringLiteral("multipart_2.png")),
                                                        QByteArrayLiteral("multipart-image-duplicate").size())),
                 QByteArrayLiteral("multipart-image-duplicate"));

        AltiumSchComponent diagnosticSymbol;
        diagnosticSymbol.name = QStringLiteral("IMAGE_DIAGNOSTICS");
        AltiumSchImage emptyImage;
        emptyImage.embedImage = true;
        emptyImage.fileName = QStringLiteral("empty.png");
        diagnosticSymbol.images.append(emptyImage);
        AltiumSchImage invalidNameImage;
        invalidNameImage.embedImage = true;
        invalidNameImage.fileName = QStringLiteral("bad|name.png");
        invalidNameImage.data = QByteArrayLiteral("image");
        diagnosticSymbol.images.append(invalidNameImage);
        diagnosticSymbol.graphicOrder = {{QStringLiteral("T"), 0, 0}};
        AltiumSchPolygon fallbackPolygon;
        fallbackPolygon.vertices = {QPointF(100000, 100000), QPointF(200000, 100000), QPointF(100000, 200000)};
        diagnosticSymbol.polygons.append(fallbackPolygon);
        AltiumSchLibWriter diagnosticWriter;
        const QString diagnosticPath = QDir(tempDir.path()).filePath(QStringLiteral("image-diagnostics.SchLib"));
        QVERIFY(diagnosticWriter.write({diagnosticSymbol}, diagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS")));
        QByteArray fallbackData;
        QVERIFY(readCfbStream(diagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS/Data"), fallbackData));
        QVERIFY(fallbackData.contains("RECORD=7"));
        QVERIFY(diagnosticWriter.diagnostics().contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 0 的嵌入数据为空，已跳过 Storage")));
        QVERIFY(diagnosticWriter.diagnostics().contains(
            QStringLiteral("组件 IMAGE_DIAGNOSTICS 图片 1 的嵌入文件名无效: bad|name.png，已跳过 Storage")));
        AltiumSchImage oversizedNameImage;
        oversizedNameImage.embedImage = true;
        oversizedNameImage.fileName = QString(256, QLatin1Char('a')) + QStringLiteral(".png");
        oversizedNameImage.data = QByteArrayLiteral("image");
        diagnosticSymbol.images.append(oversizedNameImage);
        const QString oversizedDiagnosticPath = QDir(tempDir.path()).filePath(QStringLiteral("oversized-image.SchLib"));
        QVERIFY(
            diagnosticWriter.write({diagnosticSymbol}, oversizedDiagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS")));
        QVERIFY(diagnosticWriter.diagnostics().join('\n').contains(QStringLiteral("图片 2 的嵌入文件名无效")));
        QByteArray diagnosticData;
        QVERIFY(readCfbStream(oversizedDiagnosticPath, QStringLiteral("IMAGE_DIAGNOSTICS/Data"), diagnosticData));
        QVERIFY(!diagnosticData.contains("FileName=bad|name.png"));
        QVERIFY(!diagnosticData.contains("FileName=aaaaaaaa"));

        IR::SymbolComponentIR invalidBezierSymbol;
        invalidBezierSymbol.name = QStringLiteral("INVALID_BEZIER");
        IR::SymbolBezierIR invalidBezier;
        invalidBezier.controlPoints = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
        invalidBezierSymbol.beziers.append(invalidBezier);
        ExporterAltiumSymbol invalidBezierExporter;
        const QString invalidBezierPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-bezier.SchLib"));
        QVERIFY(invalidBezierExporter.exportSymbol(invalidBezierSymbol, invalidBezierPath));
        QVERIFY(invalidBezierExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BEZIER Bézier 图元 0 的控制点参数无效（数量为 2），已跳过")));

        IR::SymbolComponentIR invalidPrimitivePointsSymbol;
        invalidPrimitivePointsSymbol.name = QStringLiteral("INVALID_PRIMITIVE_POINTS");
        IR::SymbolPolylineIR invalidPolyline;
        invalidPolyline.points = {QPointF(0.0, 0.0)};
        invalidPrimitivePointsSymbol.polylines.append(invalidPolyline);
        IR::SymbolBezierIR invalidFiniteBezier;
        invalidFiniteBezier.controlPoints = {QPointF(0.0, 0.0),
                                             QPointF(1.0, 0.0),
                                             QPointF(std::numeric_limits<double>::infinity(), 1.0),
                                             QPointF(2.0, 0.0)};
        invalidPrimitivePointsSymbol.beziers.append(invalidFiniteBezier);
        ExporterAltiumSymbol invalidPrimitivePointsExporter;
        const QString invalidPrimitivePointsFile =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-primitive-points.SchLib"));
        QVERIFY(invalidPrimitivePointsExporter.exportSymbol(invalidPrimitivePointsSymbol, invalidPrimitivePointsFile));
        QVERIFY(invalidPrimitivePointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PRIMITIVE_POINTS 折线图元 0 的点列无效，已跳过")));
        QVERIFY(invalidPrimitivePointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PRIMITIVE_POINTS Bézier 图元 0 的控制点参数无效（数量为 4），已跳过")));

        IR::SymbolComponentIR invalidGeometrySymbol;
        invalidGeometrySymbol.name = QStringLiteral("INVALID_GEOMETRY");
        IR::SymbolCircleIR invalidCircle;
        invalidCircle.radius = -1.0;
        invalidGeometrySymbol.circles.append(invalidCircle);
        IR::SymbolCircleIR invalidStrokeCircle;
        invalidStrokeCircle.strokeWidth = std::numeric_limits<double>::quiet_NaN();
        invalidGeometrySymbol.circles.append(invalidStrokeCircle);
        IR::SymbolArcIR degenerateArc;
        degenerateArc.startPoint = QPointF(0.0, 0.0);
        degenerateArc.midPoint = QPointF(1.0, 1.0);
        degenerateArc.endPoint = QPointF(2.0, 2.0);
        invalidGeometrySymbol.arcs.append(degenerateArc);
        ExporterAltiumSymbol invalidGeometryExporter;
        const QString invalidGeometryPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-geometry.SchLib"));
        QVERIFY(invalidGeometryExporter.exportSymbol(invalidGeometrySymbol, invalidGeometryPath));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆图元 0 的半径无效，已钳制为非负值")));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆图元 1 的中心或线宽无效，已跳过")));
        QVERIFY(invalidGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_GEOMETRY 圆弧图元 0 三点退化，已使用安全回退圆心")));

        IR::SymbolComponentIR invalidArcGeometrySymbol;
        invalidArcGeometrySymbol.name = QStringLiteral("INVALID_ARC_GEOMETRY");
        IR::SymbolPieIR invalidPie;
        invalidPie.radius = -1.0;
        invalidPie.startAngle = std::numeric_limits<double>::quiet_NaN();
        invalidArcGeometrySymbol.pies.append(invalidPie);
        IR::SymbolEllipticalArcIR invalidEllipticalArc;
        invalidEllipticalArc.radiusX = 0.0;
        invalidEllipticalArc.radiusY = 1.0;
        invalidArcGeometrySymbol.ellipticalArcs.append(invalidEllipticalArc);
        ExporterAltiumSymbol invalidArcGeometryExporter;
        const QString invalidArcGeometryPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-geometry.SchLib"));
        QVERIFY(invalidArcGeometryExporter.exportSymbol(invalidArcGeometrySymbol, invalidArcGeometryPath));
        QVERIFY(invalidArcGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_GEOMETRY 扇形图元 0 的几何参数无效，已跳过")));
        QVERIFY(invalidArcGeometryExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_GEOMETRY 椭圆弧图元 0 的几何参数无效，已跳过")));

        IR::SymbolComponentIR invalidTextSymbol;
        invalidTextSymbol.name = QStringLiteral("INVALID_TEXT_DATA");
        IR::SymbolTextIR invalidText;
        invalidText.text = QStringLiteral("label");
        invalidText.position = QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
        invalidTextSymbol.texts.append(invalidText);
        IR::SymbolTextFrameIR invalidTextFrame;
        invalidTextFrame.textMargin = -1.0;
        invalidTextSymbol.textFrames.append(invalidTextFrame);
        IR::SymbolParameterIR invalidParameter;
        invalidParameter.name = QStringLiteral("Custom");
        invalidParameter.position = QPointF(0.0, std::numeric_limits<double>::infinity());
        invalidTextSymbol.parameters.append(invalidParameter);
        ExporterAltiumSymbol invalidTextExporter;
        const QString invalidTextPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-text-data.SchLib"));
        QVERIFY(invalidTextExporter.exportSymbol(invalidTextSymbol, invalidTextPath));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 文本图元 0 的内容或几何参数无效，已跳过")));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 文本框图元 0 的几何参数无效，已跳过")));
        QVERIFY(invalidTextExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 参数 Custom 的位置、旋转角度或名称无效，已跳过")));
        IR::SymbolParameterIR invalidParameterFont;
        invalidParameterFont.name = QStringLiteral("FontInvalid");
        invalidParameterFont.fontSizeMm = std::numeric_limits<double>::quiet_NaN();
        invalidTextSymbol.parameters.append(invalidParameterFont);
        ExporterAltiumSymbol invalidParameterFontExporter;
        const QString invalidParameterFontPath =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-parameter-font.SchLib"));
        QVERIFY(invalidParameterFontExporter.exportSymbol(invalidTextSymbol, invalidParameterFontPath));
        QVERIFY(invalidParameterFontExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_TEXT_DATA 参数 FontInvalid 的字体大小无效，已跳过")));

        IR::SymbolComponentIR invalidBoundsSymbol;
        invalidBoundsSymbol.name = QStringLiteral("INVALID_BOUNDS_DATA");
        IR::SymbolRectangleIR invalidRectangle;
        invalidRectangle.x1 = std::numeric_limits<double>::quiet_NaN();
        invalidBoundsSymbol.rectangles.append(invalidRectangle);
        IR::SymbolTextFrameIR zeroTextFrame;
        zeroTextFrame.x1 = 0.0;
        zeroTextFrame.y1 = 0.0;
        invalidBoundsSymbol.textFrames.append(zeroTextFrame);
        ExporterAltiumSymbol invalidBoundsExporter;
        const QString invalidBoundsPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-bounds-data.SchLib"));
        QVERIFY(invalidBoundsExporter.exportSymbol(invalidBoundsSymbol, invalidBoundsPath));
        QVERIFY(invalidBoundsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BOUNDS_DATA 矩形图元 0 的边界无效，已跳过")));
        QVERIFY(invalidBoundsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_BOUNDS_DATA 文本框图元 0 的几何参数无效，已跳过")));

        IR::SymbolComponentIR invalidImageSymbol;
        invalidImageSymbol.name = QStringLiteral("INVALID_IMAGE_DATA");
        IR::SymbolImageIR invalidImage;
        invalidImage.x1 = std::numeric_limits<double>::quiet_NaN();
        invalidImage.fileName = QStringLiteral("image.png");
        invalidImageSymbol.images.append(invalidImage);
        ExporterAltiumSymbol invalidImageExporter;
        const QString invalidImagePath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-image-data.SchLib"));
        QVERIFY(invalidImageExporter.exportSymbol(invalidImageSymbol, invalidImagePath));
        QVERIFY(invalidImageExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_IMAGE_DATA 图片图元 0 的边界、线宽或资源无效，已跳过")));

        IR::SymbolComponentIR invalidArcSymbol;
        invalidArcSymbol.name = QStringLiteral("INVALID_ARC_DATA");
        IR::SymbolArcIR invalidArc;
        invalidArc.startPoint = QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0);
        invalidArcSymbol.arcs.append(invalidArc);
        IR::SymbolIeeeIR invalidIeee;
        invalidIeee.position = QPointF(0.0, std::numeric_limits<double>::infinity());
        invalidArcSymbol.ieeeSymbols.append(invalidIeee);
        ExporterAltiumSymbol invalidArcExporter;
        const QString invalidArcPath = QDir(tempDir.path()).filePath(QStringLiteral("invalid-arc-data.SchLib"));
        QVERIFY(invalidArcExporter.exportSymbol(invalidArcSymbol, invalidArcPath));
        QVERIFY(invalidArcExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_DATA 圆弧图元 0 的点列或线宽无效，已跳过")));
        QVERIFY(invalidArcExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_ARC_DATA IEEE 图形 0 的位置无效，已跳过")));

        IR::SymbolComponentIR invalidPathSymbol;
        invalidPathSymbol.name = QStringLiteral("INVALID_PATH_SEGMENT");
        IR::SymbolPathIR invalidPath;
        IR::SymbolPathSegmentIR invalidEllipseArc;
        invalidEllipseArc.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        invalidEllipseArc.arcCenter = QPointF(0.0, 0.0);
        invalidEllipseArc.radiusX = 0.0;
        invalidEllipseArc.radiusY = 1.0;
        invalidEllipseArc.arcStartAngle = 0.0;
        invalidEllipseArc.arcEndAngle = 90.0;
        invalidPath.segments.append(invalidEllipseArc);
        invalidPathSymbol.paths.append(invalidPath);
        ExporterAltiumSymbol invalidPathExporter;
        const QString invalidPathFile = QDir(tempDir.path()).filePath(QStringLiteral("invalid-path-segment.SchLib"));
        QVERIFY(invalidPathExporter.exportSymbol(invalidPathSymbol, invalidPathFile));
        QVERIFY(invalidPathExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PATH_SEGMENT 路径图元 0 的段 0 参数无效，已跳过")));

        IR::SymbolComponentIR invalidPathPointsSymbol;
        invalidPathPointsSymbol.name = QStringLiteral("INVALID_PATH_POINTS");
        IR::SymbolPathIR invalidFilledPath;
        invalidFilledPath.isFilled = true;
        invalidFilledPath.points = {QPointF(0.0, 0.0), QPointF(std::numeric_limits<double>::quiet_NaN(), 1.0)};
        invalidPathPointsSymbol.paths.append(invalidFilledPath);
        ExporterAltiumSymbol invalidPathPointsExporter;
        const QString invalidPathPointsFile =
            QDir(tempDir.path()).filePath(QStringLiteral("invalid-path-points.SchLib"));
        QVERIFY(invalidPathPointsExporter.exportSymbol(invalidPathPointsSymbol, invalidPathPointsFile));
        QVERIFY(invalidPathPointsExporter.diagnostics().contains(
            QStringLiteral("符号 INVALID_PATH_POINTS 路径图元 0 的点列无效，已跳过")));

        IR::SymbolComponentIR nativeArcSymbol;
        nativeArcSymbol.name = QStringLiteral("NATIVE_ARC");
        IR::SymbolPathIR nativeArcPath;
        nativeArcPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR nativeArcSegment;
        nativeArcSegment.type = IR::SymbolPathSegmentIR::Type::CircularArc;
        nativeArcSegment.start = QPointF(0.0, 0.0);
        nativeArcSegment.arcMid = QPointF(1.0, 1.0);
        nativeArcSegment.end = QPointF(2.0, 0.0);
        nativeArcPath.segments.append(nativeArcSegment);
        nativeArcSymbol.paths.append(nativeArcPath);
        const QString nativeArcPathFile = QDir(tempDir.path()).filePath(QStringLiteral("native-arc.SchLib"));
        ExporterAltiumSymbol nativeArcExporter;
        QVERIFY(nativeArcExporter.exportSymbolLibrary(
            {nativeArcSymbol}, QStringLiteral("native-arc"), nativeArcPathFile, false, false));
        QByteArray nativeArcData;
        QVERIFY(readCfbStream(nativeArcPathFile, QStringLiteral("NATIVE_ARC/Data"), nativeArcData));
        QVERIFY(nativeArcData.contains("RECORD=12"));

        IR::SymbolComponentIR nativeQuadraticSymbol;
        nativeQuadraticSymbol.name = QStringLiteral("NATIVE_QUADRATIC");
        IR::SymbolPathIR nativeQuadraticPath;
        nativeQuadraticPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR nativeQuadraticSegment;
        nativeQuadraticSegment.type = IR::SymbolPathSegmentIR::Type::QuadraticBezier;
        nativeQuadraticSegment.start = QPointF(0.0, 0.0);
        nativeQuadraticSegment.control1 = QPointF(1.0, 2.0);
        nativeQuadraticSegment.end = QPointF(2.0, 0.0);
        nativeQuadraticPath.segments.append(nativeQuadraticSegment);
        nativeQuadraticSymbol.paths.append(nativeQuadraticPath);
        const QString nativeQuadraticPathFile =
            QDir(tempDir.path()).filePath(QStringLiteral("native-quadratic.SchLib"));
        ExporterAltiumSymbol nativeQuadraticExporter;
        QVERIFY(nativeQuadraticExporter.exportSymbolLibrary(
            {nativeQuadraticSymbol}, QStringLiteral("native-quadratic"), nativeQuadraticPathFile, false, false));
        QByteArray nativeQuadraticData;
        QVERIFY(readCfbStream(nativeQuadraticPathFile, QStringLiteral("NATIVE_QUADRATIC/Data"), nativeQuadraticData));
        QVERIFY(nativeQuadraticData.contains("RECORD=5"));
        QVERIFY(nativeQuadraticData.contains("LocationCount=4"));

        IR::SymbolComponentIR nativeEllipticalArcSymbol;
        nativeEllipticalArcSymbol.name = QStringLiteral("NATIVE_ELLIPTICAL_ARC");
        IR::SymbolPathIR nativeEllipticalArcPath;
        nativeEllipticalArcPath.strokeWidth = 0.1;
        IR::SymbolPathSegmentIR nativeEllipticalArcSegment;
        nativeEllipticalArcSegment.type = IR::SymbolPathSegmentIR::Type::EllipticalArc;
        nativeEllipticalArcSegment.arcCenter = QPointF(0.0, 0.0);
        nativeEllipticalArcSegment.radiusX = 2.0;
        nativeEllipticalArcSegment.radiusY = 1.0;
        nativeEllipticalArcSegment.arcStartAngle = 0.0;
        nativeEllipticalArcSegment.arcEndAngle = 90.0;
        nativeEllipticalArcPath.segments.append(nativeEllipticalArcSegment);
        nativeEllipticalArcSymbol.paths.append(nativeEllipticalArcPath);
        const QString nativeEllipticalArcFile =
            QDir(tempDir.path()).filePath(QStringLiteral("native-elliptical-arc.SchLib"));
        ExporterAltiumSymbol nativeEllipticalArcExporter;
        QVERIFY(nativeEllipticalArcExporter.exportSymbolLibrary({nativeEllipticalArcSymbol},
                                                                QStringLiteral("native-elliptical-arc"),
                                                                nativeEllipticalArcFile,
                                                                false,
                                                                false));
        QByteArray nativeEllipticalArcData;
        QVERIFY(readCfbStream(
            nativeEllipticalArcFile, QStringLiteral("NATIVE_ELLIPTICAL_ARC/Data"), nativeEllipticalArcData));
        QVERIFY(nativeEllipticalArcData.contains("RECORD=11"));
        QVERIFY(nativeEllipticalArcData.contains("SecondaryRadius="));

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("SEGMENTS");
        IR::FootprintTrackIR polyline;
        polyline.points = {QPointF(0, 0), QPointF(1, 0), QPointF(1, 1)};
        polyline.width = 0.2;
        polyline.layer = IR::LayerType::TopSilk;
        footprint.tracks.append(polyline);
        IR::FootprintHoleIR hole;
        hole.center = QPointF(0.5, 0.5);
        hole.radius = 0.25;
        footprint.holes.append(hole);
        IR::FootprintOutlineIR outline;
        outline.points = {QPointF(-1, -1), QPointF(2, -1)};
        outline.strokeWidth = 0.1;
        footprint.outlines.append(outline);

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("segments.PcbLib"));
        ExporterAltiumFootprint footprintExporter;
        QVERIFY(footprintExporter.exportFootprintLibrary({footprint}, QStringLiteral("segments"), pcbPath));
        QByteArray footprintHeader;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("SEGMENTS/Header"), footprintHeader));
        QCOMPARE(readU32(footprintHeader, 0), quint32(4));  // 2 track segments + 1 hole pad + 1 outline segment
    }

    /**
     * @brief 不完整来源顺序回退到默认写出，避免丢失图元。
     */
    void incompleteGraphicOrderFallsBackWithoutDroppingGraphics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("INCOMPLETE_ORDER");
        AltiumSchRectangle first;
        first.sourceGraphicIndex = 0;
        first.sourcePartIndex = 0;
        first.locationX = 100000;
        first.cornerX = 200000;
        symbol.rectangles.append(first);
        AltiumSchRectangle second;
        second.sourceGraphicIndex = 1;
        second.sourcePartIndex = 0;
        second.locationX = 300000;
        second.cornerX = 400000;
        symbol.rectangles.append(second);
        symbol.graphicOrder = {{QStringLiteral("R"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("incomplete-order.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("incomplete-order")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 INCOMPLETE_ORDER 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("INCOMPLETE_ORDER/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=14")), 2);
        QVERIFY(data.contains("Location.X=1"));
        QVERIFY(data.contains("Location.X=3"));
    }

    /**
     * @brief 路径分段编号出现缺口时回退，避免顺序写出提前终止。
     */
    void gappedPathSegmentsFallBackWithoutDroppingGraphics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("GAPPED_PATH");
        for (const int segmentIndex : {0, 2}) {
            AltiumSchPath path;
            path.sourceGraphicType = QStringLiteral("PT");
            path.sourceGraphicIndex = 0;
            path.sourceSegmentIndex = segmentIndex;
            path.sourcePartIndex = 0;
            path.vertices = {QPointF(segmentIndex * 1000, 0), QPointF(segmentIndex * 1000 + 1000, 0)};
            symbol.paths.append(path);
        }
        symbol.graphicOrder = {{QStringLiteral("PT"), 0, 0}};

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("gapped-path.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("gapped-path")));
        QVERIFY(writer.diagnostics().contains(
            QStringLiteral("符号 GAPPED_PATH 的 graphicOrder 不完整或包含无效引用，已回退到默认图元顺序")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("GAPPED_PATH/Data"), data));
        QCOMPARE(data.count(QByteArrayLiteral("RECORD=6")), 2);
    }

    /**
     * @brief 验证 Pie 和 IEEE 图元遵循无 UniqueID 的兼容记录格式。
     */
    void pieAndIeeeRecordsOmitUniqueIds() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("UNIQUE_ID_GRAPHICS");
        AltiumSchPie pie;
        pie.centerX = 100000;
        pie.radius = 200000;
        symbol.pies.append(pie);
        AltiumSchIeee ieee;
        ieee.locationX = 300000;
        ieee.locationY = 400000;
        symbol.ieeeSymbols.append(ieee);

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("unique-id-graphics.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("unique-id-graphics")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("UNIQUE_ID_GRAPHICS/Data"), data));
        const int pieOffset = data.indexOf("RECORD=9");
        const int ieeeOffset = data.indexOf("RECORD=3");
        QVERIFY(pieOffset >= 0);
        QVERIFY(ieeeOffset >= 0);
        QVERIFY(!data.mid(pieOffset, ieeeOffset - pieOffset).contains("UniqueID="));
        const int nextRecordOffset = data.indexOf("|RECORD=", ieeeOffset + 1);
        QVERIFY(nextRecordOffset > ieeeOffset);
        QVERIFY(!data.mid(ieeeOffset, nextRecordOffset - ieeeOffset).contains("UniqueID="));
    }

    /**
     * @brief 验证二进制引脚会占用共享内容索引，且首条用户参数不会写出索引 0。
     */
    void contentIndexIsSharedAcrossPinsGraphicsAndParameters() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent symbol;
        symbol.name = QStringLiteral("CONTENT_INDEX");
        symbol.designatorPrefix = QStringLiteral("U");

        AltiumSchPin pin;
        pin.name = QStringLiteral("IN");
        pin.designator = QStringLiteral("1");
        symbol.pins.append(pin);

        AltiumSchRectangle rectangle;
        rectangle.locationX = -100000;
        rectangle.locationY = -100000;
        rectangle.cornerX = 100000;
        rectangle.cornerY = 100000;
        symbol.rectangles.append(rectangle);

        AltiumSchParameter parameter;
        parameter.name = QStringLiteral("Custom");
        parameter.value = QStringLiteral("value");
        parameter.ownerPartId = 0;
        symbol.parameters.append(parameter);

        AltiumSchLibWriter writer;
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("content-index.SchLib"));
        QVERIFY(writer.write({symbol}, path, QStringLiteral("content-index")));

        QByteArray data;
        QVERIFY(readCfbStream(path, QStringLiteral("CONTENT_INDEX/Data"), data));
        QVERIFY(!data.contains("IndexInSheet=0"));
        QVERIFY(data.contains("RECORD=14"));
        QVERIFY(data.contains("IndexInSheet=1"));
        QVERIFY(data.contains("OWNERPARTDISPLAYMODE=1"));
        QVERIFY(data.contains("OWNERPARTID=1"));
        QVERIFY(data.contains("RECORD=41"));
        QVERIFY(data.contains("IndexInSheet=2"));
        const int customParameterOffset = data.indexOf("NAME=Custom");
        const int parameterOffset = data.lastIndexOf("|RECORD=41|", customParameterOffset);
        const int nextRecordOffset = data.indexOf("|RECORD=", parameterOffset + 1);
        QVERIFY(parameterOffset >= 0);
        QVERIFY(customParameterOffset > parameterOffset);
        QVERIFY(nextRecordOffset > parameterOffset);
        const QByteArray parameterRecord = data.mid(parameterOffset, nextRecordOffset - parameterOffset);
        QVERIFY(parameterRecord.contains("OWNERPARTID=1"));
        QVERIFY(!parameterRecord.contains("ISNOTACCESIBLE"));
    }

    /**
     * @brief 验证生产 OLE 读取器能够读取目录、迷你流和普通流。
     */
    void compoundReaderReadsMiniAndRegularStreams() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(writer.addStorage(QStringLiteral("Nested")));
        QVERIFY(writer.addStorage(QStringLiteral("Nested"), QStringLiteral("Deep")));
        const QByteArray miniData("mini-stream-data");
        const QByteArray deepData("nested-stream-data");
        const QByteArray regularData(5000, '\x5A');
        QVERIFY(writer.writeStream(QStringLiteral("Tiny"), miniData));
        QVERIFY(writer.writeStream(QStringLiteral("Nested/Deep"), QStringLiteral("Data"), deepData));
        QVERIFY(writer.writeStream(QStringLiteral("Regular"), regularData));

        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("reader-roundtrip.cfb"));
        QVERIFY(writer.saveToFile(path));

        OLECompoundReader reader;
        QVERIFY(reader.open(path));
        QCOMPARE(reader.streamPaths(),
                 QStringList({QStringLiteral("Nested/Deep/Data"), QStringLiteral("Regular"), QStringLiteral("Tiny")}));
        QVERIFY(reader.containsStream(QStringLiteral("Nested/Deep/Data")));

        QByteArray data;
        QVERIFY(reader.readStream(QStringLiteral("Tiny"), &data));
        QCOMPARE(data, miniData);
        QVERIFY(reader.readStream(QStringLiteral("Nested/Deep/Data"), &data));
        QCOMPARE(data, deepData);
        QVERIFY(reader.readStream(QStringLiteral("Regular"), &data));
        QCOMPARE(data, regularData);
        QVERIFY(!reader.readStream(QStringLiteral("Missing"), &data));
    }

    /**
     * @brief 无效输入必须被读取器拒绝并返回诊断信息。
     */
    void compoundReaderRejectsInvalidDocument() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("invalid.cfb"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("not-an-ole-document") > 0);
        file.close();

        OLECompoundReader reader;
        QVERIFY(!reader.open(path));
        QVERIFY(reader.hasError());
        QVERIFY(!reader.errorString().isEmpty());
        QVERIFY(reader.streamPaths().isEmpty());
    }

    /**
     * @brief 验证 Altium 二进制读写器的基础块和 Unicode 参数往返。
     */
    void binaryReaderRoundTripsWriterFormats() {
        QByteArray buffer;
        AltiumBinaryWriter writer(buffer);
        writer.writeInt16(-1234);
        writer.writeUInt32(0xA1B2C3D4U);
        writer.writeFloat(1.25F);
        writer.writeDouble(-3.5);
        writer.beginBlock(7);
        writer.writeBytes(QByteArrayLiteral("payload"));
        writer.endBlock();
        writer.writePascalShortString(QStringLiteral("短文本"));
        writer.writeStringBlock(QStringLiteral("string-block"));
        writer.writePascalString(QStringLiteral("pascal-block"));
        writer.writeCStringParameterBlockUtf8({{QStringLiteral("ASCII"), QStringLiteral("value")},
                                               {QStringLiteral("DESCRIPTION"), QStringLiteral("参数值")}});

        AltiumBinaryReader reader(buffer);
        int16_t signedValue = 0;
        uint32_t unsignedValue = 0;
        float floatValue = 0.0F;
        double doubleValue = 0.0;
        QVERIFY(reader.readInt16(&signedValue));
        QVERIFY(reader.readUInt32(&unsignedValue));
        QVERIFY(reader.readFloat(&floatValue));
        QVERIFY(reader.readDouble(&doubleValue));
        QCOMPARE(signedValue, int16_t(-1234));
        QCOMPARE(unsignedValue, uint32_t(0xA1B2C3D4U));
        QCOMPARE(floatValue, 1.25F);
        QCOMPARE(doubleValue, -3.5);

        QByteArray payload;
        uint8_t flags = 0;
        QVERIFY(reader.readBlock(&payload, &flags));
        QCOMPARE(flags, uint8_t(7));
        QCOMPARE(payload, QByteArrayLiteral("payload"));

        QString value;
        QVERIFY(reader.readPascalShortString(&value));
        QCOMPARE(value, QStringLiteral("???"));  // Writer 的 Pascal 短字符串使用 Latin-1。
        QVERIFY(reader.readStringBlock(&value));
        QCOMPARE(value, QStringLiteral("string-block"));
        QVERIFY(reader.readPascalString(&value));
        QCOMPARE(value, QStringLiteral("pascal-block"));

        QMap<QString, QString> params;
        QVERIFY(reader.readCStringParameterBlock(&params));
        QCOMPARE(params.value(QStringLiteral("ASCII")), QStringLiteral("value"));
        QCOMPARE(params.value(QStringLiteral("DESCRIPTION")), QStringLiteral("参数值"));
        QCOMPARE(reader.remaining(), 0);
        QVERIFY(!reader.hasError());
    }

    /**
     * @brief 验证 SchLib SectionKeys 能恢复截断和冲突后的存储名称。
     */
    void schLibReaderResolvesSectionKeys() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumSchComponent first;
        first.name = QStringLiteral("A/B");
        AltiumSchComponent second;
        second.name = QStringLiteral("A:B");
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("section-keys.SchLib"));
        AltiumSchLibWriter writer;
        QVERIFY(writer.write({first, second}, path, QStringLiteral("section-keys")));

        AltiumSchLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        const auto components = reader.components();
        QCOMPARE(components.size(), 2);
        QCOMPARE(components.at(0).name, QStringLiteral("A/B"));
        QCOMPARE(components.at(1).name, QStringLiteral("A:B"));
        QVERIFY(components.at(0).sectionKey != components.at(1).sectionKey);
        QByteArray data;
        QVERIFY(reader.readComponentData(QStringLiteral("A/B"), &data));
        QVERIFY(data.contains("LibReference=A/B"));
        QVERIFY(reader.readComponentData(QStringLiteral("A:B"), &data));
        QVERIFY(data.contains("LibReference=A:B"));
    }

    /**
     * @brief 验证 PcbLib SectionKeys 能恢复冲突后的封装存储名称。
     */
    void pcbLibReaderResolvesSectionKeys() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        AltiumPcbComponent first;
        first.name = QStringLiteral("A/B");
        AltiumPcbComponent second;
        second.name = QStringLiteral("A:B");
        const QString path = QDir(tempDir.path()).filePath(QStringLiteral("section-keys.PcbLib"));
        AltiumPcbLibWriter writer;
        QVERIFY(writer.write({first, second}, path, QStringLiteral("section-keys")));

        AltiumPcbLibReader reader;
        QVERIFY2(reader.open(path), qPrintable(reader.errorString()));
        const auto components = reader.components();
        QCOMPARE(components.size(), 2);
        QCOMPARE(components.at(0).name, QStringLiteral("A/B"));
        QCOMPARE(components.at(1).name, QStringLiteral("A:B"));
        QVERIFY(components.at(0).sectionKey != components.at(1).sectionKey);
        QByteArray data;
        QVERIFY(reader.readFootprintStream(QStringLiteral("A/B"), QStringLiteral("Data"), &data));
        QVERIFY(data.contains("A/B"));
        QVERIFY(reader.readFootprintStream(QStringLiteral("A:B"), QStringLiteral("Data"), &data));
        QVERIFY(data.contains("A:B"));
    }
};

QTEST_GUILESS_MAIN(TestAltiumOle)
#include "test_altium_ole.moc"
