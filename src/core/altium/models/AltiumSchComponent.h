#pragma once

#include "AltiumCommon.h"

#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

/**
 * @brief Altium 符号引脚
 */
struct AltiumSchPin {
    QString name;  ///< 显示名称
    QString designator;  ///< 引脚编号
    int locationX = 0;  ///< X 坐标（原始单位）
    int locationY = 0;  ///< Y 坐标（原始单位）
    int length = 100000;  ///< 引脚长度（原始单位，默认 10mil）
    AltiumModels::PinElectricalType electricalType = AltiumModels::PinElectricalType::Passive;
    AltiumModels::PinOrientation orientation = AltiumModels::PinOrientation::Right;
    bool showName = true;
    bool showDesignator = true;
    bool isHidden = false;
    uint32_t color = 0x000000;  ///< 颜色（0x00BBGGRR）
    uint8_t symbolInnerEdge = 0;  ///< 主体内侧装饰（0=None, 3=Clock）
    uint8_t symbolOuterEdge = 0;  ///< 引脚外侧装饰（1=Dot 等）
    uint8_t symbolInside = 0;  ///< 引脚内部装饰
    uint8_t symbolOutside = 0;  ///< 引脚外部装饰
    int ownerPartId = 1;  ///< 所属部件（Altium 使用 1-based 编号）
};

/**
 * @brief Altium 符号矩形
 */
struct AltiumSchRectangle {
    int locationX = 0, locationY = 0;  ///< 左上角（原始单位）
    int cornerX = 0, cornerY = 0;  ///< 右下角（原始单位）
    int lineWidth = 0;  ///< 线宽索引 (0-3)
    uint32_t color = 0x000000;
    uint32_t areaColor = 0xFFFFFF;  ///< 填充色
    bool isSolid = true;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号线段
 */
struct AltiumSchLine {
    int locationX = 0, locationY = 0;  ///< 起点
    int cornerX = 0, cornerY = 0;  ///< 终点
    int lineWidth = 0;
    uint32_t color = 0x000000;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号弧线
 */
struct AltiumSchArc {
    int centerX = 0, centerY = 0;  ///< 圆心（原始单位）
    int radius = 0;  ///< 半径（原始单位）
    double startAngle = 0.0;  ///< 起始角度
    double endAngle = 360.0;  ///< 结束角度
    int lineWidth = 0;
    uint32_t color = 0x000000;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号多边形
 */
struct AltiumSchPolygon {
    QList<QPointF> vertices;  ///< 顶点列表（Schematic Units）
    int lineWidth = 0;
    uint32_t color = 0x000000;
    uint32_t areaColor = 0xFFFFFF;
    bool isSolid = true;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号椭圆/圆
 */
struct AltiumSchEllipse {
    int centerX = 0, centerY = 0;
    int radiusX = 0, radiusY = 0;  ///< X/Y 半径（原始单位）
    int lineWidth = 0;
    uint32_t color = 0x000000;
    uint32_t areaColor = 0xFFFFFF;
    bool isSolid = true;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号折线
 */
struct AltiumSchPolyline {
    QList<QPointF> vertices;  ///< 顶点列表（Schematic Units）
    int lineWidth = 0;
    uint32_t color = 0x000000;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号路径
 */
struct AltiumSchPath {
    QList<QPointF> vertices;  ///< 路径顶点（Schematic Units）
    int lineWidth = 0;
    uint32_t color = 0x000000;
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号文本
 */
struct AltiumSchText {
    int locationX = 0, locationY = 0;  ///< 文本锚点位置（原始单位）
    QString text;  ///< 文本内容
    int fontId = 1;  ///< Altium 字体编号
    double fontSizeMm = 0.0;  ///< 原始字体大小（mm，0 表示使用字体编号默认值）
    QString anchor = QStringLiteral("middle");  ///< 原始文本锚点
    uint32_t color = 0x000000;  ///< 颜色（0x00BBGGRR）
    bool isDisplayed = true;  ///< 是否显示文本
    bool isHidden = false;  ///< 是否强制隐藏文本
    int orientation = 0;  ///< 0-3，表示 0°/90°/180°/270°
    int ownerPartId = 1;
};

/**
 * @brief Altium 符号参数字段
 * @details 参数使用 RECORD=41 写入 SchLib Data 流。
 */
struct AltiumSchParameter {
    QString name;  ///< 参数名称
    QString value;  ///< 参数值
    int locationX = 0;  ///< 参数位置 X（原始单位）
    int locationY = 0;  ///< 参数位置 Y（原始单位）
    int fontId = 1;  ///< Altium 字体编号
    uint32_t color = 0x000000;  ///< 颜色（0x00BBGGRR）
    bool isHidden = true;  ///< 是否隐藏
    bool readOnly = false;  ///< 是否只读
    int orientation = 0;  ///< 0-3，表示 0°/90°/180°/270°
    int ownerPartId = -1;  ///< 所属部件，-1 表示所有部件
};

/**
 * @brief Altium 符号元件
 * @details IR 与 SchLib 二进制协议之间的强类型边界，包含图元、引脚及实现关系。
 */
struct AltiumSchComponent {
    QString name;  ///< 元件名称（LibReference）
    QString description;  ///< 元件描述
    QString designatorPrefix = "?";  ///< 位号前缀（如 "R", "C", "U"）
    int partCount = 1;  ///< 部件数量

    /** @brief 符号参数（如制造商、料号、Datasheet 和来源元数据） */
    QMap<QString, QString> sourceMetadata;
    /** @brief 显式参数字段 */
    QList<AltiumSchParameter> parameters;
    /** @brief 符号别名 */
    QStringList aliases;

    QList<AltiumSchPin> pins;
    QList<AltiumSchRectangle> rectangles;
    QList<AltiumSchLine> lines;
    QList<AltiumSchArc> arcs;
    QList<AltiumSchPolygon> polygons;
    QList<AltiumSchEllipse> ellipses;
    QList<AltiumSchPolyline> polylines;
    QList<AltiumSchPath> paths;
    QList<AltiumSchText> texts;

    /** @brief 封装链接（实现记录） */
    struct Implementation {
        QString modelName;  ///< 封装名称
        QString modelType = "PCBLIB";  ///< 模型类型
        QString dataFileKind = "PCBLib";  ///< 数据文件类型
        QString dataFileEntity;  ///< 数据文件实体或路径
        QMap<QString, QString> parameters;  ///< 模型参数
        QMap<QString, QString> pinMappings;  ///< 引脚映射
    };

    QList<Implementation> implementations;
};

}  // namespace EasyKiConverter
