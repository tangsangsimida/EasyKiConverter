#pragma once

#include "compound/OLECompoundWriter.h"
#include "models/AltiumPcbComponent.h"
#include "utils/AltiumBinaryWriter.h"

#include <QList>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium PcbLib 文件写入器
 * @details 将 AltiumPcbComponent 列表写入 .PcbLib 格式的 OLE 复合文档。
 *
 * 参考：AltiumSharp PcbLibWriter.cs
 */
class AltiumPcbLibWriter {
public:
    /**
     * @brief 写入 PcbLib 文件
     * @param components 封装列表
     * @param filePath 输出文件路径
     * @param libraryName 库名称
     * @return 是否成功
     */
    bool write(const QList<AltiumPcbComponent>& components,
               const QString& filePath,
               const QString& libraryName = QString());

private:
    // ---- 文件级写入 ----
    void writeFileHeader(OLECompoundWriter& ole);
    void writeSectionKeys(OLECompoundWriter& ole, const QList<AltiumPcbComponent>& components);
    void writeFileVersionInfo(OLECompoundWriter& ole);
    void writeLibraryStorage(OLECompoundWriter& ole, const QList<AltiumPcbComponent>& components);
    void writeLibraryData(QByteArray& buffer, const QList<AltiumPcbComponent>& components);
    void writeModelsStorage(OLECompoundWriter& ole, const QList<AltiumPcbComponent>& components);
    void writeLayerKindMapping(QByteArray& buffer);
    void writePadViaLibrary(QByteArray& buffer);
    void writeComponentParamsToc(QByteArray& buffer, const QList<AltiumPcbComponent>& components);

    // ---- 封装级写入 ----
    void writeFootprintStorage(OLECompoundWriter& ole, const AltiumPcbComponent& component);
    void writeFootprintParameters(QByteArray& buffer, const AltiumPcbComponent& component);
    void writeFootprintData(QByteArray& buffer, const AltiumPcbComponent& component);
    void writeWideStrings(QByteArray& buffer, const AltiumPcbComponent& component);

    // ---- 图元写入 ----
    void writePad(AltiumBinaryWriter& writer, const AltiumPcbPad& pad, int componentIndex);
    void writeTrack(AltiumBinaryWriter& writer, const AltiumPcbTrack& track, int componentIndex);
    void writeArc(AltiumBinaryWriter& writer, const AltiumPcbArc& arc, int componentIndex);
    void writeText(AltiumBinaryWriter& writer, const AltiumPcbText& text, int componentIndex);
    void writeFill(AltiumBinaryWriter& writer, const AltiumPcbFill& fill, int componentIndex);
    void writeRegion(AltiumBinaryWriter& writer, const AltiumPcbRegion& region, int componentIndex);
    void writeComponentBody(AltiumBinaryWriter& writer, const AltiumPcbComponentBody& body, int componentIndex);

    // ---- 辅助 ----
    void writeCommonPrimitiveHeader(AltiumBinaryWriter& writer,
                                    uint8_t layer,
                                    uint16_t flags,
                                    uint16_t netIndex,
                                    uint16_t componentIndex);
    static uint16_t encodePrimitiveFlags(bool isLocked, bool isTentingTop, bool isTentingBottom, bool isKeepout);
    void writePadExtendedBlock(AltiumBinaryWriter& writer, const AltiumPcbPad& pad);
    void writeUniqueIdPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component);
    void writeExtendedPrimitiveInformation(QByteArray& buffer, const AltiumPcbComponent& component);
    uint32_t toV7LayerId(uint8_t layer) const;
    int countPrimitives(const AltiumPcbComponent& component) const;
    QString getSectionKey(const QString& name) const;

    // 广字符串管理
    int addWideString(const QString& text);
    QStringList m_wideStrings;
};

}  // namespace EasyKiConverter
