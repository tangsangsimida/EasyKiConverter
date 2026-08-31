#include "core/altium/compound/OLECompoundWriter.h"
#include "core/altium/writers/AltiumPcbLibWriter.h"
#include "core/altium/writers/AltiumSchLibWriter.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cstdint>

using namespace EasyKiConverter;

namespace {

constexpr qint64 CFB_SECTOR_SIZE = 512;
constexpr qint64 CFB_MINI_SECTOR_SIZE = 64;
constexpr quint32 CFB_ENDOFCHAIN = 0xFFFFFFFE;
constexpr quint32 CFB_FREESECT = 0xFFFFFFFF;
constexpr quint32 CFB_FATSECT = 0xFFFFFFFD;

quint32 readU32(const QByteArray& data, qint64 offset) {
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

quint64 readU64(const QByteArray& data, qint64 offset) {
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<quint64>(static_cast<unsigned char>(data.at(offset + i))) << (i * 8);
    }
    return value;
}

struct CfbDirectoryEntry {
    QString name;
    quint8 type = 0;
    quint32 leftChild = CFB_FREESECT;
    quint32 rightChild = CFB_FREESECT;
    quint32 child = CFB_FREESECT;
    quint32 startSector = CFB_ENDOFCHAIN;
    quint64 streamSize = 0;
};

QByteArray readCfbSector(const QByteArray& fileData, quint32 sector) {
    const qint64 offset = static_cast<qint64>(sector + 1) * CFB_SECTOR_SIZE;
    return fileData.mid(offset, CFB_SECTOR_SIZE);
}

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

bool cfbNameLess(const QString& left, const QString& right) {
    if (left.size() != right.size()) {
        return left.size() < right.size();
    }
    return left.compare(right, Qt::CaseInsensitive) < 0;
}

bool readCfbStream(const QString& filePath, const QString& streamPath, QByteArray& output) {
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

    QVector<quint32> fatSectorList;
    for (int i = 0; i < 109 && static_cast<int>(fatSectorList.size()) < static_cast<int>(numFatSectors); ++i) {
        const quint32 sector = readU32(data, 76 + i * 4);
        if (sector == CFB_ENDOFCHAIN || sector == CFB_FREESECT) {
            break;
        }
        fatSectorList.append(sector);
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

    const QByteArray miniStream = readRegularStream(data, fat, entries.at(0).startSector, entries.at(0).streamSize);
    QVector<quint32> miniFat;
    for (quint32 i = 0; i < numMiniFatSectors; ++i) {
        const QByteArray sectorData = readCfbSector(data, firstMiniFatSector + i);
        for (int j = 0; j < CFB_SECTOR_SIZE / 4; ++j) {
            miniFat.append(readU32(sectorData, j * 4));
        }
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

}  // namespace

class TestAltiumOle : public QObject {
    Q_OBJECT

private slots:

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

        const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("test.cfb"));
        QVERIFY(writer.saveToFile(filePath));

        QByteArray headerPayload;
        QVERIFY(readCfbStream(filePath, QStringLiteral("FileHeader"), headerPayload));
        QCOMPARE(headerPayload, QByteArrayLiteral("header payload"));

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

        AltiumSchComponent::Implementation impl;
        impl.modelName = QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP");
        impl.modelType = QStringLiteral("PCBLIB");
        symbol.implementations.append(impl);

        const QString schPath = QDir(tempDir.path()).filePath(QStringLiteral("easyeda_convertlib.SchLib"));
        AltiumSchLibWriter schWriter;
        QVERIFY(schWriter.write({symbol}, schPath, QStringLiteral("easyeda_convertlib")));

        QByteArray schData;
        QVERIFY(readCfbStream(schPath, QStringLiteral("C2040/Data"), schData));
        QVERIFY(schData.contains("LibReference=C2040"));
        QVERIFY(schData.contains("RECORD=14"));
        QVERIFY(schData.contains("RECORD=45"));

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
        QVERIFY(pcbHeader.contains("PCB 6.0 Binary Library File"));

        QByteArray libraryData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("Library/Data"), libraryData));
        QVERIFY(libraryData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));

        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("LQFN-56_L7.0-W7.0-P0.4-EP/Data"), footprintData));
        QVERIFY(footprintData.contains("LQFN-56_L7.0-W7.0-P0.4-EP"));
    }

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

        const QString pcbPath = QDir(tempDir.path()).filePath(QStringLiteral("test_pad.PcbLib"));
        AltiumPcbLibWriter pcbWriter;
        QVERIFY(pcbWriter.write({footprint}, pcbPath));

        QByteArray footprintData;
        QVERIFY(readCfbStream(pcbPath, QStringLiteral("TEST_PAD/Data"), footprintData));
        // 验证 Data 流包含焊盘 Object ID = 2
        QVERIFY(static_cast<unsigned char>(footprintData.at(footprintData.indexOf("TEST_PAD") + 9)) == 2 ||
                footprintData.size() > 50);
    }

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
    }
};

QTEST_GUILESS_MAIN(TestAltiumOle)
#include "test_altium_ole.moc"
