#pragma once

#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"

namespace EasyKiConverter {

/**
 * @brief 将 IR 曲线图元转换为 Altium SchLib 记录。
 * @details 转换器不保存状态，仅负责坐标、线型、颜色和部件归属映射。
 */
class AltiumSymbolCurveConverter final {
public:
    /** @brief 转换三点圆弧。 */
    static AltiumSchArc convertArc(const IR::SymbolArcIR& arc);

    /** @brief 转换多边形。 */
    static AltiumSchPolygon convertPolygon(const IR::SymbolPolygonIR& polygon);

    /** @brief 转换折线。 */
    static AltiumSchPolyline convertPolyline(const IR::SymbolPolylineIR& polyline);

    /** @brief 转换路径。 */
    static AltiumSchPath convertPath(const IR::SymbolPathIR& path);

    /** @brief 转换 Bezier 曲线。 */
    static AltiumSchBezier convertBezier(const IR::SymbolBezierIR& bezier);

    /** @brief 转换椭圆。 */
    static AltiumSchEllipse convertEllipse(const IR::SymbolEllipseIR& ellipse);

    /** @brief 转换扇形。 */
    static AltiumSchPie convertPie(const IR::SymbolPieIR& pie);

    /** @brief 转换椭圆弧。 */
    static AltiumSchEllipticalArc convertEllipticalArc(const IR::SymbolEllipticalArcIR& arc);
};

}  // namespace EasyKiConverter
