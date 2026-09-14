#pragma once

#include "compound/OLECompoundWriter.h"
#include "models/AltiumSchComponent.h"
#include "utils/AltiumBinaryWriter.h"

#include <QHash>
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

    /**
     * @brief 获取最近一次写入产生的非致命诊断
     * @return 图片文件名、嵌入数据等被跳过或修正时的诊断列表
     */
    QStringList diagnostics() const {
        return m_diagnostics;
    }

private:
    // ---- 文件级写入 ----
    void writeFileHeader(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components);
    void writeSectionKeys(OLECompoundWriter& ole,
                          const QList<AltiumSchComponent>& components,
                          const QStringList& sectionKeys);
    void writeComponentStorage(OLECompoundWriter& ole, const AltiumSchComponent& component, const QString& sectionKey);

    // ---- 记录写入 ----
    void writeComponentRecord(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    void writePinRecord(AltiumBinaryWriter& writer, const AltiumSchPin& pin);
    void writeRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRectangle& rect);
    void writeRoundRectangleRecord(AltiumBinaryWriter& writer, const AltiumSchRoundRectangle& rect);
    void writeLineRecord(AltiumBinaryWriter& writer, const AltiumSchLine& line);
    void writeArcRecord(AltiumBinaryWriter& writer, const AltiumSchArc& arc);
    void writePolygonRecord(AltiumBinaryWriter& writer, const AltiumSchPolygon& polygon);
    void writeEllipseRecord(AltiumBinaryWriter& writer, const AltiumSchEllipse& ellipse);
    void writePieRecord(AltiumBinaryWriter& writer, const AltiumSchPie& pie);
    void writeEllipticalArcRecord(AltiumBinaryWriter& writer, const AltiumSchEllipticalArc& arc);
    void writePolylineRecord(AltiumBinaryWriter& writer, const AltiumSchPolyline& polyline);
    void writePathRecord(AltiumBinaryWriter& writer, const AltiumSchPath& path);
    void writeBezierRecord(AltiumBinaryWriter& writer, const AltiumSchBezier& bezier);
    void writeIeeeRecord(AltiumBinaryWriter& writer, const AltiumSchIeee& ieee);
    void writeTextRecord(AltiumBinaryWriter& writer, const AltiumSchText& text);
    void writeTextFrameRecord(AltiumBinaryWriter& writer, const AltiumSchTextFrame& frame);
    void writeImageRecord(AltiumBinaryWriter& writer, const AltiumSchImage& image);
    void writeOrderedGraphic(AltiumBinaryWriter& writer,
                             const AltiumSchComponent& component,
                             const AltiumSchGraphicOrder& order);
    void prepareImageStorageNames(const QList<AltiumSchComponent>& components);
    void writeImageStorage(OLECompoundWriter& ole, const QList<AltiumSchComponent>& components);
    void writeComponentParameterRecords(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    void writeImplementationRecords(AltiumBinaryWriter& writer, const AltiumSchComponent& component);
    bool hasCompleteGraphicOrder(const AltiumSchComponent& component) const;

    // ---- 辅助 ----
    QString getSectionKey(const QString& name) const;
    int getOrAddFont(const QString& fontName,
                     int fontSize,
                     bool bold = false,
                     bool italic = false,
                     bool underline = false);
    void registerTextFonts(const QList<AltiumSchComponent>& components);
    void addCoordParam(QMap<QString, QString>& params, const QString& key, int raw);
    void addColorParam(QMap<QString, QString>& params, const QString& key, uint32_t color);
    void addUniqueID(QMap<QString, QString>& params);
    void addContentIndex(QMap<QString, QString>& params);
    int componentRecordCount(const AltiumSchComponent& component) const;
    int componentParameterRecordCount(const AltiumSchComponent& component) const;
    void addOwnerParams(QMap<QString, QString>& params, int ownerPartId);

    // 字体表管理
    QList<AltiumModels::FontEntry> m_fonts;
    QHash<const AltiumSchImage*, QString> m_embeddedImageNames;
    int m_uniqueIdCounter = 0;
    int m_nextIndexInSheet = 0;
    QString m_libraryName;
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
