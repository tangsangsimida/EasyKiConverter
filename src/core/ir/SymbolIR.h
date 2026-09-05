#pragma once

/**
 * @file SymbolIR.h
 * @brief 原理图符号的通用中间表示
 *
 * 替代 SymbolData.h 中的结构体，移除所有 EasyEDA 特有格式：
 * - 坐标以 QList<QPointF> 存储（替代原始字符串）
 * - 引脚类型/方向使用枚举（替代整数）
 * - 文本属性使用标准类型（替代字符串编码）
 */

#include "IRTypes.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 引脚样式（电气语义层）
 *
 * 描述引脚的电气语义特征，与具体 EDA 格式的图形表示解耦。
 * 各导出器根据自身能力将这些语义映射为图形装饰。
 */
struct SymbolPinStyle {
    bool inverted = false;  ///< 取反（低电平有效）
    bool clock = false;  ///< 时钟信号
    bool activeLow = false;  ///< 低电平有效（语义标记，与 inverted 不同）
    PinDecoration decoration = PinDecoration::None;  ///< IEEE 图形装饰

    /** @brief 是否有任何样式标记 */
    bool hasAny() const {
        return inverted || clock || activeLow || decoration != PinDecoration::None;
    }
};

/**
 * @brief 引脚显示控制
 *
 * 控制引脚名称和编号的显示方式，包括锚点和字体信息。
 * 适用于需要精确控制文本位置的格式（如 Altium）。
 */
struct SymbolPinDisplay {
    bool showName = true;  ///< 是否显示引脚名称
    bool showDesignator = true;  ///< 是否显示引脚编号
    TextAnchor nameAnchor = TextAnchor::Start;  ///< 名称文本锚点
    TextAnchor numberAnchor = TextAnchor::Start;  ///< 编号文本锚点
    TextOrientation nameOrientation = TextOrientation::Horizontal;  ///< 名称文本方向
    TextOrientation numberOrientation = TextOrientation::Horizontal;  ///< 编号文本方向
    double nameFontSizeMm = 0.0;  ///< 名称字体大小（mm，0 表示默认）
    double numberFontSizeMm = 0.0;  ///< 编号字体大小（mm，0 表示默认）
};

/**
 * @brief 通用符号引脚
 *
 * 替代 SymbolPin 及其子结构体（SymbolPinSettings, SymbolPinDot,
 * SymbolPinPath, SymbolPinName, SymbolPinDotBis, SymbolPinClock）。
 * 所有字段均为解析后的最终值，不再存储原始字符串。
 *
 * 结构分为三层：
 * - 几何：position, length, direction
 * - 电气语义：electricalType, style
 * - 显示控制：display
 */
struct SymbolPinIR {
    // === 几何 ===
    QString name;  ///< 引脚名称（如 "VCC", "A0"）
    QString designator;  ///< 引脚编号（如 "1", "2"）
    QPointF position;  ///< 引脚位置（已解析，单位 mm）
    double length = 0.0;  ///< 引脚长度（mm，替代 SVG path 解析）
    PinDirection direction = PinDirection::Right;  ///< 引脚方向

    // === 电气语义 ===
    PinElectricalType electricalType = PinElectricalType::Unspecified;  ///< 电气类型
    SymbolPinStyle style;  ///< 引脚样式（语义层）

    // === 显示控制 ===
    SymbolPinDisplay display;  ///< 引脚显示控制

    // === 兼容性字段（过渡期，新代码请使用 style/display） ===
    bool showName = true;  ///< @deprecated 使用 display.showName
    bool showDesignator = true;  ///< @deprecated 使用 display.showDesignator
    bool hasDot = false;  ///< @deprecated 使用 style.inverted
    bool hasClock = false;  ///< @deprecated 使用 style.clock

    // === 部件 ===
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号矩形
 * @note 使用未归一化的起点/终点坐标，而非 QRectF（QRectF 会归一化负宽高导致坐标信息丢失）
 */
struct SymbolRectangleIR {
    double x0 = 0.0;  ///< 起点 X（mm，已转换）
    double y0 = 0.0;  ///< 起点 Y（mm，已转换，KiCad Y 翻转后）
    double x1 = 0.0;  ///< 终点 X（mm，已转换）
    double y1 = 0.0;  ///< 终点 Y（mm，已转换，KiCad Y 翻转后）
    QColor strokeColor = Qt::black;  ///< 边框颜色
    double strokeWidth = 0.0;  ///< 边框宽度（mm）
    QColor fillColor = Qt::transparent;  ///< 填充颜色
    bool isFilled = false;  ///< 是否填充
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号圆形
 */
struct SymbolCircleIR {
    QPointF center;  ///< 圆心（已解析，单位 mm）
    double radius = 0.0;  ///< 半径（mm）
    QColor strokeColor = Qt::black;
    double strokeWidth = 0.0;
    QColor fillColor = Qt::transparent;
    bool isFilled = false;
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号圆弧
 *
 * @note EasyEDA 的 SymbolArc.path 存储 3 个 QPointF（起点、中点、终点），
 *       保留原始三点表示以便直接生成 KiCad 三点式圆弧。
 */
struct SymbolArcIR {
    QPointF startPoint;  ///< 起点（已解析，单位 mm）
    QPointF midPoint;  ///< 中点（已解析，单位 mm）
    QPointF endPoint;  ///< 终点（已解析，单位 mm）
    QColor strokeColor = Qt::black;
    double strokeWidth = 0.0;
    QColor fillColor = Qt::transparent;
    bool isFilled = false;
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号椭圆
 */
struct SymbolEllipseIR {
    QPointF center;  ///< 中心点（已解析，单位 mm）
    double radiusX = 0.0;  ///< X 轴半径（mm）
    double radiusY = 0.0;  ///< Y 轴半径（mm）
    QColor strokeColor = Qt::black;
    double strokeWidth = 0.0;
    QColor fillColor = Qt::transparent;
    bool isFilled = false;
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号折线
 * @note 替代 SymbolPolyline::points 原始字符串，坐标已解析
 */
struct SymbolPolylineIR {
    QList<QPointF> points;  ///< 顶点列表（已解析，单位 mm）
    QColor strokeColor = Qt::black;
    double strokeWidth = 0.0;
    QColor fillColor = Qt::transparent;
    bool isFilled = false;
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号多边形
 * @note 替代 SymbolPolygon::points 原始字符串，坐标已解析
 */
struct SymbolPolygonIR {
    QList<QPointF> points;  ///< 顶点列表（已解析，单位 mm）
    QColor strokeColor = Qt::black;
    double strokeWidth = 0.0;
    QColor fillColor = Qt::transparent;
    bool isFilled = false;
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号路径
 * @note 替代 SymbolPath::paths SVG 路径字符串，坐标已解析为点序列
 */
struct SymbolPathIR {
    QList<QPointF> points;  ///< 路径坐标序列（已解析，单位 mm）
    QColor strokeColor = Qt::black;
    double strokeWidth = 0.0;
    QColor fillColor = Qt::transparent;
    bool isFilled = false;
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号文本
 * @note 颜色、字体大小已标准化
 */
struct SymbolTextIR {
    QString text;  ///< 文本内容
    QPointF position;  ///< 位置（已解析，单位 mm）
    double rotation = 0.0;  ///< 旋转角度（度）
    QColor color = Qt::black;  ///< 文本颜色
    QString fontFamily;  ///< 字体名称
    double fontSizeMm = 0.0;  ///< 字体大小（mm，已从 pt 转换）
    bool bold = false;  ///< 是否粗体
    bool italic = false;  ///< 是否斜体（替代原始字符串 "1"/"Italic"）
    bool visible = true;  ///< 是否可见
    int partIndex = 0;  ///< 所属部件索引（多部件符号使用）
};

/**
 * @brief 通用符号组件
 *
 * 包含一个符号的所有图形原语和引脚。
 * 不包含任何 EDA 平台特有的元数据字段。
 */
struct SymbolComponentIR {
    QString name;  ///< 组件名称
    QString description;  ///< 组件描述
    QString designatorPrefix;  ///< 位号前缀（"U", "R", "C" 等）
    int partCount = 1;  ///< 部件数（多部件符号）
    double originX = 0.0;  ///< 原点 X 坐标（mm）
    double originY = 0.0;  ///< 原点 Y 坐标（mm）

    // 图形原语列表
    QList<SymbolPinIR> pins;
    QList<SymbolRectangleIR> rectangles;
    QList<SymbolCircleIR> circles;
    QList<SymbolArcIR> arcs;
    QList<SymbolEllipseIR> ellipses;
    QList<SymbolPolylineIR> polylines;
    QList<SymbolPolygonIR> polygons;
    QList<SymbolPathIR> paths;
    QList<SymbolTextIR> texts;

    /** @brief 封装关联名称 */
    QString footprintName;

    /** @brief 来源平台特有元数据（如 LCSC ID、制造商等） */
    QMap<QString, QString> sourceMetadata;

    /** @brief 是否为多部件符号 */
    bool isMultiPart() const {
        return partCount > 1;
    }

    /** @brief 是否包含任何图形数据 */
    bool hasGraphics() const {
        return !pins.isEmpty() || !rectangles.isEmpty() || !circles.isEmpty() || !arcs.isEmpty() ||
               !ellipses.isEmpty() || !polylines.isEmpty() || !polygons.isEmpty() || !paths.isEmpty() ||
               !texts.isEmpty();
    }

    void clear() {
        name.clear();
        description.clear();
        designatorPrefix.clear();
        partCount = 1;
        originX = originY = 0.0;
        pins.clear();
        rectangles.clear();
        circles.clear();
        arcs.clear();
        ellipses.clear();
        polylines.clear();
        polygons.clear();
        paths.clear();
        texts.clear();
        footprintName.clear();
    }
};

}  // namespace IR
}  // namespace EasyKiConverter
