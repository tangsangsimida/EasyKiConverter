#pragma once

#include "core/altium/compound/OLECompoundReader.h"

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief Altium PcbLib 库级读取器
 * @details 解析 FileHeader、Library/Data、SectionKeys 以及封装 Storage 映射。
 *          当前只读，不执行增量写回。
 */
class AltiumPcbLibReader {
public:
    /** @brief PcbLib 封装目录项 */
    struct ComponentInfo {
        QString name;
        QString sectionKey;
    };

    /** @brief PcbLib Data 中的已知图元块 */
    struct PrimitiveBlock {
        quint8 flags = 0;
        QByteArray payload;
        QByteArray encoded;
    };

    /** @brief PcbLib 走线公共字段之后的结构化数据 */
    struct TrackFields {
        qint32 startX = 0;
        qint32 startY = 0;
        qint32 endX = 0;
        qint32 endY = 0;
        qint32 width = 0;
        quint16 netIndex = 0;
        quint8 componentIndex = 0;
    };

    /** @brief PcbLib 弧线公共字段之后的结构化数据 */
    struct ArcFields {
        qint32 centerX = 0;
        qint32 centerY = 0;
        qint32 radius = 0;
        double startAngle = 0.0;
        double endAngle = 0.0;
        qint32 width = 0;
    };

    /** @brief PcbLib 文本主块的结构化数据 */
    struct TextFields {
        qint32 locationX = 0;
        qint32 locationY = 0;
        qint32 height = 0;
        qint16 fontId = 0;
        double rotation = 0.0;
        quint8 mirrored = 0;
        qint32 strokeWidth = 0;
        quint8 isComment = 0;
        quint8 isDesignator = 0;
        quint8 characterSet = 0;
        quint8 baseFontType = 0;
        quint32 wideStringIndex = 0;
        quint8 kind = 0;
        quint32 v7LayerId = 0;
    };

    /** @brief PcbLib 焊盘主块和扩展块的结构化数据 */
    struct PadFields {
        qint32 locationX = 0;
        qint32 locationY = 0;
        qint32 sizeTopX = 0;
        qint32 sizeTopY = 0;
        qint32 sizeMidX = 0;
        qint32 sizeMidY = 0;
        qint32 sizeBotX = 0;
        qint32 sizeBotY = 0;
        qint32 holeSize = 0;
        quint8 shapeTop = 0;
        quint8 shapeMid = 0;
        quint8 shapeBot = 0;
        double rotation = 0.0;
        quint8 isPlated = 0;
        quint8 stackMode = 0;
        quint8 mode = 0;
        quint8 powerPlaneConnectStyle = 0;
        qint32 reliefAirGapRaw = 0;
        qint32 reliefConductorWidthRaw = 0;
        qint16 reliefEntries = 0;
        qint32 powerPlaneClearanceRaw = 0;
        qint32 powerPlaneReliefExpansionRaw = 0;
        qint32 pasteMaskExpansionRaw = 0;
        qint32 solderMaskExpansionRaw = 0;
        quint8 pasteMaskExpansionMode = 0;
        quint8 solderMaskExpansionMode = 0;
        quint8 drillType = 0;
        quint8 holeType = 0;
        qint32 holeSlotLengthRaw = 0;
        double holeRotation = 0.0;
        quint8 hasRoundedRect = 0;
        quint8 extendedShapeTop = 0;
        quint8 extendedShapeMid = 0;
        quint8 extendedShapeBottom = 0;
        quint8 cornerRadiusPercentage = 0;
    };

    /**
     * @brief PcbLib Data 中的图元对象
     * @details 保留对象 ID、所有子块和完整编码；未知对象类型不会被猜测解析。
     */
    struct PrimitiveRecord {
        quint8 objectId = 0;
        quint8 layer = 0;  ///< 图元公共头部中的层编号
        quint16 primitiveFlags = 0;  ///< 图元公共头部中的标志位
        bool hasTrackFields = false;
        TrackFields track;
        bool hasArcFields = false;
        ArcFields arc;
        bool hasPadFields = false;
        PadFields pad;
        bool hasTextFields = false;
        TextFields textFields;
        QString designator;
        QString text;
        QVector<PrimitiveBlock> blocks;
        QByteArray encoded;
    };

    /**
     * @brief 打开并解析 PcbLib 文件
     * @param filePath 文件路径
     * @return 结构完整且封装映射有效时返回 true
     */
    bool open(const QString& filePath);

    /** @brief 获取封装目录 */
    QVector<ComponentInfo> components() const;
    /** @brief 获取 PcbLib 文件版本文本 */
    QString fileVersion() const;
    /** @brief 获取 Library/Data 中的元数据块 */
    QByteArray libraryMetadata() const;
    /** @brief 读取指定封装的指定流 */
    bool readFootprintStream(int componentIndex, const QString& streamName, QByteArray* data) const;
    /** @brief 读取指定封装的指定流 */
    bool readFootprintStream(const QString& componentName, const QString& streamName, QByteArray* data) const;
    /**
     * @brief 扫描指定封装 Data 流中的已知图元对象
     * @details Data 流首部的封装名称不会作为对象返回。当前支持项目写入器使用的对象类型；
     *          遇到未知对象 ID 或损坏子块时返回 false，调用方仍可通过 readFootprintStream 获取原始流。
     */
    bool readFootprintObjects(int componentIndex, QVector<PrimitiveRecord>* objects) const;
    /** @brief 按封装名称扫描已知图元对象 */
    bool readFootprintObjects(const QString& componentName, QVector<PrimitiveRecord>* objects) const;
    /** @brief 获取最近一次错误说明 */
    QString errorString() const;
    /** @brief 获取是否发生读取错误 */
    bool hasError() const;

private:
    bool fail(const QString& message);
    int componentIndex(const QString& componentName) const;

    OLECompoundReader m_oleReader;
    QVector<ComponentInfo> m_components;
    QString m_fileVersion;
    QByteArray m_libraryMetadata;
    mutable QString m_errorMessage;
};

}  // namespace EasyKiConverter
