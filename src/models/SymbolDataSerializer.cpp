#include "SymbolDataSerializer.h"

#include <QDebug>
#include <QJsonArray>


namespace EasyKiConverter {

// ==================== SymbolPinSettings ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPinSettings& settings) {
    QJsonObject json;
    json["is_displayed"] = settings.isDisplayed;
    json["type"] = static_cast<int>(settings.type);
    json["spice_pin_number"] = settings.spicePinNumber;
    json["pos_x"] = settings.posX;
    json["pos_y"] = settings.posY;
    json["rotation"] = settings.rotation;
    json["id"] = settings.id;
    json["is_locked"] = settings.isLocked;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPinSettings& settings, const QJsonObject& json) {
    if (!json.contains("type") || !json.contains("spice_pin_number")) {
        return false;
    }
    settings.isDisplayed = json["is_displayed"].toBool(true);
    settings.type = static_cast<PinType>(json["type"].toInt(0));
    settings.spicePinNumber = json["spice_pin_number"].toString();
    settings.posX = json["pos_x"].toDouble(0.0);
    settings.posY = json["pos_y"].toDouble(0.0);
    settings.rotation = json["rotation"].toInt(0);
    settings.id = json["id"].toString();
    settings.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPinDot ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPinDot& dot) {
    QJsonObject json;
    json["dot_x"] = dot.dotX;
    json["dot_y"] = dot.dotY;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPinDot& dot, const QJsonObject& json) {
    dot.dotX = json["dot_x"].toDouble(0.0);
    dot.dotY = json["dot_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinPath ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPinPath& path) {
    QJsonObject json;
    json["path"] = path.path;
    json["color"] = path.color;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPinPath& path, const QJsonObject& json) {
    if (!json.contains("path")) {
        return false;
    }
    path.path = json["path"].toString();
    path.color = json["color"].toString();
    return true;
}

// ==================== SymbolPinName ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPinName& name) {
    QJsonObject json;
    json["is_displayed"] = name.isDisplayed;
    json["pos_x"] = name.posX;
    json["pos_y"] = name.posY;
    json["rotation"] = name.rotation;
    json["text"] = name.text;
    json["text_anchor"] = name.textAnchor;
    json["font"] = name.font;
    json["font_size"] = name.fontSize;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPinName& name, const QJsonObject& json) {
    if (!json.contains("text")) {
        return false;
    }
    name.isDisplayed = json["is_displayed"].toBool(true);
    name.posX = json["pos_x"].toDouble(0.0);
    name.posY = json["pos_y"].toDouble(0.0);
    name.rotation = json["rotation"].toInt(0);
    name.text = json["text"].toString();
    name.textAnchor = json["text_anchor"].toString();
    name.font = json["font"].toString();
    name.fontSize = json["font_size"].toDouble(7.0);
    return true;
}

// ==================== SymbolPinDotBis ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPinDotBis& dot) {
    QJsonObject json;
    json["is_displayed"] = dot.isDisplayed;
    json["circle_x"] = dot.circleX;
    json["circle_y"] = dot.circleY;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPinDotBis& dot, const QJsonObject& json) {
    dot.isDisplayed = json["is_displayed"].toBool(false);
    dot.circleX = json["circle_x"].toDouble(0.0);
    dot.circleY = json["circle_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinClock ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPinClock& clock) {
    QJsonObject json;
    json["is_displayed"] = clock.isDisplayed;
    json["path"] = clock.path;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPinClock& clock, const QJsonObject& json) {
    clock.isDisplayed = json["is_displayed"].toBool(false);
    clock.path = json["path"].toString();
    return true;
}

// ==================== SymbolPin ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPin& pin) {
    QJsonObject json;
    json["settings"] = toJson(pin.settings);
    json["pin_dot"] = toJson(pin.pinDot);
    json["pin_path"] = toJson(pin.pinPath);
    json["name"] = toJson(pin.name);
    json["dot"] = toJson(pin.dot);
    json["clock"] = toJson(pin.clock);
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPin& pin, const QJsonObject& json) {
    if (!json.contains("settings") || !json.contains("name")) {
        return false;
    }
    if (!fromJson(pin.settings, json["settings"].toObject())) {
        return false;
    }
    fromJson(pin.pinDot, json["pin_dot"].toObject());
    fromJson(pin.pinPath, json["pin_path"].toObject());
    fromJson(pin.name, json["name"].toObject());
    fromJson(pin.dot, json["dot"].toObject());
    fromJson(pin.clock, json["clock"].toObject());
    return true;
}

// ==================== SymbolBBox ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolBBox& bbox) {
    QJsonObject json;
    json["x"] = bbox.x;
    json["y"] = bbox.y;
    json["width"] = bbox.width;
    json["height"] = bbox.height;
    return json;
}

bool SymbolDataSerializer::fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    bbox.x = json["x"].toDouble(0.0);
    bbox.y = json["y"].toDouble(0.0);
    bbox.width = json["width"].toDouble(0.0);
    bbox.height = json["height"].toDouble(0.0);
    return true;
}

// ==================== SymbolInfo ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolInfo& info) {
    QJsonObject json;
    json["name"] = info.name;
    json["prefix"] = info.prefix;
    json["package"] = info.package;
    json["manufacturer"] = info.manufacturer;
    json["description"] = info.description;
    json["datasheet"] = info.datasheet;
    json["lcsc_id"] = info.lcscId;
    json["jlc_id"] = info.jlcId;

    // EasyEDA API 原始字段
    json["uuid"] = info.uuid;
    json["title"] = info.title;
    json["doc_type"] = info.docType;
    json["type"] = info.type;
    json["thumb"] = info.thumb;
    json["datastrid"] = info.datastrid;
    json["jlc_on_sale"] = info.jlcOnSale;
    json["writable"] = info.writable;
    json["is_favorite"] = info.isFavorite;
    json["verify"] = info.verify;
    json["smt"] = info.smt;

    // 时间
    json["update_time"] = info.updateTime;
    json["updated_at"] = info.updatedAt;

    // 编辑器信
    json["editor_version"] = info.editorVersion;

    // 项目信息
    json["puuid"] = info.puuid;
    json["utime"] = info.utime;
    json["import_flag"] = info.importFlag;
    json["has_id_flag"] = info.hasIdFlag;

    // 附加参数
    json["time_stamp"] = info.timeStamp;
    json["subpart_no"] = info.subpartNo;
    json["supplier_part"] = info.supplierPart;
    json["supplier"] = info.supplier;
    json["manufacturer_part"] = info.manufacturerPart;
    json["jlcpcb_part_class"] = info.jlcpcbPartClass;

    return json;
}

bool SymbolDataSerializer::fromJson(SymbolInfo& info, const QJsonObject& json) {
    info.name = json["name"].toString();
    info.prefix = json["prefix"].toString();
    info.package = json["package"].toString();
    info.manufacturer = json["manufacturer"].toString();
    info.description = json["description"].toString();
    info.datasheet = json["datasheet"].toString();
    info.lcscId = json["lcsc_id"].toString();
    info.jlcId = json["jlc_id"].toString();

    // EasyEDA API 原始字段
    info.uuid = json["uuid"].toString();
    info.title = json["title"].toString();
    info.docType = json["doc_type"].toString();
    info.type = json["type"].toString();
    info.thumb = json["thumb"].toString();
    info.datastrid = json["datastrid"].toString();
    info.jlcOnSale = json["jlc_on_sale"].toBool(false);
    info.writable = json["writable"].toBool(false);
    info.isFavorite = json["is_favorite"].toBool(false);
    info.verify = json["verify"].toBool(false);
    info.smt = json["smt"].toBool(false);

    // 时间
    info.updateTime = json["update_time"].toVariant().toLongLong();
    info.updatedAt = json["updated_at"].toString();

    // 编辑器信
    info.editorVersion = json["editor_version"].toString();

    // 项目信息
    info.puuid = json["puuid"].toString();
    info.utime = json["utime"].toVariant().toLongLong();
    info.importFlag = json["import_flag"].toBool(false);
    info.hasIdFlag = json["has_id_flag"].toBool(false);

    // 附加参数
    info.timeStamp = json["time_stamp"].toString();
    info.subpartNo = json["subpart_no"].toString();
    info.supplierPart = json["supplier_part"].toString();
    info.supplier = json["supplier"].toString();
    info.manufacturerPart = json["manufacturer_part"].toString();
    info.jlcpcbPartClass = json["jlcpcb_part_class"].toString();

    return true;
}

// ==================== SymbolRectangle ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolRectangle& rect) {
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

bool SymbolDataSerializer::fromJson(SymbolRectangle& rect, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolCircle& circle) {
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

bool SymbolDataSerializer::fromJson(SymbolCircle& circle, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolArc& arc) {
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

bool SymbolDataSerializer::fromJson(SymbolArc& arc, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolEllipse& ellipse) {
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

bool SymbolDataSerializer::fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolPolyline& polyline) {
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

bool SymbolDataSerializer::fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolPolygon& polygon) {
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

bool SymbolDataSerializer::fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolPath& path) {
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

bool SymbolDataSerializer::fromJson(SymbolPath& path, const QJsonObject& json) {
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

QJsonObject SymbolDataSerializer::toJson(const SymbolText& text) {
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

bool SymbolDataSerializer::fromJson(SymbolText& text, const QJsonObject& json) {
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

// ==================== SymbolPart ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolPart& part) {
    QJsonObject json;
    json["unit_number"] = part.unitNumber;
    json["origin_x"] = part.originX;
    json["origin_y"] = part.originY;

    QJsonArray pinsArray;
    for (const SymbolPin& pin : part.pins) {
        pinsArray.append(toJson(pin));
    }
    json["pins"] = pinsArray;

    QJsonArray rectanglesArray;
    for (const SymbolRectangle& rect : part.rectangles) {
        rectanglesArray.append(toJson(rect));
    }
    json["rectangles"] = rectanglesArray;

    QJsonArray circlesArray;
    for (const SymbolCircle& circle : part.circles) {
        circlesArray.append(toJson(circle));
    }
    json["circles"] = circlesArray;

    QJsonArray arcsArray;
    for (const SymbolArc& arc : part.arcs) {
        arcsArray.append(toJson(arc));
    }
    json["arcs"] = arcsArray;

    QJsonArray ellipsesArray;
    for (const SymbolEllipse& ellipse : part.ellipses) {
        ellipsesArray.append(toJson(ellipse));
    }
    json["ellipses"] = ellipsesArray;

    QJsonArray polylinesArray;
    for (const SymbolPolyline& polyline : part.polylines) {
        polylinesArray.append(toJson(polyline));
    }
    json["polylines"] = polylinesArray;

    QJsonArray polygonsArray;
    for (const SymbolPolygon& polygon : part.polygons) {
        polygonsArray.append(toJson(polygon));
    }
    json["polygons"] = polygonsArray;

    QJsonArray pathsArray;
    for (const SymbolPath& path : part.paths) {
        pathsArray.append(toJson(path));
    }
    json["paths"] = pathsArray;

    QJsonArray textsArray;
    for (const SymbolText& text : part.texts) {
        textsArray.append(toJson(text));
    }
    json["texts"] = textsArray;

    return json;
}

bool SymbolDataSerializer::fromJson(SymbolPart& part, const QJsonObject& json) {
    part.unitNumber = json["unit_number"].toInt(0);
    part.originX = json["origin_x"].toDouble(0.0);
    part.originY = json["origin_y"].toDouble(0.0);

    if (json.contains("pins")) {
        QJsonArray pinsArray = json["pins"].toArray();
        part.pins.clear();
        for (const QJsonValue& value : pinsArray) {
            SymbolPin pin;
            if (fromJson(pin, value.toObject())) {
                part.pins.append(pin);
            }
        }
    }

    if (json.contains("rectangles")) {
        QJsonArray rectanglesArray = json["rectangles"].toArray();
        part.rectangles.clear();
        for (const QJsonValue& value : rectanglesArray) {
            SymbolRectangle rect;
            if (fromJson(rect, value.toObject())) {
                part.rectangles.append(rect);
            }
        }
    }

    if (json.contains("circles")) {
        QJsonArray circlesArray = json["circles"].toArray();
        part.circles.clear();
        for (const QJsonValue& value : circlesArray) {
            SymbolCircle circle;
            if (fromJson(circle, value.toObject())) {
                part.circles.append(circle);
            }
        }
    }

    if (json.contains("arcs")) {
        QJsonArray arcsArray = json["arcs"].toArray();
        part.arcs.clear();
        for (const QJsonValue& value : arcsArray) {
            SymbolArc arc;
            if (fromJson(arc, value.toObject())) {
                part.arcs.append(arc);
            }
        }
    }

    if (json.contains("ellipses")) {
        QJsonArray ellipsesArray = json["ellipses"].toArray();
        part.ellipses.clear();
        for (const QJsonValue& value : ellipsesArray) {
            SymbolEllipse ellipse;
            if (fromJson(ellipse, value.toObject())) {
                part.ellipses.append(ellipse);
            }
        }
    }

    if (json.contains("polylines")) {
        QJsonArray polylinesArray = json["polylines"].toArray();
        part.polylines.clear();
        for (const QJsonValue& value : polylinesArray) {
            SymbolPolyline polyline;
            if (fromJson(polyline, value.toObject())) {
                part.polylines.append(polyline);
            }
        }
    }

    if (json.contains("polygons")) {
        QJsonArray polygonsArray = json["polygons"].toArray();
        part.polygons.clear();
        for (const QJsonValue& value : polygonsArray) {
            SymbolPolygon polygon;
            if (fromJson(polygon, value.toObject())) {
                part.polygons.append(polygon);
            }
        }
    }

    if (json.contains("paths")) {
        QJsonArray pathsArray = json["paths"].toArray();
        part.paths.clear();
        for (const QJsonValue& value : pathsArray) {
            SymbolPath path;
            if (fromJson(path, value.toObject())) {
                part.paths.append(path);
            }
        }
    }

    if (json.contains("texts")) {
        QJsonArray textsArray = json["texts"].toArray();
        part.texts.clear();
        for (const QJsonValue& value : textsArray) {
            SymbolText text;
            if (fromJson(text, value.toObject())) {
                part.texts.append(text);
            }
        }
    }

    return true;
}

// ==================== SymbolData ====================

QJsonObject SymbolDataSerializer::toJson(const SymbolData& data) {
    QJsonObject json;
    json["info"] = toJson(data.info());
    json["bbox"] = toJson(data.bbox());

    QJsonArray pinsArray;
    for (const SymbolPin& pin : data.pins()) {
        pinsArray.append(toJson(pin));
    }
    json["pins"] = pinsArray;

    QJsonArray rectanglesArray;
    for (const SymbolRectangle& rect : data.rectangles()) {
        rectanglesArray.append(toJson(rect));
    }
    json["rectangles"] = rectanglesArray;

    QJsonArray circlesArray;
    for (const SymbolCircle& circle : data.circles()) {
        circlesArray.append(toJson(circle));
    }
    json["circles"] = circlesArray;

    QJsonArray arcsArray;
    for (const SymbolArc& arc : data.arcs()) {
        arcsArray.append(toJson(arc));
    }
    json["arcs"] = arcsArray;

    QJsonArray ellipsesArray;
    for (const SymbolEllipse& ellipse : data.ellipses()) {
        ellipsesArray.append(toJson(ellipse));
    }
    json["ellipses"] = ellipsesArray;

    QJsonArray polylinesArray;
    for (const SymbolPolyline& polyline : data.polylines()) {
        polylinesArray.append(toJson(polyline));
    }
    json["polylines"] = polylinesArray;

    QJsonArray polygonsArray;
    for (const SymbolPolygon& polygon : data.polygons()) {
        polygonsArray.append(toJson(polygon));
    }
    json["polygons"] = polygonsArray;

    QJsonArray pathsArray;
    for (const SymbolPath& path : data.paths()) {
        pathsArray.append(toJson(path));
    }
    json["paths"] = pathsArray;

    QJsonArray textsArray;
    for (const SymbolText& text : data.texts()) {
        textsArray.append(toJson(text));
    }
    json["texts"] = textsArray;

    // 添加多部分符号的部分
    QJsonArray partsArray;
    for (const SymbolPart& part : data.parts()) {
        partsArray.append(toJson(part));
    }
    json["parts"] = partsArray;

    return json;
}

bool SymbolDataSerializer::fromJson(SymbolData& data, const QJsonObject& json) {
    // 读取信息
    if (json.contains("info") && json["info"].isObject()) {
        SymbolInfo info;
        if (!fromJson(info, json["info"].toObject())) {
            qWarning() << "Failed to parse symbol info";
            return false;
        }
        data.setInfo(info);
    }

    // 读取边界
    if (json.contains("bbox") && json["bbox"].isObject()) {
        SymbolBBox bbox;
        if (!fromJson(bbox, json["bbox"].toObject())) {
            qWarning() << "Failed to parse symbol bbox";
            return false;
        }
        data.setBbox(bbox);
    }

    // 读取引脚
    if (json.contains("pins") && json["pins"].isArray()) {
        QJsonArray pinsArray = json["pins"].toArray();
        QList<SymbolPin> pins;
        for (const QJsonValue& value : pinsArray) {
            if (value.isObject()) {
                SymbolPin pin;
                if (fromJson(pin, value.toObject())) {
                    pins.append(pin);
                }
            }
        }
        data.setPins(pins);
    }

    // 读取矩形
    if (json.contains("rectangles") && json["rectangles"].isArray()) {
        QJsonArray rectanglesArray = json["rectangles"].toArray();
        QList<SymbolRectangle> rectangles;
        for (const QJsonValue& value : rectanglesArray) {
            if (value.isObject()) {
                SymbolRectangle rect;
                if (fromJson(rect, value.toObject())) {
                    rectangles.append(rect);
                }
            }
        }
        data.setRectangles(rectangles);
    }

    // 读取
    if (json.contains("circles") && json["circles"].isArray()) {
        QJsonArray circlesArray = json["circles"].toArray();
        QList<SymbolCircle> circles;
        for (const QJsonValue& value : circlesArray) {
            if (value.isObject()) {
                SymbolCircle circle;
                if (fromJson(circle, value.toObject())) {
                    circles.append(circle);
                }
            }
        }
        data.setCircles(circles);
    }

    // 读取圆弧
    if (json.contains("arcs") && json["arcs"].isArray()) {
        QJsonArray arcsArray = json["arcs"].toArray();
        QList<SymbolArc> arcs;
        for (const QJsonValue& value : arcsArray) {
            if (value.isObject()) {
                SymbolArc arc;
                if (fromJson(arc, value.toObject())) {
                    arcs.append(arc);
                }
            }
        }
        data.setArcs(arcs);
    }

    // 读取椭圆
    if (json.contains("ellipses") && json["ellipses"].isArray()) {
        QJsonArray ellipsesArray = json["ellipses"].toArray();
        QList<SymbolEllipse> ellipses;
        for (const QJsonValue& value : ellipsesArray) {
            if (value.isObject()) {
                SymbolEllipse ellipse;
                if (fromJson(ellipse, value.toObject())) {
                    ellipses.append(ellipse);
                }
            }
        }
        data.setEllipses(ellipses);
    }

    // 读取多段
    if (json.contains("polylines") && json["polylines"].isArray()) {
        QJsonArray polylinesArray = json["polylines"].toArray();
        QList<SymbolPolyline> polylines;
        for (const QJsonValue& value : polylinesArray) {
            if (value.isObject()) {
                SymbolPolyline polyline;
                if (fromJson(polyline, value.toObject())) {
                    polylines.append(polyline);
                }
            }
        }
        data.setPolylines(polylines);
    }

    // 读取多边
    if (json.contains("polygons") && json["polygons"].isArray()) {
        QJsonArray polygonsArray = json["polygons"].toArray();
        QList<SymbolPolygon> polygons;
        for (const QJsonValue& value : polygonsArray) {
            if (value.isObject()) {
                SymbolPolygon polygon;
                if (fromJson(polygon, value.toObject())) {
                    polygons.append(polygon);
                }
            }
        }
        data.setPolygons(polygons);
    }

    // 读取路径
    if (json.contains("paths") && json["paths"].isArray()) {
        QJsonArray pathsArray = json["paths"].toArray();
        QList<SymbolPath> paths;
        for (const QJsonValue& value : pathsArray) {
            if (value.isObject()) {
                SymbolPath path;
                if (fromJson(path, value.toObject())) {
                    paths.append(path);
                }
            }
        }
        data.setPaths(paths);
    }

    // 读取文本
    if (json.contains("texts") && json["texts"].isArray()) {
        QJsonArray textsArray = json["texts"].toArray();
        QList<SymbolText> texts;
        for (const QJsonValue& value : textsArray) {
            if (value.isObject()) {
                SymbolText text;
                if (fromJson(text, value.toObject())) {
                    texts.append(text);
                }
            }
        }
        data.setTexts(texts);
    }

    // 读取多部分符号的部分
    if (json.contains("parts") && json["parts"].isArray()) {
        QJsonArray partsArray = json["parts"].toArray();
        QList<SymbolPart> parts;
        for (const QJsonValue& value : partsArray) {
            if (value.isObject()) {
                SymbolPart part;
                if (fromJson(part, value.toObject())) {
                    parts.append(part);
                }
            }
        }
        data.setParts(parts);
    }

    return true;
}

}  // namespace EasyKiConverter
