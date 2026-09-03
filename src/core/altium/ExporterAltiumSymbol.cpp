#include "ExporterAltiumSymbol.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

#include <QDebug>
#include <QSet>

namespace EasyKiConverter {

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
    altiumPin.showName = pin.showName;
    altiumPin.showDesignator = pin.showDesignator;
    altiumPin.isHidden = !pin.showName && !pin.showDesignator;
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
    altiumArc.ownerPartId = qMax(1, arc.partIndex + 1);
    return altiumArc;
}

/**
 * @brief SymbolPolygonIR → AltiumSchPolygon
 */
AltiumSchPolygon ExporterAltiumSymbol::convertPolygon(const IR::SymbolPolygonIR& polygon) {
    AltiumSchPolygon altiumPolygon;
    altiumPolygon.lineWidth = AltiumCoord::lineWidthMmToIndex(polygon.strokeWidth);
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
    altiumEllipse.isSolid = ellipse.isFilled;
    altiumEllipse.ownerPartId = qMax(1, ellipse.partIndex + 1);
    return altiumEllipse;
}

/**
 * @brief 将符号坐标归一化，以第一个引脚为原点
 * @details 用第一个引脚的位置作为偏移量，确保至少有一个引脚在原点（网格交叉点），
 *          方便用户在 Altium Designer 中连线。
 */
void ExporterAltiumSymbol::centerComponent(AltiumSchComponent& component) {
    if (component.pins.isEmpty())
        return;

    // 以第一个引脚位置为原点
    int offsetX = component.pins[0].locationX;
    int offsetY = component.pins[0].locationY;
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
}

}  // namespace EasyKiConverter
