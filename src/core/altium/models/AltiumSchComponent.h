#pragma once

#include "AltiumCommon.h"

#include <QList>
#include <QPointF>
#include <QString>

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
    int locationX = 0, locationY = 0;
    QString text;
    int fontId = 1;
    uint32_t color = 0x000000;
    bool isHidden = false;
    int orientation = 0;  ///< 0-3，表示 0°/90°/180°/270°
    int ownerPartId = 1;
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
    };

    QList<Implementation> implementations;
};

}  // namespace EasyKiConverter
