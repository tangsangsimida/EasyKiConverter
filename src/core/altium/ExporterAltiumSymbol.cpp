#include "ExporterAltiumSymbol.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

#include <QDebug>
#include <QSet>

#include <climits>

namespace EasyKiConverter {

namespace {

/**
 * @brief 将 Qt 颜色转换为 Altium 使用的 0x00BBGGRR 编码。
 * @details IR 统一使用 RGB，而 SchLib 参数保存的是 BGR 数值。此前转换器
 *          没有复制颜色字段，导致所有图元退化为 Altium 默认黑色。
 */
uint32_t toAltiumColor(const QColor& color) {
    // Altium 原理图库的默认符号色是深蓝色；黑色是 IR 的“未指定”默认值，
    // 若直接写入 0，SchLib 写入器会省略 Color 参数并由 AD 使用不可预测的默认值。
    constexpr uint32_t kDefaultSchematicColor = 0x68380B;
    if (!color.isValid() || color.alpha() == 0 || color == QColor(Qt::black))
        return kDefaultSchematicColor;
    return (static_cast<uint32_t>(color.blue()) << 16) | (static_cast<uint32_t>(color.green()) << 8) |
           static_cast<uint32_t>(color.red());
}

/**
 * @brief 将原始坐标量化到 Altium SchLib 的最小转换网格
 * @details SchLib 多边形使用 raw / 1000 的 Schematic Unit，而引脚和基本图元
 *          使用 raw。平移量必须落在 1000 raw 网格上，避免两套坐标写出后产生
 *          小于 0.1 mil 的相对偏移。
 */
int quantizeSchematicOffset(int value) {
    constexpr int kSchematicUnitRaw = 1000;
    if (value >= 0)
        return ((value + kSchematicUnitRaw / 2) / kSchematicUnitRaw) * kSchematicUnitRaw;
    return ((value - kSchematicUnitRaw / 2) / kSchematicUnitRaw) * kSchematicUnitRaw;
}

}  // namespace

/**
 * @brief 导出单个符号
 */
bool ExporterAltiumSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    QList<AltiumSchComponent> components;
    components.append(convertSymbol(symbol));
    bool ok = m_writer.write(components, filePath);
    if (!ok) {
        qWarning() << "ExporterAltiumSymbol: Failed to write symbol to" << filePath;
    }
    return ok;
}

/**
 * @brief 导出符号库
 */
bool ExporterAltiumSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                               const QString& libName,
                                               const QString& filePath,
                                               bool appendMode,
                                               bool updateMode,
                                               const QString& libraryDescription) {
    QList<AltiumSchComponent> components;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        components.append(convertSymbol(symbol));
    }
    bool ok = m_writer.write(components, filePath, libName);
    if (!ok) {
        qWarning() << "ExporterAltiumSymbol: Failed to write symbol library to" << filePath;
    }
    return ok;
}

/**
 * @brief SymbolComponentIR → AltiumSchComponent
 */
AltiumSchComponent ExporterAltiumSymbol::convertSymbol(const IR::SymbolComponentIR& data) {
    AltiumSchComponent component;
    component.name = data.name;
    component.description = data.description;
    component.designatorPrefix = data.designatorPrefix;
    component.partCount = data.partCount;

    // 转换引脚
    for (const IR::SymbolPinIR& pin : data.pins) {
        component.pins.append(convertPin(pin));
    }

    // 转换图形元素
    for (const IR::SymbolRectangleIR& r : data.rectangles)
        component.rectangles.append(convertRectangle(r));
    for (const IR::SymbolCircleIR& c : data.circles)
        component.ellipses.append(convertCircle(c));
    for (const IR::SymbolArcIR& a : data.arcs)
        component.arcs.append(convertArc(a));
    for (const IR::SymbolPolygonIR& p : data.polygons)
        component.polygons.append(convertPolygon(p));
    for (const IR::SymbolPolylineIR& p : data.polylines)
        component.polylines.append(convertPolyline(p));
    for (const IR::SymbolPathIR& p : data.paths)
        component.paths.append(convertPath(p));
    for (const IR::SymbolTextIR& t : data.texts)
        component.texts.append(convertText(t));
    for (const IR::SymbolEllipseIR& e : data.ellipses)
        component.ellipses.append(convertEllipse(e));

    // 添加封装链接
    if (!data.footprintName.isEmpty()) {
        AltiumSchComponent::Implementation impl;
        impl.modelName = data.footprintName;
        impl.modelType = "PCBLIB";
        component.implementations.append(impl);
    }

    // 坐标归一化：将符号中心移到原点
    centerComponent(component);

    return component;
}

/**
 * @brief SymbolPinIR → AltiumSchPin
 */
AltiumSchPin ExporterAltiumSymbol::convertPin(const IR::SymbolPinIR& pin) {
    AltiumSchPin altiumPin;
    altiumPin.name = pin.name;
    altiumPin.designator = pin.designator;
    altiumPin.locationX = AltiumCoord::mmToRaw(pin.position.x());
    altiumPin.locationY = AltiumCoord::mmToRaw(pin.position.y());
    altiumPin.length = pin.length > 0.0 ? AltiumCoord::mmToRaw(pin.length) : 100000;
    altiumPin.electricalType =
        static_cast<AltiumModels::PinElectricalType>(AltiumLayerMap::toAltiumElectricalType(pin.electricalType));
    altiumPin.orientation =
        static_cast<AltiumModels::PinOrientation>(AltiumLayerMap::toAltiumPinOrientation(pin.direction));
    // EasyEDA 的 pin name 显示标志在部分库中未设置，但名称字符串本身
    // 仍是符号的一部分。Altium 的 PinConglomerate 必须显式打开 show-name，
    // 否则 AD 只绘制 Pin Number，名称会表现为脱离引脚的独立文本。
    // 优先从 display 控制层获取，兼容旧 showName 字段。
    altiumPin.showName = pin.display.showName || pin.showName || !pin.name.trimmed().isEmpty();
    altiumPin.showDesignator = pin.display.showDesignator || pin.showDesignator;
    altiumPin.isHidden = !altiumPin.showName && !altiumPin.showDesignator;
    altiumPin.color = toAltiumColor(QColor(Qt::black));
    // EasyEDA 的反相圆点位于引脚外侧，时钟标记贴近主体内侧。
    // 优先从 style 语义层获取，兼容旧 hasDot/hasClock 字段。
    if (pin.style.inverted || pin.hasDot)
        altiumPin.symbolOuterEdge = 1;  // Dot
    if (pin.style.clock || pin.hasClock)
        altiumPin.symbolInnerEdge = 3;  // Clock

    // 处理更丰富的 PinDecoration 枚举
    switch (pin.style.decoration) {
        case IR::PinDecoration::OpenCollector:
            altiumPin.symbolOuterEdge = 8;
            break;
        case IR::PinDecoration::OpenEmitter:
            altiumPin.symbolOuterEdge = 15;
            break;
        case IR::PinDecoration::HiZ:
            altiumPin.symbolOuterEdge = 12;
            break;
        case IR::PinDecoration::Pulse:
            altiumPin.symbolOuterEdge = 14;
            break;
        case IR::PinDecoration::Postponed:
            altiumPin.symbolOuterEdge = 11;
            break;
        default:
            break;
    }

    // 这些电气类型同时具有明确的 Altium IEEE 装饰
    switch (pin.electricalType) {
        case IR::PinElectricalType::OpenCollector:
            altiumPin.symbolOuterEdge = 8;
            break;
        case IR::PinElectricalType::OpenEmitter:
            altiumPin.symbolOuterEdge = 15;
            break;
        default:
            break;
    }
    altiumPin.ownerPartId = qMax(1, pin.partIndex + 1);

    // 电源引脚检测：EasyEDA 通常不区分电源引脚（type=3/Bidirectional），
    // 通过引脚名称匹配常见电源网络名称，强制设为 Power 类型
    if (altiumPin.electricalType != AltiumModels::PinElectricalType::Power) {
        static const QSet<QString> powerPinNames = {
            "GND",      "AGND",    "DGND",     "PGND",      "SGND",  "CGND", "GNDP", "GNDN", "VCC",
            "VDD",      "AVCC",    "AVDD",     "DVDD",      "IOVDD", "PVDD", "SVDD", "VDDA", "VDDIO",
            "VDDS",     "VDDP",    "VBUS",     "VSYS",      "VIN",   "5V",   "3V3",  "1V8",  "USB_VDD",
            "ADC_AVDD", "VREG_IN", "VREG_OUT", "VREG_VOUT", "VEE",   "VSS",  "VSSA",
        };
        QString upperName = pin.name.toUpper().trimmed();
        if (powerPinNames.contains(upperName)) {
            altiumPin.electricalType = AltiumModels::PinElectricalType::Power;
        }
    }

    return altiumPin;
}

/**
 * @brief SymbolRectangleIR → AltiumSchRectangle
 */
AltiumSchRectangle ExporterAltiumSymbol::convertRectangle(const IR::SymbolRectangleIR& rect) {
    AltiumSchRectangle altiumRect;
    altiumRect.locationX = AltiumCoord::mmToRaw(rect.x0);
    altiumRect.locationY = AltiumCoord::mmToRaw(rect.y0);
    altiumRect.cornerX = AltiumCoord::mmToRaw(rect.x1);
    altiumRect.cornerY = AltiumCoord::mmToRaw(rect.y1);
    altiumRect.lineWidth = AltiumCoord::lineWidthMmToIndex(rect.strokeWidth);
    altiumRect.color = toAltiumColor(rect.strokeColor);
    altiumRect.areaColor = rect.isFilled ? toAltiumColor(rect.fillColor) : 0xFFFFFF;
    altiumRect.isSolid = rect.isFilled;
    altiumRect.ownerPartId = qMax(1, rect.partIndex + 1);
    return altiumRect;
}

/**
 * @brief SymbolCircleIR → AltiumSchEllipse
 */
AltiumSchEllipse ExporterAltiumSymbol::convertCircle(const IR::SymbolCircleIR& circle) {
    AltiumSchEllipse altiumEllipse;
    altiumEllipse.centerX = AltiumCoord::mmToRaw(circle.center.x());
    altiumEllipse.centerY = AltiumCoord::mmToRaw(circle.center.y());
    altiumEllipse.radiusX = AltiumCoord::mmToRaw(circle.radius);
    altiumEllipse.radiusY = AltiumCoord::mmToRaw(circle.radius);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthMmToIndex(circle.strokeWidth);
    altiumEllipse.color = toAltiumColor(circle.strokeColor);
    altiumEllipse.areaColor = circle.isFilled ? toAltiumColor(circle.fillColor) : 0xFFFFFF;
    altiumEllipse.isSolid = circle.isFilled;
    altiumEllipse.ownerPartId = qMax(1, circle.partIndex + 1);
    return altiumEllipse;
}

/**
 * @brief SymbolArcIR → AltiumSchArc
 */
AltiumSchArc ExporterAltiumSymbol::convertArc(const IR::SymbolArcIR& arc) {
    AltiumSchArc altiumArc;
    // 三点确定圆弧。退化为共线时，以首尾中点作为安全回退。
    const double ax = arc.startPoint.x();
    const double ay = arc.startPoint.y();
    const double bx = arc.midPoint.x();
    const double by = arc.midPoint.y();
    const double cx = arc.endPoint.x();
    const double cy = arc.endPoint.y();
    const double determinant = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    QPointF center = (arc.startPoint + arc.endPoint) / 2.0;
    if (std::abs(determinant) > 1e-12) {
        const double a2 = ax * ax + ay * ay;
        const double b2 = bx * bx + by * by;
        const double c2 = cx * cx + cy * cy;
        center.setX((a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / determinant);
        center.setY((a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / determinant);
    }
    double dx = arc.startPoint.x() - center.x();
    double dy = arc.startPoint.y() - center.y();
    double radius = std::sqrt(dx * dx + dy * dy);
    altiumArc.centerX = AltiumCoord::mmToRaw(center.x());
    altiumArc.centerY = AltiumCoord::mmToRaw(center.y());
    altiumArc.radius = AltiumCoord::mmToRaw(radius);
    altiumArc.startAngle = std::atan2(arc.startPoint.y() - center.y(), arc.startPoint.x() - center.x()) * 180.0 / M_PI;
    altiumArc.endAngle = std::atan2(arc.endPoint.y() - center.y(), arc.endPoint.x() - center.x()) * 180.0 / M_PI;
    altiumArc.lineWidth = AltiumCoord::lineWidthMmToIndex(arc.strokeWidth);
    altiumArc.color = toAltiumColor(arc.strokeColor);
    altiumArc.ownerPartId = qMax(1, arc.partIndex + 1);
    return altiumArc;
}

/**
 * @brief SymbolPolygonIR → AltiumSchPolygon
 */
AltiumSchPolygon ExporterAltiumSymbol::convertPolygon(const IR::SymbolPolygonIR& polygon) {
    AltiumSchPolygon altiumPolygon;
    altiumPolygon.lineWidth = AltiumCoord::lineWidthMmToIndex(polygon.strokeWidth);
    altiumPolygon.color = toAltiumColor(polygon.strokeColor);
    altiumPolygon.areaColor = polygon.isFilled ? toAltiumColor(polygon.fillColor) : 0xFFFFFF;
    altiumPolygon.isSolid = polygon.isFilled;
    altiumPolygon.ownerPartId = qMax(1, polygon.partIndex + 1);

    for (const QPointF& point : polygon.points) {
        altiumPolygon.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumPolygon;
}

/**
 * @brief SymbolPolylineIR → AltiumSchPolyline
 */
AltiumSchPolyline ExporterAltiumSymbol::convertPolyline(const IR::SymbolPolylineIR& polyline) {
    AltiumSchPolyline altiumPolyline;
    altiumPolyline.lineWidth = AltiumCoord::lineWidthMmToIndex(polyline.strokeWidth);
    altiumPolyline.color = toAltiumColor(polyline.strokeColor);
    altiumPolyline.ownerPartId = qMax(1, polyline.partIndex + 1);

    for (const QPointF& point : polyline.points) {
        altiumPolyline.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumPolyline;
}

/**
 * @brief SymbolPathIR → AltiumSchPath
 */
AltiumSchPath ExporterAltiumSymbol::convertPath(const IR::SymbolPathIR& path) {
    AltiumSchPath altiumPath;
    altiumPath.lineWidth = AltiumCoord::lineWidthMmToIndex(path.strokeWidth);
    altiumPath.color = toAltiumColor(path.strokeColor);
    altiumPath.ownerPartId = qMax(1, path.partIndex + 1);

    for (const QPointF& point : path.points) {
        altiumPath.vertices.append(
            QPointF(AltiumCoord::mmToSchematicUnits(point.x()), AltiumCoord::mmToSchematicUnits(point.y())));
    }
    return altiumPath;
}

/**
 * @brief SymbolTextIR → AltiumSchText
 */
AltiumSchText ExporterAltiumSymbol::convertText(const IR::SymbolTextIR& text) {
    AltiumSchText altiumText;
    altiumText.locationX = AltiumCoord::mmToRaw(text.position.x());
    altiumText.locationY = AltiumCoord::mmToRaw(text.position.y());
    altiumText.text = text.text;
    altiumText.fontId = 1;
    altiumText.color = toAltiumColor(text.color);
    altiumText.isHidden = !text.visible;
    altiumText.orientation = static_cast<int>(text.rotation / 90.0) % 4;
    altiumText.ownerPartId = qMax(1, text.partIndex + 1);
    return altiumText;
}

/**
 * @brief SymbolEllipseIR → AltiumSchEllipse
 */
AltiumSchEllipse ExporterAltiumSymbol::convertEllipse(const IR::SymbolEllipseIR& ellipse) {
    AltiumSchEllipse altiumEllipse;
    altiumEllipse.centerX = AltiumCoord::mmToRaw(ellipse.center.x());
    altiumEllipse.centerY = AltiumCoord::mmToRaw(ellipse.center.y());
    altiumEllipse.radiusX = AltiumCoord::mmToRaw(ellipse.radiusX);
    altiumEllipse.radiusY = AltiumCoord::mmToRaw(ellipse.radiusY);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthMmToIndex(ellipse.strokeWidth);
    altiumEllipse.color = toAltiumColor(ellipse.strokeColor);
    altiumEllipse.areaColor = ellipse.isFilled ? toAltiumColor(ellipse.fillColor) : 0xFFFFFF;
    altiumEllipse.isSolid = ellipse.isFilled;
    altiumEllipse.ownerPartId = qMax(1, ellipse.partIndex + 1);
    return altiumEllipse;
}

/**
 * @brief 将符号坐标归一化，以第一个引脚为原点
 * @details 使用符号图形包围盒中心作为统一原点，保证主体、引脚和文本在
 *          Altium Designer 中保持相对位置。
 */
void ExporterAltiumSymbol::centerComponent(AltiumSchComponent& component) {
    if (component.pins.isEmpty())
        return;

    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    auto include = [&](int x, int y) {
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
    };
    for (const auto& pin : component.pins)
        include(pin.locationX, pin.locationY);
    for (const auto& rect : component.rectangles) {
        include(rect.locationX, rect.locationY);
        include(rect.cornerX, rect.cornerY);
    }
    for (const auto& arc : component.arcs) {
        include(arc.centerX - arc.radius, arc.centerY - arc.radius);
        include(arc.centerX + arc.radius, arc.centerY + arc.radius);
    }
    for (const auto& ellipse : component.ellipses) {
        include(ellipse.centerX - ellipse.radiusX, ellipse.centerY - ellipse.radiusY);
        include(ellipse.centerX + ellipse.radiusX, ellipse.centerY + ellipse.radiusY);
    }
    if (minX == INT_MAX)
        return;

    // 量化平移量而不是逐个量化引脚，保持所有相对间距不变。
    int offsetX = quantizeSchematicOffset((minX + maxX) / 2);
    int offsetY = quantizeSchematicOffset((minY + maxY) / 2);
    // Altium 符号的可编辑原点相对 EasyEDA 几何中心有一个 10 mil 的
    // Y 基准偏置。将偏置加入统一平移量，使主体上下边界和四侧引脚
    // 在同一坐标基准上（例如 C2040 的 -180..200 mil）。
    constexpr int kAltiumSymbolOriginYBias = 1000000;
    offsetY -= kAltiumSymbolOriginYBias;
    int offsetPolyX = offsetX / 1000;
    int offsetPolyY = offsetY / 1000;

    // 平移引脚
    for (auto& pin : component.pins) {
        pin.locationX -= offsetX;
        pin.locationY -= offsetY;
    }
    // 平移矩形
    for (auto& rect : component.rectangles) {
        rect.locationX -= offsetX;
        rect.locationY -= offsetY;
        rect.cornerX -= offsetX;
        rect.cornerY -= offsetY;
    }
    // 平移弧线
    for (auto& arc : component.arcs) {
        arc.centerX -= offsetX;
        arc.centerY -= offsetY;
    }
    // 平移椭圆
    for (auto& ellipse : component.ellipses) {
        ellipse.centerX -= offsetX;
        ellipse.centerY -= offsetY;
    }
    // 平移多边形（mmToSchematicUnits 坐标系）
    for (auto& poly : component.polygons) {
        for (QPointF& v : poly.vertices) {
            v = QPointF(v.x() - offsetPolyX, v.y() - offsetPolyY);
        }
    }
    for (auto& polyline : component.polylines) {
        for (QPointF& v : polyline.vertices) {
            v = QPointF(v.x() - offsetPolyX, v.y() - offsetPolyY);
        }
    }
    for (auto& path : component.paths) {
        for (QPointF& v : path.vertices) {
            v = QPointF(v.x() - offsetPolyX, v.y() - offsetPolyY);
        }
    }
    // 平移文本
    for (auto& text : component.texts) {
        text.locationX -= offsetX;
        text.locationY -= offsetY;
    }

    // 统一引脚与主体的法向锚点。Altium 的 Pin Name/Number 都以二进制
    // 引脚位置为基准；如果引脚位置落在主体边界外，名称就会看起来与图形
    // 脱离。仅修正朝向法向坐标，沿边方向坐标保持不变，因此不会改变引脚
    // 间距。没有矩形主体的符号不强制投影，避免破坏原始几何。
    if (!component.rectangles.isEmpty()) {
        int bodyMinX = INT_MAX, bodyMinY = INT_MAX;
        int bodyMaxX = INT_MIN, bodyMaxY = INT_MIN;
        for (const auto& rect : component.rectangles) {
            bodyMinX = qMin(bodyMinX, qMin(rect.locationX, rect.cornerX));
            bodyMinY = qMin(bodyMinY, qMin(rect.locationY, rect.cornerY));
            bodyMaxX = qMax(bodyMaxX, qMax(rect.locationX, rect.cornerX));
            bodyMaxY = qMax(bodyMaxY, qMax(rect.locationY, rect.cornerY));
        }
        for (auto& pin : component.pins) {
            switch (pin.orientation) {
                case AltiumModels::PinOrientation::Right:
                    pin.locationX = bodyMaxX;
                    break;
                case AltiumModels::PinOrientation::Left:
                    pin.locationX = bodyMinX;
                    break;
                case AltiumModels::PinOrientation::Up:
                    pin.locationY = bodyMaxY;
                    break;
                case AltiumModels::PinOrientation::Down:
                    pin.locationY = bodyMinY;
                    break;
            }
        }

        // EasyEDA 的 Value/Comment 字段有时共享同一个锚点，并携带 270°
        // 的画布旋转值。直接写入 Altium 后，两个字段会被放到符号外部或
        // 变成不可见的竖排文本。仅当多个可见字段确实重合时，按 Altium
        // 符号字段习惯在主体中心水平排列；独立文本仍保留其原始几何。
        QList<int> visibleTextIndices;
        for (int i = 0; i < component.texts.size(); ++i) {
            if (!component.texts[i].isHidden)
                visibleTextIndices.append(i);
        }
        if (visibleTextIndices.size() > 1) {
            const auto& first = component.texts[visibleTextIndices.first()];
            bool coincident = true;
            for (int index : visibleTextIndices) {
                const auto& text = component.texts[index];
                if (qAbs(text.locationX - first.locationX) > 10000 ||
                    qAbs(text.locationY - first.locationY) > 10000) {
                    coincident = false;
                    break;
                }
            }
            if (coincident) {
                const int centerX = (bodyMinX + bodyMaxX) / 2;
                const int centerY = (bodyMinY + bodyMaxY) / 2;
                constexpr int kFieldStart = -300000;  // 主体中心 + (-3 mil) = 7 mil
                constexpr int kFieldSpacing = 2000000;  // 20 mil
                for (int order = 0; order < visibleTextIndices.size(); ++order) {
                    auto& text = component.texts[visibleTextIndices[order]];
                    text.locationX = centerX;
                    text.locationY = centerY + kFieldStart + order * kFieldSpacing;
                    text.orientation = 0;
                }
            }
        }
    }
}

}  // namespace EasyKiConverter
