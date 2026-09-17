#include "AltiumSchImageStorageEncoder.h"

#include "utils/AltiumBinaryWriter.h"

#include <QDataStream>
#include <QIODevice>

namespace EasyKiConverter {

/** @brief 压缩图片并组装 Altium Storage 流中的条目和头部。 */
QByteArray AltiumSchImageStorageEncoder::encode(const QList<AltiumSchComponent>& components,
                                                const QHash<const AltiumSchImage*, QString>& imageNames,
                                                QStringList& diagnostics) {
    QList<const AltiumSchImage*> embeddedImages;
    for (const AltiumSchComponent& component : components) {
        for (const AltiumSchImage& image : component.images) {
            if (image.embedImage && !image.data.isEmpty() && imageNames.contains(&image))
                embeddedImages.append(&image);
        }
    }

    constexpr qsizetype kMaxStorageEntrySize = 0x00FFFFFF;
    QList<QByteArray> encodedEntries;
    encodedEntries.reserve(embeddedImages.size());
    for (const AltiumSchImage* image : embeddedImages) {
        const QByteArray compressed = qCompress(image->data, 9).mid(4);
        const QByteArray name = imageNames.value(image).toLocal8Bit();
        if (compressed.isEmpty() || name.isEmpty() || name.size() > 255) {
            diagnostics.append(
                QStringLiteral("Altium SchLib 图片 Storage 条目无效，已跳过: %1").arg(QString::fromLocal8Bit(name)));
            continue;
        }

        QByteArray entry;
        QDataStream entryStream(&entry, QIODevice::WriteOnly);
        entryStream.setByteOrder(QDataStream::LittleEndian);
        entryStream << static_cast<quint8>(0xD0) << static_cast<quint8>(name.size());
        entry.append(name);
        entryStream.device()->seek(entry.size());
        entryStream << static_cast<quint32>(compressed.size());
        entry.append(compressed);
        if (entry.size() > kMaxStorageEntrySize) {
            diagnostics.append(
                QStringLiteral("Altium SchLib 图片 Storage 条目过大，已跳过: %1").arg(QString::fromLocal8Bit(name)));
            continue;
        }
        encodedEntries.append(entry);
    }

    QByteArray storageData;
    AltiumBinaryWriter storageWriter(storageData);
    QMap<QString, QString> storageParams;
    storageParams["HEADER"] = "Icon storage";
    storageParams["Weight"] = QString::number(encodedEntries.size());
    storageWriter.writeCStringParameterBlock(storageParams);

    for (const QByteArray& entry : encodedEntries) {
        QDataStream blockStream(&storageData, QIODevice::WriteOnly | QIODevice::Append);
        blockStream.setByteOrder(QDataStream::LittleEndian);
        blockStream << static_cast<quint32>(0x01000000U | static_cast<quint32>(entry.size()));
        blockStream.writeRawData(entry.constData(), entry.size());
    }
    return storageData;
}

}  // namespace EasyKiConverter
