#include "AltiumSchImageStorageWriter.h"

#include "AltiumSchImageStorageEncoder.h"
#include "AltiumSchLibWriter.h"
#include "utils/AltiumWriterUtils.h"

#include <QFileInfo>
#include <QSet>

namespace EasyKiConverter {

/** @brief 保存协作者所属的 SchLib 主写入器。 */
AltiumSchImageStorageWriter::AltiumSchImageStorageWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/**
 * @brief 校验嵌入图片名称并生成不冲突的 Storage 文件名。
 * @details 无效图片不会进入 Storage，但仍通过主写入器记录诊断信息。
 */
void AltiumSchImageStorageWriter::prepareNames(const QList<AltiumSchComponent>& components) {
    QSet<QString> usedNames;
    for (const AltiumSchComponent& component : components) {
        for (int imageIndex = 0; imageIndex < component.images.size(); ++imageIndex) {
            const AltiumSchImage& image = component.images.at(imageIndex);
            if (!image.embedImage) {
                if (image.fileName.trimmed().isEmpty()) {
                    const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的外部文件名为空，已跳过文件引用")
                                                   .arg(component.name)
                                                   .arg(imageIndex);
                    m_owner.m_diagnostics.append(diagnostic);
                    qWarning() << "AltiumSchLibWriter:" << diagnostic;
                }
                continue;
            }

            QString sourceName = image.fileName;
            sourceName.replace('\\', '/');
            const QString embeddedName = QFileInfo(sourceName).fileName();
            if (image.data.isEmpty()) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入数据为空，已跳过 Storage")
                                               .arg(component.name)
                                               .arg(imageIndex);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }
            if (embeddedName.toLocal8Bit().size() > 255) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入文件名超过 255 字节，已跳过 Storage")
                                               .arg(component.name)
                                               .arg(imageIndex);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }
            if (!AltiumWriterUtils::isValidImageStorageName(embeddedName)) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入文件名无效: %3，已跳过 Storage")
                                               .arg(component.name)
                                               .arg(imageIndex)
                                               .arg(image.fileName);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
                continue;
            }

            const QFileInfo fileInfo(embeddedName);
            const QString suffix = fileInfo.suffix();
            const QString baseName =
                suffix.isEmpty() ? embeddedName : embeddedName.left(embeddedName.size() - suffix.size() - 1);
            QString candidate = embeddedName;
            int duplicateIndex = 1;
            while (usedNames.contains(candidate.toCaseFolded())) {
                ++duplicateIndex;
                const QString suffixText = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
                const QString marker = QStringLiteral("_%1").arg(duplicateIndex);
                QString trimmedBase = baseName;
                while (!trimmedBase.isEmpty() && (trimmedBase + marker + suffixText).toLocal8Bit().size() > 255)
                    trimmedBase.chop(1);
                candidate = trimmedBase + marker + suffixText;
            }
            usedNames.insert(candidate.toCaseFolded());
            m_owner.m_embeddedImageNames.insert(&image, candidate);
            if (candidate != embeddedName) {
                const QString diagnostic = QStringLiteral("组件 %1 图片 %2 的嵌入文件名 %3 重复，已改为 %4")
                                               .arg(component.name)
                                               .arg(imageIndex)
                                               .arg(embeddedName)
                                               .arg(candidate);
                m_owner.m_diagnostics.append(diagnostic);
                qWarning() << "AltiumSchLibWriter:" << diagnostic;
            }
        }
    }
}

/** @brief 使用统一编码器写入根 Storage 图片流。 */
void AltiumSchImageStorageWriter::write(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components) {
    const QByteArray storageData =
        AltiumSchImageStorageEncoder::encode(components, m_owner.m_embeddedImageNames, m_owner.m_diagnostics);
    ole.writeStream("Storage", storageData);
}

}  // namespace EasyKiConverter
