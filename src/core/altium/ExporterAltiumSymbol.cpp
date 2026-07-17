#include "ExporterAltiumSymbol.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

namespace EasyKiConverter {

/**
 * @brief 导出单个符号
 */
bool ExporterAltiumSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    QList<AltiumSchComponent> components;
    components.append(convertSymbol(symbol));
    return m_writer.write(components, filePath);
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
    return m_writer.write(components, filePath, libName);
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
    return altiumPin;
}

/**
 * @brief SymbolRectangleIR → AltiumSchRectangle
 */
AltiumSchRectangle ExporterAltiumSymbol::convertRectangle(const IR::SymbolRectangleIR& rect) {
    AltiumSchRectangle altiumRect;
    altiumRect.locationX = AltiumCoord::mmToRaw(rect.bounds.left());
    altiumRect.locationY = AltiumCoord::mmToRaw(rect.bounds.top());
    altiumRect.cornerX = AltiumCoord::mmToRaw(rect.bounds.right());
    altiumRect.cornerY = AltiumCoord::mmToRaw(rect.bounds.bottom());
    altiumRect.lineWidth = AltiumCoord::lineWidthMmToIndex(rect.strokeWidth);
    altiumRect.isSolid = true;
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
    return altiumEllipse;
}

/**
 * @brief SymbolArcIR → AltiumSchArc
 */
AltiumSchArc ExporterAltiumSymbol::convertArc(const IR::SymbolArcIR& arc) {
    AltiumSchArc altiumArc;
    altiumArc.centerX = AltiumCoord::mmToRaw(arc.center.x());
    altiumArc.centerY = AltiumCoord::mmToRaw(arc.center.y());
    altiumArc.radius = AltiumCoord::mmToRaw(arc.radius);
    altiumArc.startAngle = arc.startAngle;
    altiumArc.endAngle = arc.endAngle;
    altiumArc.lineWidth = AltiumCoord::lineWidthMmToIndex(arc.strokeWidth);
    return altiumArc;
}

/**
 * @brief SymbolPolygonIR → AltiumSchPolygon
 */
AltiumSchPolygon ExporterAltiumSymbol::convertPolygon(const IR::SymbolPolygonIR& polygon) {
    AltiumSchPolygon altiumPolygon;
    altiumPolygon.lineWidth = AltiumCoord::lineWidthMmToIndex(polygon.strokeWidth);
    altiumPolygon.isSolid = polygon.isFilled;

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
    return altiumEllipse;
}

}  // namespace EasyKiConverter
