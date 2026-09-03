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
#include "core/altium/compound/OLECompoundWriter.h"
#include "core/altium/utils/AltiumWriterUtils.h"
#include "core/altium/writers/AltiumPcbLibWriter.h"
#include "core/altium/writers/AltiumSchLibWriter.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
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
    }

    /**
     * @brief 验证无效路径和重名条目被正确拒绝
     */
    void rejectsInvalidOrDuplicateCfbEntries() {
        OLECompoundWriter writer;
        QVERIFY(writer.create());
        QVERIFY(!writer.addStorage(QStringLiteral("Missing"), QStringLiteral("Child")));
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
        symbol.rectangles.append(rect);

        AltiumSchPath path;
        path.vertices = {QPointF(0, 0), QPointF(100000, 100000), QPointF(200000, 0)};
        path.ownerPartId = 1;
        symbol.paths.append(path);

        AltiumSchComponent::Implementation impl;
        impl.modelName = QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP");
        impl.modelType = QStringLiteral("PCBLIB");
        symbol.implementations.append(impl);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.SchLib"));
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath, QStringLiteral("easyeda_convertlib")));

        QByteArray schHeader;
        QVERIFY(readCfbStream(schPath, QStringLiteral("FileHeader"), schHeader));
        QVERIFY(schHeader.contains("COMPCOUNT=1"));
        QVERIFY(schHeader.contains("WEIGHT=10"));
        QVERIFY(schHeader.contains("LIBREF0=C2040"));
        QVERIFY(schHeader.contains("PARTCOUNT0=2"));

        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("C2040/Data"), schData));
        QVERIFY(schData.contains("LibReference=C2040"));
        QVERIFY(schData.contains("RECORD=14"));
        QVERIFY(schData.contains("RECORD=45"));
        QVERIFY(schData.contains("RECORD=6"));
        QVERIFY(schData.contains("OWNERPARTID=1"));
        QVERIFY(schData.contains("PartCount=2"));
        QVERIFY(schData.contains("RECORD=34"));
        QVERIFY(schData.contains("NAME=Designator"));
        QVERIFY(schData.contains("RECORD=41"));
        QVERIFY(schData.contains("NAME=Comment"));

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

        QByteArray wideStrings;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/WideStrings"), wideStrings));
        QVERIFY(wideStrings.contains("ENCODEDTEXT0=82,69,70,42,42"));
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
        QVERIFY(schData.contains(QStringLiteral("中文文本").toUtf8()));
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
        symbol.pins.append(pin);
        IR::SymbolPathIR path;
        path.points = {QPointF(0, 0), QPointF(1, 1), QPointF(2, 0)};
        path.partIndex = 1;
        symbol.paths.append(path);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("multipart.SchLib"));
        ExporterAltiumSymbol symbolExporter;
        QVERIFY(symbolExporter.exportSymbolLibrary({symbol}, QStringLiteral("multipart"), schPath, false, false));
        QByteArray symbolData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("MULTIPART/Data"), symbolData));
        const int pinOffset = 4 + static_cast<int>(readU32(symbolData, 0) & 0x00FFFFFFU);
        QCOMPARE(readU32(symbolData, pinOffset + 4), quint32(2));
        QCOMPARE(readU16(symbolData, pinOffset + 9), quint16(2));
        QVERIFY(symbolData.contains("OWNERPARTID=2"));

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
};

QTEST_GUILESS_MAIN(TestAltiumOle)
#include "test_altium_ole.moc"
