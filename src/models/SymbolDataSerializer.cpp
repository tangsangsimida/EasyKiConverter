#include "SymbolDataSerializer.h"

#include "SymbolPinSerializer.h"
#include "SymbolShapeSerializer.h"

#include <QDebug>
#include <QJsonArray>

namespace EasyKiConverter {

// Pin sub-type helpers delegate to SymbolPinSerializer
static QJsonObject toJson(const SymbolPinSettings& settings) {
    return SymbolPinSerializer::toJson(settings);
}

static bool fromJson(SymbolPinSettings& settings, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(settings, json);
}

static QJsonObject toJson(const SymbolPinDot& dot) {
    return SymbolPinSerializer::toJson(dot);
}

static bool fromJson(SymbolPinDot& dot, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(dot, json);
}

static QJsonObject toJson(const SymbolPinPath& path) {
    return SymbolPinSerializer::toJson(path);
}

static bool fromJson(SymbolPinPath& path, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(path, json);
}

static QJsonObject toJson(const SymbolPinName& name) {
    return SymbolPinSerializer::toJson(name);
}

static bool fromJson(SymbolPinName& name, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(name, json);
}

static QJsonObject toJson(const SymbolPinDotBis& dot) {
    return SymbolPinSerializer::toJson(dot);
}

static bool fromJson(SymbolPinDotBis& dot, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(dot, json);
}

static QJsonObject toJson(const SymbolPinClock& clock) {
    return SymbolPinSerializer::toJson(clock);
}

static bool fromJson(SymbolPinClock& clock, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(clock, json);
}

// Shape helpers delegate to SymbolShapeSerializer
static QJsonObject toJson(const SymbolBBox& bbox) {
    return SymbolShapeSerializer::toJson(bbox);
}

static bool fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(bbox, json);
}

static QJsonObject toJson(const SymbolRectangle& rect) {
    return SymbolShapeSerializer::toJson(rect);
}

static bool fromJson(SymbolRectangle& rect, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(rect, json);
}

static QJsonObject toJson(const SymbolCircle& circle) {
    return SymbolShapeSerializer::toJson(circle);
}

static bool fromJson(SymbolCircle& circle, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(circle, json);
}

static QJsonObject toJson(const SymbolArc& arc) {
    return SymbolShapeSerializer::toJson(arc);
}

static bool fromJson(SymbolArc& arc, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(arc, json);
}

static QJsonObject toJson(const SymbolEllipse& ellipse) {
    return SymbolShapeSerializer::toJson(ellipse);
}

static bool fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(ellipse, json);
}

static QJsonObject toJson(const SymbolPolyline& polyline) {
    return SymbolShapeSerializer::toJson(polyline);
}

static bool fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polyline, json);
}

static QJsonObject toJson(const SymbolPolygon& polygon) {
    return SymbolShapeSerializer::toJson(polygon);
}

static bool fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polygon, json);
}

static QJsonObject toJson(const SymbolPath& path) {
    return SymbolShapeSerializer::toJson(path);
}

static bool fromJson(SymbolPath& path, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(path, json);
}

static QJsonObject toJson(const SymbolText& text) {
    return SymbolShapeSerializer::toJson(text);
}

static bool fromJson(SymbolText& text, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(text, json);
}

// Class method definitions for shape types (also delegate to SymbolShapeSerializer)
QJsonObject SymbolDataSerializer::toJson(const SymbolBBox& bbox) {
    return SymbolShapeSerializer::toJson(bbox);
}

bool SymbolDataSerializer::fromJson(SymbolBBox& bbox, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(bbox, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolRectangle& rect) {
    return SymbolShapeSerializer::toJson(rect);
}

bool SymbolDataSerializer::fromJson(SymbolRectangle& rect, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(rect, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolCircle& circle) {
    return SymbolShapeSerializer::toJson(circle);
}

bool SymbolDataSerializer::fromJson(SymbolCircle& circle, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(circle, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolArc& arc) {
    return SymbolShapeSerializer::toJson(arc);
}

bool SymbolDataSerializer::fromJson(SymbolArc& arc, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(arc, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolEllipse& ellipse) {
    return SymbolShapeSerializer::toJson(ellipse);
}

bool SymbolDataSerializer::fromJson(SymbolEllipse& ellipse, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(ellipse, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolPolyline& polyline) {
    return SymbolShapeSerializer::toJson(polyline);
}

bool SymbolDataSerializer::fromJson(SymbolPolyline& polyline, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polyline, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolPolygon& polygon) {
    return SymbolShapeSerializer::toJson(polygon);
}

bool SymbolDataSerializer::fromJson(SymbolPolygon& polygon, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(polygon, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolPath& path) {
    return SymbolShapeSerializer::toJson(path);
}

bool SymbolDataSerializer::fromJson(SymbolPath& path, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(path, json);
}

QJsonObject SymbolDataSerializer::toJson(const SymbolText& text) {
    return SymbolShapeSerializer::toJson(text);
}

bool SymbolDataSerializer::fromJson(SymbolText& text, const QJsonObject& json) {
    return SymbolShapeSerializer::fromJson(text, json);
}

// SymbolPin delegates to SymbolPinSerializer
QJsonObject SymbolDataSerializer::toJson(const SymbolPin& pin) {
    return SymbolPinSerializer::toJson(pin);
}

bool SymbolDataSerializer::fromJson(SymbolPin& pin, const QJsonObject& json) {
    return SymbolPinSerializer::fromJson(pin, json);
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