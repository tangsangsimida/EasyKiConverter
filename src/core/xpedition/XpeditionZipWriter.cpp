#include "XpeditionZipWriter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace EasyKiConverter {

namespace {

quint32 crc32(const QByteArray& data) {
    quint32 crc = 0xFFFFFFFFu;
    for (const auto byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & static_cast<quint32>(-(crc & 1u)));
    }
    return ~crc;
}

void appendU16(QByteArray& output, quint16 value) {
    output.append(static_cast<char>(value & 0xFF));
    output.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendU32(QByteArray& output, quint32 value) {
    appendU16(output, static_cast<quint16>(value & 0xFFFF));
    appendU16(output, static_cast<quint16>((value >> 16) & 0xFFFF));
}

bool isSafeEntryName(const QString& name) {
    if (name.isEmpty() || name.startsWith('/') || name.contains('\\'))
        return false;
    const QStringList parts = name.split('/', Qt::KeepEmptyParts);
    for (const QString& part : parts) {
        if (part.isEmpty() || part == "." || part == "..")
            return false;
    }
    return true;
}

}  // namespace

bool XpeditionZipWriter::addFile(const QString& name, const QByteArray& data) {
    if (!isSafeEntryName(name))
        return false;
    for (const Entry& entry : m_entries) {
        if (entry.name == name)
            return false;
    }
    m_entries.append({name, data});
    return true;
}

bool XpeditionZipWriter::write(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QByteArray body;

    struct CentralEntry {
        QByteArray name;
        quint32 crc = 0;
        quint32 size = 0;
        quint32 offset = 0;
    };

    QVector<CentralEntry> central;

    for (const Entry& entry : m_entries) {
        const QByteArray name = entry.name.toUtf8();
        const quint32 offset = static_cast<quint32>(body.size());
        const quint32 checksum = crc32(entry.data);
        appendU32(body, 0x04034B50u);
        appendU16(body, 20);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU32(body, checksum);
        appendU32(body, static_cast<quint32>(entry.data.size()));
        appendU32(body, static_cast<quint32>(entry.data.size()));
        appendU16(body, static_cast<quint16>(name.size()));
        appendU16(body, 0);
        body.append(name);
        body.append(entry.data);
        central.append({name, checksum, static_cast<quint32>(entry.data.size()), offset});
    }

    const quint32 centralOffset = static_cast<quint32>(body.size());
    for (const CentralEntry& entry : central) {
        appendU32(body, 0x02014B50u);
        appendU16(body, 20);
        appendU16(body, 20);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU32(body, entry.crc);
        appendU32(body, entry.size);
        appendU32(body, entry.size);
        appendU16(body, static_cast<quint16>(entry.name.size()));
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU16(body, 0);
        appendU32(body, 0);
        appendU32(body, entry.offset);
        body.append(entry.name);
    }

    const quint32 centralSize = static_cast<quint32>(body.size()) - centralOffset;
    appendU32(body, 0x06054B50u);
    appendU16(body, 0);
    appendU16(body, 0);
    appendU16(body, static_cast<quint16>(central.size()));
    appendU16(body, static_cast<quint16>(central.size()));
    appendU32(body, centralSize);
    appendU32(body, centralOffset);
    appendU16(body, 0);

    return file.write(body) == body.size();
}

}  // namespace EasyKiConverter
