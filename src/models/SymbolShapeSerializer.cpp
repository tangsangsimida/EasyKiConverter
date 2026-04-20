#include "SymbolShapeSerializer.h"

#include <QJsonArray>

namespace EasyKiConverter {

// ==================== SymbolBBox ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolBBox& bbox) {
    QJsonObject json;
    json["x"] = bbox.x;
    json["y"] = bbox.y;
    json["width"] = bbox.width;
    json["height"] = bbox.height;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    bbox.x = json["x"].toDouble(0.0);
    bbox.y = json["y"].toDouble(0.0);
    bbox.width = json["width"].toDouble(0.0);
    bbox.height = json["height"].toDouble(0.0);
    return true;
}

// ==================== SymbolRectangle ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolRectangle& rect) {
    QJsonObject json;
    json["pos_x"] = rect.posX;
    json["pos_y"] = rect.posY;
    json["rx"] = rect.rx;
    json["ry"] = rect.ry;
    json["width"] = rect.width;
    json["height"] = rect.height;
    json["stroke_color"] = rect.strokeColor;
    json["stroke_width"] = rect.strokeWidth;
    json["stroke_style"] = rect.strokeStyle;
    json["fill_color"] = rect.fillColor;
    json["id"] = rect.id;
    json["is_locked"] = rect.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolRectangle& rect, const QJsonObject& json) {
    rect.posX = json["pos_x"].toDouble(0.0);
    rect.posY = json["pos_y"].toDouble(0.0);
    rect.rx = json["rx"].toDouble(0.0);
    rect.ry = json["ry"].toDouble(0.0);
    rect.width = json["width"].toDouble(0.0);
    rect.height = json["height"].toDouble(0.0);
    rect.strokeColor = json["stroke_color"].toString();
    rect.strokeWidth = json["stroke_width"].toDouble(0.0);
    rect.strokeStyle = json["stroke_style"].toString();
    rect.fillColor = json["fill_color"].toString();
    rect.id = json["id"].toString();
    rect.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolCircle ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolCircle& circle) {
    QJsonObject json;
    json["center_x"] = circle.centerX;
    json["center_y"] = circle.centerY;
    json["radius"] = circle.radius;
    json["stroke_color"] = circle.strokeColor;
    json["stroke_width"] = circle.strokeWidth;
    json["stroke_style"] = circle.strokeStyle;
    json["fill_color"] = circle.fillColor;
    json["id"] = circle.id;
    json["is_locked"] = circle.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolCircle& circle, const QJsonObject& json) {
    circle.centerX = json["center_x"].toDouble(0.0);
    circle.centerY = json["center_y"].toDouble(0.0);
    circle.radius = json["radius"].toDouble(0.0);
    circle.strokeColor = json["stroke_color"].toString();
    circle.strokeWidth = json["stroke_width"].toDouble(0.0);
    circle.strokeStyle = json["stroke_style"].toString();
    circle.fillColor = json["fill_color"].toBool(false);
    circle.id = json["id"].toString();
    circle.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolArc ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolArc& arc) {
    QJsonObject json;
    QJsonArray pathArray;
    for (const QPointF& point : arc.path) {
        QJsonObject pointObj;
        pointObj["x"] = point.x();
        pointObj["y"] = point.y();
        pathArray.append(pointObj);
    }
    json["path"] = pathArray;
    json["helper_dots"] = arc.helperDots;
    json["stroke_color"] = arc.strokeColor;
    json["stroke_width"] = arc.strokeWidth;
    json["stroke_style"] = arc.strokeStyle;
    json["fill_color"] = arc.fillColor;
    json["id"] = arc.id;
    json["is_locked"] = arc.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolArc& arc, const QJsonObject& json) {
    if (json.contains("path") && json["path"].isArray()) {
        QJsonArray pathArray = json["path"].toArray();
        arc.path.clear();
        for (const QJsonValue& value : pathArray) {
            if (value.isObject()) {
                QJsonObject pointObj = value.toObject();
                QPointF point(pointObj["x"].toDouble(0.0), pointObj["y"].toDouble(0.0));
                arc.path.append(point);
            }
        }
    }
    arc.helperDots = json["helper_dots"].toString();
    arc.strokeColor = json["stroke_color"].toString();
    arc.strokeWidth = json["stroke_width"].toDouble(0.0);
    arc.strokeStyle = json["stroke_style"].toString();
    arc.fillColor = json["fill_color"].toBool(false);
    arc.id = json["id"].toString();
    arc.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolEllipse ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolEllipse& ellipse) {
    QJsonObject json;
    json["center_x"] = ellipse.centerX;
    json["center_y"] = ellipse.centerY;
    json["radius_x"] = ellipse.radiusX;
    json["radius_y"] = ellipse.radiusY;
    json["stroke_color"] = ellipse.strokeColor;
    json["stroke_width"] = ellipse.strokeWidth;
    json["stroke_style"] = ellipse.strokeStyle;
    json["fill_color"] = ellipse.fillColor;
    json["id"] = ellipse.id;
    json["is_locked"] = ellipse.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
    ellipse.centerX = json["center_x"].toDouble(0.0);
    ellipse.centerY = json["center_y"].toDouble(0.0);
    ellipse.radiusX = json["radius_x"].toDouble(0.0);
    ellipse.radiusY = json["radius_y"].toDouble(0.0);
    ellipse.strokeColor = json["stroke_color"].toString();
    ellipse.strokeWidth = json["stroke_width"].toDouble(0.0);
    ellipse.strokeStyle = json["stroke_style"].toString();
    ellipse.fillColor = json["fill_color"].toBool(false);
    ellipse.id = json["id"].toString();
    ellipse.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPolyline ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolPolyline& polyline) {
    QJsonObject json;
    json["points"] = polyline.points;
    json["stroke_color"] = polyline.strokeColor;
    json["stroke_width"] = polyline.strokeWidth;
    json["stroke_style"] = polyline.strokeStyle;
    json["fill_color"] = polyline.fillColor;
    json["id"] = polyline.id;
    json["is_locked"] = polyline.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
    polyline.points = json["points"].toString();
    polyline.strokeColor = json["stroke_color"].toString();
    polyline.strokeWidth = json["stroke_width"].toDouble(0.0);
    polyline.strokeStyle = json["stroke_style"].toString();
    polyline.fillColor = json["fill_color"].toBool(false);
    polyline.id = json["id"].toString();
    polyline.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPolygon ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolPolygon& polygon) {
    QJsonObject json;
    json["points"] = polygon.points;
    json["stroke_color"] = polygon.strokeColor;
    json["stroke_width"] = polygon.strokeWidth;
    json["stroke_style"] = polygon.strokeStyle;
    json["fill_color"] = polygon.fillColor;
    json["id"] = polygon.id;
    json["is_locked"] = polygon.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
    polygon.points = json["points"].toString();
    polygon.strokeColor = json["stroke_color"].toString();
    polygon.strokeWidth = json["stroke_width"].toDouble(0.0);
    polygon.strokeStyle = json["stroke_style"].toString();
    polygon.fillColor = json["fill_color"].toBool(false);
    polygon.id = json["id"].toString();
    polygon.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPath ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolPath& path) {
    QJsonObject json;
    json["paths"] = path.paths;
    json["stroke_color"] = path.strokeColor;
    json["stroke_width"] = path.strokeWidth;
    json["stroke_style"] = path.strokeStyle;
    json["fill_color"] = path.fillColor;
    json["id"] = path.id;
    json["is_locked"] = path.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolPath& path, const QJsonObject& json) {
    path.paths = json["paths"].toString();
    path.strokeColor = json["stroke_color"].toString();
    path.strokeWidth = json["stroke_width"].toDouble(0.0);
    path.strokeStyle = json["stroke_style"].toString();
    path.fillColor = json["fill_color"].toBool(false);
    path.id = json["id"].toString();
    path.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolText ====================

QJsonObject SymbolShapeSerializer::toJson(const SymbolText& text) {
    QJsonObject json;
    json["mark"] = text.mark;
    json["pos_x"] = text.posX;
    json["pos_y"] = text.posY;
    json["rotation"] = text.rotation;
    json["color"] = text.color;
    json["font"] = text.font;
    json["text_size"] = text.textSize;
    json["bold"] = text.bold;
    json["italic"] = text.italic;
    json["baseline"] = text.baseline;
    json["type"] = text.type;
    json["text"] = text.text;
    json["visible"] = text.visible;
    json["anchor"] = text.anchor;
    json["id"] = text.id;
    json["is_locked"] = text.isLocked;
    return json;
}

bool SymbolShapeSerializer::fromJson(SymbolText& text, const QJsonObject& json) {
    text.mark = json["mark"].toString();
    text.posX = json["pos_x"].toDouble(0.0);
    text.posY = json["pos_y"].toDouble(0.0);
    text.rotation = json["rotation"].toInt(0);
    text.color = json["color"].toString();
    text.font = json["font"].toString();
    text.textSize = json["text_size"].toDouble(10.0);
    text.bold = json["bold"].toBool(false);
    text.italic = json["italic"].toString();
    text.baseline = json["baseline"].toString();
    text.type = json["type"].toString();
    text.text = json["text"].toString();
    text.visible = json["visible"].toBool(true);
    text.anchor = json["anchor"].toString();
    text.id = json["id"].toString();
    text.isLocked = json["is_locked"].toBool(false);
    return true;
}

}  // namespace EasyKiConverter