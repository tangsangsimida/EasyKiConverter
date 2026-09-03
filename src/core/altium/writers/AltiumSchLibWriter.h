#pragma once

#include "compound/OLECompoundWriter.h"
#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

#include <QList>
#include <QString>
#include <QStringList>

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
 * 文件索引、组件记录及子图元归属在同一模型上计算，保证 Header 的计数与 Data 流一致。
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
    void writeSectionKeys(OLECompoundWriter& ole,
                          const QList<AltiumSchComponent>& components,
                          const QStringList& sectionKeys);
    void writeComponentStorage(OLECompoundWriter& ole, const AltiumSchComponent& component, const QString& sectionKey);

    // ---- 记录写入 ----
    void writeComponentRecord(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    void writePinRecord(AltiumBinaryWriter& writer, const AltiumSchPin& pin, int partId);
    void writeRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRectangle& rect);
    void writeLineRecord(AltiumBinaryWriter& writer, const AltiumSchLine& line);
    void writeArcRecord(AltiumBinaryWriter& writer, const AltiumSchArc& arc);
    void writePolygonRecord(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon);
    void writeEllipseRecord(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse);
    void writePolylineRecord(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline);
    void writePathRecord(AltiumBinaryWriter& writer, const AltiumSchPath& path);
    void writeTextRecord(AltiumBinaryWriter& writer, const AltiumSchText& text);
    void writeComponentParameterRecords(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
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
    int componentRecordCount(const AltiumSchComponent& component) const;
    void addOwnerParams(QMap<QString, QString>& params, int ownerPartId) const;

    // 字体表管理
    QList<AltiumModels::FontEntry> m_fonts;
    int m_uniqueIdCounter = 0;
    QString m_libraryName;
};

}  // namespace EasyKiConverter
