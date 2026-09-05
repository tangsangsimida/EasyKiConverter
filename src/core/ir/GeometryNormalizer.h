#pragma once

/**
 * @file GeometryNormalizer.h
 * @brief 坐标解析和几何归一化工具
 *
 * 从 SymbolDataConverter.h 中提取的纯几何工具函数。
 * 负责：
 * - EasyEDA 坐标字符串解析（空格分隔、逗号分隔、SVG 路径）
 * - 单位转换（px -> mm, pt -> mm）
 * - Y 轴翻转
 * - 原点偏移
 *
 * 不涉及任何类型映射或 IR 构建逻辑。
 */

#include "IRTypes.h"

#include <QPointF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 坐标解析和几何归一化工具集
 */
namespace GeometryNormalizer {

// ==================== 坐标字符串解析 ====================

/**
 * @brief 解析 EasyEDA 空格分隔的扁平坐标字符串
 * @param pointsStr "x1 y1 x2 y2 x3 y3" 格式
 * @param scaleFactor 缩放因子（默认 PX_TO_MM = 0.254）
 * @return 解析后的点列表
 */
inline QList<QPointF> parseFlatPointString(const QString& pointsStr, double scaleFactor = EASYEDA_PX_TO_MM) {
    QList<QPointF> points;
    const QStringList parts = pointsStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); i += 2) {
        bool ok1 = false, ok2 = false;
        const double x = parts[i].toDouble(&ok1);
        const double y = parts[i + 1].toDouble(&ok2);
        if (ok1 && ok2) {
            points.append(QPointF(x * scaleFactor, y * scaleFactor));
        }
    }
    return points;
}

/**
 * @brief 解析 EasyEDA 逗号分隔的坐标字符串
 * @param pointsStr "x1,y1 x2,y2 x3,y3" 格式
 * @param scaleFactor 缩放因子
 * @return 解析后的点列表
 */
inline QList<QPointF> parseCommaSeparatedPoints(const QString& pointsStr, double scaleFactor = EASYEDA_PX_TO_MM) {
    QList<QPointF> points;
    const QStringList pairs = pointsStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (const auto& pair : pairs) {
        const QStringList xy = pair.split(',');
        if (xy.size() >= 2) {
            bool ok1 = false, ok2 = false;
            const double x = xy[0].toDouble(&ok1);
            const double y = xy[1].toDouble(&ok2);
            if (ok1 && ok2) {
                points.append(QPointF(x * scaleFactor, y * scaleFactor));
            }
        }
    }
    return points;
}

/**
 * @brief 解析 SVG 路径字符串为点序列
 *
 * 简化解析：提取 M/L 命令后的坐标对，忽略 C/Z 等复杂命令。
 * 适用于 EasyEDA 的简单 SVG 路径。
 *
 * @param pathStr SVG 路径字符串（如 "M 0 0 L 100 0 L 100 100 Z"）
 * @param scaleFactor 缩放因子
 * @return 解析后的点列表
 */
inline QList<QPointF> parseSimpleSvgPath(const QString& pathStr, double scaleFactor = EASYEDA_PX_TO_MM) {
    QList<QPointF> points;
    QString cleaned = pathStr;
    // 将命令字母替换为空格，只保留数字和分隔符
    for (const QChar& ch : QString("MmLlHhVvCcSsQqTtAaZz")) {
        cleaned.replace(ch, ' ');
    }
    const QStringList parts = cleaned.trimmed().split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); i += 2) {
        bool ok1 = false, ok2 = false;
        const double x = parts[i].toDouble(&ok1);
        const double y = parts[i + 1].toDouble(&ok2);
        if (ok1 && ok2) {
            points.append(QPointF(x * scaleFactor, y * scaleFactor));
        }
    }
    return points;
}

// ==================== 坐标变换 ====================

/**
 * @brief 对单点应用原点偏移和 Y 轴翻转
 *
 * EasyEDA 坐标系 Y 轴向下，KiCad/Altium Y 轴向上。
 * 转换公式：result = (rawX - originX) * scale, -(rawY - originY) * scale
 *
 * @param rawX 原始 X 坐标（EasyEDA 单位）
 * @param rawY 原始 Y 坐标（EasyEDA 单位）
 * @param originX 原点 X（EasyEDA 单位）
 * @param originY 原点 Y（EasyEDA 单位）
 * @param scaleFactor 缩放因子
 * @return 变换后的点（mm）
 */
inline QPointF transformPoint(double rawX, double rawY,
                              double originX, double originY,
                              double scaleFactor = EASYEDA_PX_TO_MM) {
    return QPointF((rawX - originX) * scaleFactor, -(rawY - originY) * scaleFactor);
}

/**
 * @brief 对点列表应用原点偏移和 Y 轴翻转
 *
 * 注意：输入点必须是已缩放的（mm 单位），原点也需先缩放。
 * 用于 polylines/polygons/paths 等已解析的点序列。
 *
 * @param points 已缩放的点列表（mm）
 * @param originMm 原点（mm）
 * @return 变换后的点列表
 */
inline QList<QPointF> transformPoints(const QList<QPointF>& points, const QPointF& originMm) {
    QList<QPointF> result;
    result.reserve(points.size());
    for (const auto& pt : points) {
        result.append(QPointF(pt.x() - originMm.x(), -(pt.y() - originMm.y())));
    }
    return result;
}

/**
 * @brief 计算一组图元的几何边界框原点
 *
 * 遍历所有图形原语，找到最小的 (minX, minY) 作为局部原点。
 * 用于多部件符号的每个 part 独立计算原点。
 *
 * @return 边界框左上角 (minX, minY)，若无图元则返回 (0, 0)
 */
struct BBoxOrigin {
    double x = 0.0;
    double y = 0.0;
    bool valid = false;
};

}  // namespace GeometryNormalizer
}  // namespace IR
}  // namespace EasyKiConverter
