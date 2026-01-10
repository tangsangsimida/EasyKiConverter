#include "SymbolData.h"
#include <QDebug>

namespace EasyKiConverter {

// ==================== SymbolPinSettings ====================

QJsonObject SymbolPinSettings::toJson() const
{
    QJsonObject json;
    json["is_displayed"] = isDisplayed;
    json["type"] = static_cast<int>(type);
    json["spice_pin_number"] = spicePinNumber;
    json["pos_x"] = posX;
    json["pos_y"] = posY;
    json["rotation"] = rotation;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolPinSettings::fromJson(const QJsonObject &json)
{
    if (!json.contains("type") || !json.contains("spice_pin_number")) {
        return false;
    }
    isDisplayed = json["is_displayed"].toBool(true);
    type = static_cast<PinType>(json["type"].toInt(0));
    spicePinNumber = json["spice_pin_number"].toString();
    posX = json["pos_x"].toDouble(0.0);
    posY = json["pos_y"].toDouble(0.0);
    rotation = json["rotation"].toInt(0);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPinDot ====================

QJsonObject SymbolPinDot::toJson() const
{
    QJsonObject json;
    json["dot_x"] = dotX;
    json["dot_y"] = dotY;
    return json;
}

bool SymbolPinDot::fromJson(const QJsonObject &json)
{
    dotX = json["dot_x"].toDouble(0.0);
    dotY = json["dot_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinPath ====================

QJsonObject SymbolPinPath::toJson() const
{
    QJsonObject json;
    json["path"] = path;
    json["color"] = color;
    return json;
}

bool SymbolPinPath::fromJson(const QJsonObject &json)
{
    if (!json.contains("path")) {
        return false;
    }
    path = json["path"].toString();
    color = json["color"].toString();
    return true;
}

// ==================== SymbolPinName ====================

QJsonObject SymbolPinName::toJson() const
{
    QJsonObject json;
    json["is_displayed"] = isDisplayed;
    json["pos_x"] = posX;
    json["pos_y"] = posY;
    json["rotation"] = rotation;
    json["text"] = text;
    json["text_anchor"] = textAnchor;
    json["font"] = font;
    json["font_size"] = fontSize;
    return json;
}

bool SymbolPinName::fromJson(const QJsonObject &json)
{
    if (!json.contains("text")) {
        return false;
    }
    isDisplayed = json["is_displayed"].toBool(true);
    posX = json["pos_x"].toDouble(0.0);
    posY = json["pos_y"].toDouble(0.0);
    rotation = json["rotation"].toInt(0);
    text = json["text"].toString();
    textAnchor = json["text_anchor"].toString();
    font = json["font"].toString();
    fontSize = json["font_size"].toDouble(7.0);
    return true;
}

// ==================== SymbolPinDotBis ====================

QJsonObject SymbolPinDotBis::toJson() const
{
    QJsonObject json;
    json["is_displayed"] = isDisplayed;
    json["circle_x"] = circleX;
    json["circle_y"] = circleY;
    return json;
}

bool SymbolPinDotBis::fromJson(const QJsonObject &json)
{
    isDisplayed = json["is_displayed"].toBool(false);
    circleX = json["circle_x"].toDouble(0.0);
    circleY = json["circle_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinClock ====================

QJsonObject SymbolPinClock::toJson() const
{
    QJsonObject json;
    json["is_displayed"] = isDisplayed;
    json["path"] = path;
    return json;
}

bool SymbolPinClock::fromJson(const QJsonObject &json)
{
    isDisplayed = json["is_displayed"].toBool(false);
    path = json["path"].toString();
    return true;
}

// ==================== SymbolPin ====================

QJsonObject SymbolPin::toJson() const
{
    QJsonObject json;
    json["settings"] = settings.toJson();
    json["pin_dot"] = pinDot.toJson();
    json["pin_path"] = pinPath.toJson();
    json["name"] = name.toJson();
    json["dot"] = dot.toJson();
    json["clock"] = clock.toJson();
    return json;
}

bool SymbolPin::fromJson(const QJsonObject &json)
{
    if (!json.contains("settings") || !json.contains("name")) {
        return false;
    }
    if (!settings.fromJson(json["settings"].toObject())) {
        return false;
    }
    pinDot.fromJson(json["pin_dot"].toObject());
    pinPath.fromJson(json["pin_path"].toObject());
    name.fromJson(json["name"].toObject());
    dot.fromJson(json["dot"].toObject());
    clock.fromJson(json["clock"].toObject());
    return true;
}

// ==================== SymbolBBox ====================

QJsonObject SymbolBBox::toJson() const
{
    QJsonObject json;
    json["x"] = x;
    json["y"] = y;
    return json;
}

bool SymbolBBox::fromJson(const QJsonObject &json)
{
    x = json["x"].toDouble(0.0);
    y = json["y"].toDouble(0.0);
    return true;
}

// ==================== SymbolInfo ====================

QJsonObject SymbolInfo::toJson() const
{
    QJsonObject json;
    json["name"] = name;
    json["prefix"] = prefix;
    json["package"] = package;
    json["manufacturer"] = manufacturer;
    json["datasheet"] = datasheet;
    json["lcsc_id"] = lcscId;
    json["jlc_id"] = jlcId;
    return json;
}

bool SymbolInfo::fromJson(const QJsonObject &json)
{
    name = json["name"].toString();
    prefix = json["prefix"].toString();
    package = json["package"].toString();
    manufacturer = json["manufacturer"].toString();
    datasheet = json["datasheet"].toString();
    lcscId = json["lcsc_id"].toString();
    jlcId = json["jlc_id"].toString();
    return true;
}

// ==================== SymbolRectangle ====================

QJsonObject SymbolRectangle::toJson() const
{
    QJsonObject json;
    json["pos_x"] = posX;
    json["pos_y"] = posY;
    json["rx"] = rx;
    json["ry"] = ry;
    json["width"] = width;
    json["height"] = height;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolRectangle::fromJson(const QJsonObject &json)
{
    posX = json["pos_x"].toDouble(0.0);
    posY = json["pos_y"].toDouble(0.0);
    rx = json["rx"].toDouble(0.0);
    ry = json["ry"].toDouble(0.0);
    width = json["width"].toDouble(0.0);
    height = json["height"].toDouble(0.0);
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toString();
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolCircle ====================

QJsonObject SymbolCircle::toJson() const
{
    QJsonObject json;
    json["center_x"] = centerX;
    json["center_y"] = centerY;
    json["radius"] = radius;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolCircle::fromJson(const QJsonObject &json)
{
    centerX = json["center_x"].toDouble(0.0);
    centerY = json["center_y"].toDouble(0.0);
    radius = json["radius"].toDouble(0.0);
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toBool(false);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolArc ====================

QJsonObject SymbolArc::toJson() const
{
    QJsonObject json;
    QJsonArray pathArray;
    for (const QPointF &point : path) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pathArray.append(pointObj);
    }
    json["path"] = pathArray;
    json["helper_dots"] = helperDots;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolArc::fromJson(const QJsonObject &json)
{
    if (json.contains("path") && json["path"].isArray()) {
        QJsonArray pathArray = json["path"].toArray();
        path.clear();
        for (const QJsonValue &value : pathArray) {
            if (value.isObject()) {
                QJsonObject pointObj = value.toObject();
                QPointF point(pointObj["x"].toDouble(0.0), pointObj["y"].toDouble(0.0));
                path.append(point);
            }
        }
    }
    helperDots = json["helper_dots"].toString();
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toBool(false);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolEllipse ====================

QJsonObject SymbolEllipse::toJson() const
{
    QJsonObject json;
    json["center_x"] = centerX;
    json["center_y"] = centerY;
    json["radius_x"] = radiusX;
    json["radius_y"] = radiusY;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolEllipse::fromJson(const QJsonObject &json)
{
    centerX = json["center_x"].toDouble(0.0);
    centerY = json["center_y"].toDouble(0.0);
    radiusX = json["radius_x"].toDouble(0.0);
    radiusY = json["radius_y"].toDouble(0.0);
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toBool(false);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPolyline ====================

QJsonObject SymbolPolyline::toJson() const
{
    QJsonObject json;
    json["points"] = points;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolPolyline::fromJson(const QJsonObject &json)
{
    points = json["points"].toString();
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toBool(false);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPolygon ====================

QJsonObject SymbolPolygon::toJson() const
{
    QJsonObject json;
    json["points"] = points;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolPolygon::fromJson(const QJsonObject &json)
{
    points = json["points"].toString();
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toBool(false);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPath ====================

QJsonObject SymbolPath::toJson() const
{
    QJsonObject json;
    json["paths"] = paths;
    json["stroke_color"] = strokeColor;
    json["stroke_width"] = strokeWidth;
    json["stroke_style"] = strokeStyle;
    json["fill_color"] = fillColor;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool SymbolPath::fromJson(const QJsonObject &json)
{
    paths = json["paths"].toString();
    strokeColor = json["stroke_color"].toString();
    strokeWidth = json["stroke_width"].toDouble(0.0);
    strokeStyle = json["stroke_style"].toString();
    fillColor = json["fill_color"].toBool(false);
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolData ====================

SymbolData::SymbolData()
{
}

QJsonObject SymbolData::toJson() const
{
    QJsonObject json;
    json["info"] = m_info.toJson();
    json["bbox"] = m_bbox.toJson();

    QJsonArray pinsArray;
    for (const SymbolPin &pin : m_pins) {
        pinsArray.append(pin.toJson());
    }
    json["pins"] = pinsArray;

    QJsonArray rectanglesArray;
    for (const SymbolRectangle &rect : m_rectangles) {
        rectanglesArray.append(rect.toJson());
    }
    json["rectangles"] = rectanglesArray;

    QJsonArray circlesArray;
    for (const SymbolCircle &circle : m_circles) {
        circlesArray.append(circle.toJson());
    }
    json["circles"] = circlesArray;

    QJsonArray arcsArray;
    for (const SymbolArc &arc : m_arcs) {
        arcsArray.append(arc.toJson());
    }
    json["arcs"] = arcsArray;

    QJsonArray ellipsesArray;
    for (const SymbolEllipse &ellipse : m_ellipses) {
        ellipsesArray.append(ellipse.toJson());
    }
    json["ellipses"] = ellipsesArray;

    QJsonArray polylinesArray;
    for (const SymbolPolyline &polyline : m_polylines) {
        polylinesArray.append(polyline.toJson());
    }
    json["polylines"] = polylinesArray;

    QJsonArray polygonsArray;
    for (const SymbolPolygon &polygon : m_polygons) {
        polygonsArray.append(polygon.toJson());
    }
    json["polygons"] = polygonsArray;

    QJsonArray pathsArray;
    for (const SymbolPath &path : m_paths) {
        pathsArray.append(path.toJson());
    }
    json["paths"] = pathsArray;

    return json;
}

bool SymbolData::fromJson(const QJsonObject &json)
{
    // 读取信息
    if (json.contains("info") && json["info"].isObject()) {
        if (!m_info.fromJson(json["info"].toObject())) {
            qWarning() << "Failed to parse symbol info";
            return false;
        }
    }

    // 读取边界框
    if (json.contains("bbox") && json["bbox"].isObject()) {
        if (!m_bbox.fromJson(json["bbox"].toObject())) {
            qWarning() << "Failed to parse symbol bbox";
            return false;
        }
    }

    // 读取引脚
    if (json.contains("pins") && json["pins"].isArray()) {
        QJsonArray pinsArray = json["pins"].toArray();
        m_pins.clear();
        for (const QJsonValue &value : pinsArray) {
            if (value.isObject()) {
                SymbolPin pin;
                if (pin.fromJson(value.toObject())) {
                    m_pins.append(pin);
                }
            }
        }
    }

    // 读取矩形
    if (json.contains("rectangles") && json["rectangles"].isArray()) {
        QJsonArray rectanglesArray = json["rectangles"].toArray();
        m_rectangles.clear();
        for (const QJsonValue &value : rectanglesArray) {
            if (value.isObject()) {
                SymbolRectangle rect;
                if (rect.fromJson(value.toObject())) {
                    m_rectangles.append(rect);
                }
            }
        }
    }

    // 读取圆
    if (json.contains("circles") && json["circles"].isArray()) {
        QJsonArray circlesArray = json["circles"].toArray();
        m_circles.clear();
        for (const QJsonValue &value : circlesArray) {
            if (value.isObject()) {
                SymbolCircle circle;
                if (circle.fromJson(value.toObject())) {
                    m_circles.append(circle);
                }
            }
        }
    }

    // 读取圆弧
    if (json.contains("arcs") && json["arcs"].isArray()) {
        QJsonArray arcsArray = json["arcs"].toArray();
        m_arcs.clear();
        for (const QJsonValue &value : arcsArray) {
            if (value.isObject()) {
                SymbolArc arc;
                if (arc.fromJson(value.toObject())) {
                    m_arcs.append(arc);
                }
            }
        }
    }

    // 读取椭圆
    if (json.contains("ellipses") && json["ellipses"].isArray()) {
        QJsonArray ellipsesArray = json["ellipses"].toArray();
        m_ellipses.clear();
        for (const QJsonValue &value : ellipsesArray) {
            if (value.isObject()) {
                SymbolEllipse ellipse;
                if (ellipse.fromJson(value.toObject())) {
                    m_ellipses.append(ellipse);
                }
            }
        }
    }

    // 读取多段线
    if (json.contains("polylines") && json["polylines"].isArray()) {
        QJsonArray polylinesArray = json["polylines"].toArray();
        m_polylines.clear();
        for (const QJsonValue &value : polylinesArray) {
            if (value.isObject()) {
                SymbolPolyline polyline;
                if (polyline.fromJson(value.toObject())) {
                    m_polylines.append(polyline);
                }
            }
        }
    }

    // 读取多边形
    if (json.contains("polygons") && json["polygons"].isArray()) {
        QJsonArray polygonsArray = json["polygons"].toArray();
        m_polygons.clear();
        for (const QJsonValue &value : polygonsArray) {
            if (value.isObject()) {
                SymbolPolygon polygon;
                if (polygon.fromJson(value.toObject())) {
                    m_polygons.append(polygon);
                }
            }
        }
    }

    // 读取路径
    if (json.contains("paths") && json["paths"].isArray()) {
        QJsonArray pathsArray = json["paths"].toArray();
        m_paths.clear();
        for (const QJsonValue &value : pathsArray) {
            if (value.isObject()) {
                SymbolPath path;
                if (path.fromJson(value.toObject())) {
                    m_paths.append(path);
                }
            }
        }
    }

    return true;
}

bool SymbolData::isValid() const
{
    // 检查基本字段
    if (m_bbox.x == 0.0 && m_bbox.y == 0.0) {
        return false;
    }

    return true;
}

QString SymbolData::validate() const
{
    // 验证符号信息
    if (m_info.name.isEmpty()) {
        return "Symbol name is empty";
    }

    // 验证边界框
    if (m_bbox.x == 0.0 && m_bbox.y == 0.0) {
        return "Symbol bbox is empty";
    }

    // 验证引脚
    for (int i = 0; i < m_pins.size(); ++i) {
        const SymbolPin &pin = m_pins[i];
        if (pin.settings.spicePinNumber.isEmpty()) {
            return QString("Pin %1 has empty number").arg(i);
        }
    }

    return QString(); // 空字符串表示验证通过
}

void SymbolData::clear()
{
    m_info = SymbolInfo();
    m_bbox = SymbolBBox();
    m_pins.clear();
    m_rectangles.clear();
    m_circles.clear();
    m_arcs.clear();
    m_ellipses.clear();
    m_polylines.clear();
    m_polygons.clear();
    m_paths.clear();
}

} // namespace EasyKiConverter