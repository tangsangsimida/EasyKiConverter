#include "ExporterAltiumSymbol.h"

#include "utils/AltiumCoord.h"
#include "utils/AltiumLayerMap.h"

namespace EasyKiConverter {

/**
 * @brief 导出单个符号
 */
bool ExporterAltiumSymbol::exportSymbol(const SymbolData& symbolData,
                                         const QString& filePath) {
    QList<AltiumSchComponent> components;
    components.append(convertSymbol(symbolData));
    return m_writer.write(components, filePath);
}

/**
 * @brief 导出符号库
 */
bool ExporterAltiumSymbol::exportSymbolLibrary(const QList<SymbolData>& symbols,
                                                const QString& libName,
                                                const QString& filePath,
                                                bool appendMode,
                                                bool updateMode,
                                                const QString& libraryDescription) {
    QList<AltiumSchComponent> components;
    for (const SymbolData& symbol : symbols) {
        components.append(convertSymbol(symbol));
    }
    return m_writer.write(components, filePath, libName);
}

/**
 * @brief 解析 SVG 路径点字符串为 QPointF 列表
 * @details 点字符串格式：空格分隔的 "x,y" 对，如 "0,0 100,200 300,400"
 */
static QList<QPointF> parsePointsString(const QString& pointsStr) {
    QList<QPointF> result;
    QStringList pairs = pointsStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (const QString& pair : pairs) {
        QStringList coords = pair.split(',');
        if (coords.size() >= 2) {
            bool ok1, ok2;
            double x = coords[0].toDouble(&ok1);
            double y = coords[1].toDouble(&ok2);
            if (ok1 && ok2) {
                result.append(QPointF(x, y));
            }
        }
    }
    return result;
}

/**
 * @brief 解析 SVG 路径命令为 QPointF 列表
 * @details 简化处理：提取路径中的所有坐标点
 */
static QList<QPointF> parsePathString(const QString& pathStr) {
    QList<QPointF> result;
    // 简化：提取所有数字对
    QString cleaned = pathStr;
    // 将命令字母替换为空格
    for (const QChar& c : QString("MmLlHhVvCcSsQqTtAaZz")) {
        cleaned.replace(c, ' ');
    }
    // 将逗号替换为空格
    cleaned.replace(',', ' ');
    QStringList nums = cleaned.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < nums.size(); i += 2) {
        bool ok1, ok2;
        double x = nums[i].toDouble(&ok1);
        double y = nums[i + 1].toDouble(&ok2);
        if (ok1 && ok2) {
            result.append(QPointF(x, y));
        }
    }
    return result;
}

/**
 * @brief SymbolData → AltiumSchComponent
 */
AltiumSchComponent ExporterAltiumSymbol::convertSymbol(const SymbolData& data) {
    AltiumSchComponent component;
    component.name = data.info().name;
    component.description = data.info().description;
    component.designatorPrefix = data.info().prefix;
    component.partCount = data.parts().size();

    // 转换引脚
    const auto& pins = data.isMultiPart() ? data.parts().first().pins : data.pins();
    for (const SymbolPin& pin : pins) {
        component.pins.append(convertPin(pin));
    }

    // 转换图形元素
    if (data.isMultiPart()) {
        const SymbolPart& part = data.parts().first();
        for (const SymbolRectangle& r : part.rectangles) component.rectangles.append(convertRectangle(r));
        for (const SymbolCircle& c : part.circles) component.ellipses.append(convertCircle(c));
        for (const SymbolArc& a : part.arcs) component.arcs.append(convertArc(a));
        for (const SymbolPolygon& p : part.polygons) component.polygons.append(convertPolygon(p));
        for (const SymbolPolyline& p : part.polylines) component.polylines.append(convertPolyline(p));
        for (const SymbolPath& p : part.paths) component.paths.append(convertPath(p));
        for (const SymbolText& t : part.texts) component.texts.append(convertText(t));
    } else {
        for (const SymbolRectangle& r : data.rectangles()) component.rectangles.append(convertRectangle(r));
        for (const SymbolCircle& c : data.circles()) component.ellipses.append(convertCircle(c));
        for (const SymbolArc& a : data.arcs()) component.arcs.append(convertArc(a));
        for (const SymbolPolygon& p : data.polygons()) component.polygons.append(convertPolygon(p));
        for (const SymbolPolyline& p : data.polylines()) component.polylines.append(convertPolyline(p));
        for (const SymbolPath& p : data.paths()) component.paths.append(convertPath(p));
        for (const SymbolText& t : data.texts()) component.texts.append(convertText(t));
        for (const SymbolEllipse& e : data.ellipses()) component.ellipses.append(convertEllipse(e));
    }

    // 添加封装链接
    if (!data.info().package.isEmpty()) {
        AltiumSchComponent::Implementation impl;
        impl.modelName = data.info().package;
        impl.modelType = "PCBLIB";
        component.implementations.append(impl);
    }

    return component;
}

/**
 * @brief SymbolPin → AltiumSchPin
 */
AltiumSchPin ExporterAltiumSymbol::convertPin(const SymbolPin& pin) {
    AltiumSchPin altiumPin;
    altiumPin.name = pin.name.text;
    altiumPin.designator = pin.settings.id;
    altiumPin.locationX = static_cast<int>(pin.settings.posX);
    altiumPin.locationY = static_cast<int>(pin.settings.posY);
    // SymbolPinSettings 没有 length 字段，使用默认引脚长度
    altiumPin.length = 100000;  // 默认 10mil
    altiumPin.electricalType = static_cast<AltiumModels::PinElectricalType>(
        AltiumLayerMap::toAltiumElectricalType(static_cast<int>(pin.settings.type)));
    altiumPin.orientation = static_cast<AltiumModels::PinOrientation>(
        AltiumLayerMap::toAltiumPinOrientation(pin.settings.rotation));
    altiumPin.showName = pin.settings.isDisplayed;
    altiumPin.showDesignator = pin.settings.isDisplayed;
    altiumPin.isHidden = !pin.settings.isDisplayed;
    return altiumPin;
}

/**
 * @brief SymbolRectangle → AltiumSchRectangle
 * @details SymbolRectangle 使用 posX/posY 作为左上角，rx/ry 为圆角半径
 */
AltiumSchRectangle ExporterAltiumSymbol::convertRectangle(const SymbolRectangle& rect) {
    AltiumSchRectangle altiumRect;
    altiumRect.locationX = static_cast<int>(rect.posX);
    altiumRect.locationY = static_cast<int>(rect.posY);
    altiumRect.cornerX = static_cast<int>(rect.posX + rect.width);
    altiumRect.cornerY = static_cast<int>(rect.posY + rect.height);
    altiumRect.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(rect.strokeWidth));
    altiumRect.isSolid = true;
    return altiumRect;
}

/**
 * @brief SymbolCircle → AltiumSchEllipse
 */
AltiumSchEllipse ExporterAltiumSymbol::convertCircle(const SymbolCircle& circle) {
    AltiumSchEllipse altiumEllipse;
    altiumEllipse.centerX = static_cast<int>(circle.centerX);
    altiumEllipse.centerY = static_cast<int>(circle.centerY);
    altiumEllipse.radiusX = static_cast<int>(circle.radius);
    altiumEllipse.radiusY = static_cast<int>(circle.radius);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(circle.strokeWidth));
    altiumEllipse.isSolid = circle.fillColor;
    return altiumEllipse;
}

/**
 * @brief SymbolArc → AltiumSchArc
 * @details SymbolArc 的 path 是 QPointF 列表，从中提取圆心和半径
 */
AltiumSchArc ExporterAltiumSymbol::convertArc(const SymbolArc& arc) {
    AltiumSchArc altiumArc;
    if (arc.path.size() >= 3) {
        // path[0] = 起点, path[1] = 终点, path[2] = 圆心（或根据 EasyEDA 格式）
        // 简化处理：使用前三个点推导
        QPointF start = arc.path.first();
        QPointF end = arc.path.size() > 1 ? arc.path.at(1) : arc.path.first();
        QPointF center = arc.path.size() > 2 ? arc.path.at(2) : QPointF(0, 0);
        altiumArc.centerX = static_cast<int>(center.x());
        altiumArc.centerY = static_cast<int>(center.y());
        double dx = start.x() - center.x();
        double dy = start.y() - center.y();
        altiumArc.radius = static_cast<int>(qSqrt(dx * dx + dy * dy));
    }
    altiumArc.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(arc.strokeWidth));
    return altiumArc;
}

/**
 * @brief SymbolPolygon → AltiumSchPolygon
 * @details SymbolPolygon.points 是空格分隔的 "x,y" 字符串
 */
AltiumSchPolygon ExporterAltiumSymbol::convertPolygon(const SymbolPolygon& polygon) {
    AltiumSchPolygon altiumPolygon;
    altiumPolygon.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(polygon.strokeWidth));
    altiumPolygon.isSolid = polygon.fillColor;

    QList<QPointF> points = parsePointsString(polygon.points);
    for (const QPointF& point : points) {
        altiumPolygon.vertices.append(QPointF(
            AltiumCoord::toSchematicUnits(static_cast<int>(point.x())),
            AltiumCoord::toSchematicUnits(static_cast<int>(point.y()))
        ));
    }
    return altiumPolygon;
}

/**
 * @brief SymbolPolyline → AltiumSchPolyline
 */
AltiumSchPolyline ExporterAltiumSymbol::convertPolyline(const SymbolPolyline& polyline) {
    AltiumSchPolyline altiumPolyline;
    altiumPolyline.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(polyline.strokeWidth));

    QList<QPointF> points = parsePointsString(polyline.points);
    for (const QPointF& point : points) {
        altiumPolyline.vertices.append(QPointF(
            AltiumCoord::toSchematicUnits(static_cast<int>(point.x())),
            AltiumCoord::toSchematicUnits(static_cast<int>(point.y()))
        ));
    }
    return altiumPolyline;
}

/**
 * @brief SymbolPath → AltiumSchPath
 * @details SymbolPath.paths 是 SVG 路径命令字符串
 */
AltiumSchPath ExporterAltiumSymbol::convertPath(const SymbolPath& path) {
    AltiumSchPath altiumPath;
    altiumPath.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(path.strokeWidth));

    QList<QPointF> points = parsePathString(path.paths);
    for (const QPointF& point : points) {
        altiumPath.vertices.append(QPointF(
            AltiumCoord::toSchematicUnits(static_cast<int>(point.x())),
            AltiumCoord::toSchematicUnits(static_cast<int>(point.y()))
        ));
    }
    return altiumPath;
}

/**
 * @brief SymbolText → AltiumSchText
 */
AltiumSchText ExporterAltiumSymbol::convertText(const SymbolText& text) {
    AltiumSchText altiumText;
    altiumText.locationX = static_cast<int>(text.posX);
    altiumText.locationY = static_cast<int>(text.posY);
    altiumText.text = text.text;
    altiumText.fontId = 1;
    altiumText.isHidden = !text.visible;
    return altiumText;
}

/**
 * @brief SymbolEllipse → AltiumSchEllipse
 */
AltiumSchEllipse ExporterAltiumSymbol::convertEllipse(const SymbolEllipse& ellipse) {
    AltiumSchEllipse altiumEllipse;
    altiumEllipse.centerX = static_cast<int>(ellipse.centerX);
    altiumEllipse.centerY = static_cast<int>(ellipse.centerY);
    altiumEllipse.radiusX = static_cast<int>(ellipse.radiusX);
    altiumEllipse.radiusY = static_cast<int>(ellipse.radiusY);
    altiumEllipse.lineWidth = AltiumCoord::lineWidthToIndex(static_cast<int>(ellipse.strokeWidth));
    altiumEllipse.isSolid = ellipse.fillColor;
    return altiumEllipse;
}

}  // namespace EasyKiConverter
