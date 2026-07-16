#pragma once

#include "compound/OLECompoundWriter.h"
#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

#include <QList>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium SchLib 文件写入器
 * @details 将 AltiumSchComponent 列表写入 .SchLib 格式的 OLE 复合文档。
 *
 * OLE 流结构：
 * Root/
 *   FileHeader          — 库头参数 + 元件名称列表
 *   SectionKeys         — 元件名到存储键的映射（可选）
 *   <SectionKey>/       — 每个元件一个存储区
 *     Data              — 所有图元记录
 *   Storage             — 嵌入图像数据（可选）
 *
 * 参考：AltiumSharp SchLibWriter.cs
 */
class AltiumSchLibWriter {
public:
    /**
     * @brief 写入 SchLib 文件
     * @param components 元件列表
     * @param filePath 输出文件路径
     * @param libraryName 库名称
     * @return 是否成功
     */
    bool write(const QList<AltiumSchComponent>& components,
               const QString& filePath,
               const QString& libraryName = QString());

private:
    // ---- 文件级写入 ----
    void writeFileHeader(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components);
    void writeSectionKeys(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components);
    void writeComponentStorage(OLECompoundWriter& ole, const AltiumSchComponent& component);

    // ---- 记录写入 ----
    void writeComponentRecord(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    void writePinRecord(AltiumBinaryWriter& writer, const AltiumSchPin& pin, int partId);
    void writeRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRectangle& rect);
    void writeLineRecord(AltiumBinaryWriter& writer, const AltiumSchLine& line);
    void writeArcRecord(AltiumBinaryWriter& writer, const AltiumSchArc& arc);
    void writePolygonRecord(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon);
    void writeEllipseRecord(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse);
    void writePolylineRecord(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline);
    void writeTextRecord(AltiumBinaryWriter& writer, const AltiumSchText& text);
    void writeImplementationRecords(AltiumBinaryWriter& writer, const AltiumSchComponent& component);

    // ---- 辅助 ----
    QString getSectionKey(const QString& name) const;
    int getOrAddFont(const QString& fontName,
                     int fontSize,
                     bool bold = false,
                     bool italic = false,
                     bool underline = false);
    void addCoordParam(QMap<QString, QString>& params, const QString& key, int raw);
    void addColorParam(QMap<QString, QString>& params, const QString& key, uint32_t color);
    void addUniqueID(QMap<QString, QString>& params);

    // 字体表管理
    QList<AltiumModels::FontEntry> m_fonts;
    int m_uniqueIdCounter = 0;
};

}  // namespace EasyKiConverter
