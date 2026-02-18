#include "FootprintDataSerializer.h"

#include <QDebug>
#include <QJsonArray>

namespace EasyKiConverter {

// ==================== FootprintInfo ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintInfo& info) {
    QJsonObject json;
    json["name"] = info.name;
    json["type"] = info.type;
    json["model_3d_name"] = info.model3DName;

    // EasyEDA API 原始字段
    json["uuid"] = info.uuid;
    json["doc_type"] = info.docType;
    json["datastrid"] = info.datastrid;
    json["writable"] = info.writable;
    json["update_time"] = info.updateTime;

    // 编辑器信
    json["editor_version"] = info.editorVersion;

    // 项目信息
    json["puuid"] = info.puuid;
    json["utime"] = info.utime;
    json["import_flag"] = info.importFlag;
    json["has_id_flag"] = info.hasIdFlag;
    json["newg_id"] = info.newgId;

    // 附加参数
    json["link"] = info.link;
    json["contributor"] = info.contributor;
    json["uuid_3d"] = info.uuid3d;

    // 画布信息
    json["canvas"] = info.canvas;

    // 层定位
    json["layers"] = info.layers;

    // 对象可见
    json["objects"] = info.objects;

    return json;
}

bool FootprintDataSerializer::fromJson(FootprintInfo& info, const QJsonObject& json) {
    info.name = json["name"].toString();
    info.type = json["type"].toString();
    info.model3DName = json["model_3d_name"].toString();

    // EasyEDA API 原始字段
    info.uuid = json["uuid"].toString();
    info.docType = json["doc_type"].toString();
    info.datastrid = json["datastrid"].toString();
    info.writable = json["writable"].toBool(false);
    info.updateTime = json["update_time"].toVariant().toLongLong();

    // 编辑器信
    info.editorVersion = json["editor_version"].toString();

    // 项目信息
    info.puuid = json["puuid"].toString();
    info.utime = json["utime"].toVariant().toLongLong();
    info.importFlag = json["import_flag"].toBool(false);
    info.hasIdFlag = json["has_id_flag"].toBool(false);
    info.newgId = json["newg_id"].toBool(false);

    // 附加参数
    info.link = json["link"].toString();
    info.contributor = json["contributor"].toString();
    info.uuid3d = json["uuid_3d"].toString();

    // 画布信息
    info.canvas = json["canvas"].toString();

    // 层定位
    info.layers = json["layers"].toString();

    // 对象可见
    info.objects = json["objects"].toString();

    return true;
}

// ==================== FootprintBBox ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintBBox& bbox) {
    QJsonObject json;
    json["x"] = bbox.x;
    json["y"] = bbox.y;
    json["width"] = bbox.width;
    json["height"] = bbox.height;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintBBox& bbox, const QJsonObject& json) {
    bbox.x = json["x"].toDouble();
    bbox.y = json["y"].toDouble();
    bbox.width = json["width"].toDouble();
    bbox.height = json["height"].toDouble();
    return true;
}

// ==================== FootprintPad ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintPad& pad) {
    QJsonObject json;
    json["shape"] = pad.shape;
    json["center_x"] = pad.centerX;
    json["center_y"] = pad.centerY;
    json["width"] = pad.width;
    json["height"] = pad.height;
    json["layer_id"] = pad.layerId;
    json["net"] = pad.net;
    json["number"] = pad.number;
    json["hole_radius"] = pad.holeRadius;
    json["points"] = pad.points;
    json["rotation"] = pad.rotation;
    json["id"] = pad.id;
    json["hole_length"] = pad.holeLength;
    json["hole_point"] = pad.holePoint;
    json["is_plated"] = pad.isPlated;
    json["is_locked"] = pad.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintPad& pad, const QJsonObject& json) {
    pad.shape = json["shape"].toString();
    pad.centerX = json["center_x"].toDouble();
    pad.centerY = json["center_y"].toDouble();
    pad.width = json["width"].toDouble();
    pad.height = json["height"].toDouble();
    pad.layerId = json["layer_id"].toInt();
    pad.net = json["net"].toString();
    pad.number = json["number"].toString();
    pad.holeRadius = json["hole_radius"].toDouble();
    pad.points = json["points"].toString();
    pad.rotation = json["rotation"].toDouble();
    pad.id = json["id"].toString();
    pad.holeLength = json["hole_length"].toDouble();
    pad.holePoint = json["hole_point"].toString();
    pad.isPlated = json["is_plated"].toBool();
    pad.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintTrack ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintTrack& track) {
    QJsonObject json;
    json["stroke_width"] = track.strokeWidth;
    json["layer_id"] = track.layerId;
    json["net"] = track.net;
    json["points"] = track.points;
    json["id"] = track.id;
    json["is_locked"] = track.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintTrack& track, const QJsonObject& json) {
    track.strokeWidth = json["stroke_width"].toDouble();
    track.layerId = json["layer_id"].toInt();
    track.net = json["net"].toString();
    track.points = json["points"].toString();
    track.id = json["id"].toString();
    track.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintHole ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintHole& hole) {
    QJsonObject json;
    json["center_x"] = hole.centerX;
    json["center_y"] = hole.centerY;
    json["radius"] = hole.radius;
    json["id"] = hole.id;
    json["is_locked"] = hole.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintHole& hole, const QJsonObject& json) {
    hole.centerX = json["center_x"].toDouble();
    hole.centerY = json["center_y"].toDouble();
    hole.radius = json["radius"].toDouble();
    hole.id = json["id"].toString();
    hole.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintCircle ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintCircle& circle) {
    QJsonObject json;
    json["cx"] = circle.cx;
    json["cy"] = circle.cy;
    json["radius"] = circle.radius;
    json["stroke_width"] = circle.strokeWidth;
    json["layer_id"] = circle.layerId;
    json["id"] = circle.id;
    json["is_locked"] = circle.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintCircle& circle, const QJsonObject& json) {
    circle.cx = json["cx"].toDouble();
    circle.cy = json["cy"].toDouble();
    circle.radius = json["radius"].toDouble();
    circle.strokeWidth = json["stroke_width"].toDouble();
    circle.layerId = json["layer_id"].toInt();
    circle.id = json["id"].toString();
    circle.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintRectangle ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintRectangle& rect) {
    QJsonObject json;
    json["x"] = rect.x;
    json["y"] = rect.y;
    json["width"] = rect.width;
    json["height"] = rect.height;
    json["stroke_width"] = rect.strokeWidth;
    json["id"] = rect.id;
    json["layer_id"] = rect.layerId;
    json["is_locked"] = rect.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintRectangle& rect, const QJsonObject& json) {
    rect.x = json["x"].toDouble();
    rect.y = json["y"].toDouble();
    rect.width = json["width"].toDouble();
    rect.height = json["height"].toDouble();
    rect.strokeWidth = json["stroke_width"].toDouble();
    rect.id = json["id"].toString();
    rect.layerId = json["layer_id"].toInt();
    rect.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintArc ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintArc& arc) {
    QJsonObject json;
    json["stroke_width"] = arc.strokeWidth;
    json["layer_id"] = arc.layerId;
    json["net"] = arc.net;
    json["path"] = arc.path;
    json["helper_dots"] = arc.helperDots;
    json["id"] = arc.id;
    json["is_locked"] = arc.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintArc& arc, const QJsonObject& json) {
    arc.strokeWidth = json["stroke_width"].toDouble();
    arc.layerId = json["layer_id"].toInt();
    arc.net = json["net"].toString();
    arc.path = json["path"].toString();
    arc.helperDots = json["helper_dots"].toString();
    arc.id = json["id"].toString();
    arc.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintText ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintText& text) {
    QJsonObject json;
    json["type"] = text.type;
    json["center_x"] = text.centerX;
    json["center_y"] = text.centerY;
    json["stroke_width"] = text.strokeWidth;
    json["rotation"] = text.rotation;
    json["mirror"] = text.mirror;
    json["layer_id"] = text.layerId;
    json["net"] = text.net;
    json["font_size"] = text.fontSize;
    json["text"] = text.text;
    json["text_path"] = text.textPath;
    json["is_displayed"] = text.isDisplayed;
    json["id"] = text.id;
    json["is_locked"] = text.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintText& text, const QJsonObject& json) {
    text.type = json["type"].toString();
    text.centerX = json["center_x"].toDouble();
    text.centerY = json["center_y"].toDouble();
    text.strokeWidth = json["stroke_width"].toDouble();
    text.rotation = json["rotation"].toInt();
    text.mirror = json["mirror"].toString();
    text.layerId = json["layer_id"].toInt();
    text.net = json["net"].toString();
    text.fontSize = json["font_size"].toDouble();
    text.text = json["text"].toString();
    text.textPath = json["text_path"].toString();
    text.isDisplayed = json["is_displayed"].toBool();
    text.id = json["id"].toString();
    text.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintSolidRegion ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintSolidRegion& region) {
    QJsonObject json;
    json["path"] = region.path;
    json["layer_id"] = region.layerId;
    json["fill_style"] = region.fillStyle;
    json["id"] = region.id;
    json["is_keep_out"] = region.isKeepOut;
    json["is_locked"] = region.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintSolidRegion& region, const QJsonObject& json) {
    region.path = json["path"].toString();
    region.layerId = json["layer_id"].toInt();
    region.fillStyle = json["fill_style"].toString();
    region.id = json["id"].toString();
    region.isKeepOut = json["is_keep_out"].toBool();
    region.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintOutline ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintOutline& outline) {
    QJsonObject json;
    json["path"] = outline.path;
    json["layer_id"] = outline.layerId;
    json["stroke_width"] = outline.strokeWidth;
    json["id"] = outline.id;
    json["is_locked"] = outline.isLocked;
    return json;
}

bool FootprintDataSerializer::fromJson(FootprintOutline& outline, const QJsonObject& json) {
    outline.path = json["path"].toString();
    outline.layerId = json["layer_id"].toInt();
    outline.strokeWidth = json["stroke_width"].toDouble();
    outline.id = json["id"].toString();
    outline.isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== LayerDefinition ====================

QJsonObject FootprintDataSerializer::toJson(const LayerDefinition& layer) {
    QJsonObject json;
    json["layer_id"] = layer.layerId;
    json["name"] = layer.name;
    json["color"] = layer.color;
    json["is_visible"] = layer.isVisible;
    json["is_used_for_manufacturing"] = layer.isUsedForManufacturing;
    json["expansion"] = layer.expansion;
    return json;
}

bool FootprintDataSerializer::fromJson(LayerDefinition& layer, const QJsonObject& json) {
    layer.layerId = json["layer_id"].toInt();
    layer.name = json["name"].toString();
    layer.color = json["color"].toString();
    layer.isVisible = json["is_visible"].toBool();
    layer.isUsedForManufacturing = json["is_used_for_manufacturing"].toBool();
    layer.expansion = json["expansion"].toDouble();
    return true;
}

// ==================== ObjectVisibility ====================

QJsonObject FootprintDataSerializer::toJson(const ObjectVisibility& visibility) {
    QJsonObject json;
    json["object_type"] = visibility.objectType;
    json["is_enabled"] = visibility.isEnabled;
    json["is_visible"] = visibility.isVisible;
    return json;
}

bool FootprintDataSerializer::fromJson(ObjectVisibility& visibility, const QJsonObject& json) {
    visibility.objectType = json["object_type"].toString();
    visibility.isEnabled = json["is_enabled"].toBool();
    visibility.isVisible = json["is_visible"].toBool();
    return true;
}

// ==================== FootprintData ====================

QJsonObject FootprintDataSerializer::toJson(const FootprintData& data) {
    QJsonObject json;

    // 基本信息
    json["info"] = toJson(data.info());

    // 边界
    json["bbox"] = toJson(data.bbox());

    // 焊盘
    QJsonArray padsArray;
    for (const FootprintPad& pad : data.pads()) {
        padsArray.append(toJson(pad));
    }
    json["pads"] = padsArray;

    // 走线
    QJsonArray tracksArray;
    for (const FootprintTrack& track : data.tracks()) {
        tracksArray.append(toJson(track));
    }
    json["tracks"] = tracksArray;

    // 圆弧
    QJsonArray holesArray;
    for (const FootprintHole& hole : data.holes()) {
        holesArray.append(toJson(hole));
    }
    json["holes"] = holesArray;

    // 圆弧
    QJsonArray circlesArray;
    for (const FootprintCircle& circle : data.circles()) {
        circlesArray.append(toJson(circle));
    }
    json["circles"] = circlesArray;

    // 矩形
    QJsonArray rectanglesArray;
    for (const FootprintRectangle& rect : data.rectangles()) {
        rectanglesArray.append(toJson(rect));
    }
    json["rectangles"] = rectanglesArray;

    // 圆弧
    QJsonArray arcsArray;
    for (const FootprintArc& arc : data.arcs()) {
        arcsArray.append(toJson(arc));
    }
    json["arcs"] = arcsArray;

    // 文本
    QJsonArray textsArray;
    for (const FootprintText& text : data.texts()) {
        textsArray.append(toJson(text));
    }
    json["texts"] = textsArray;

    // 实体填充区域
    QJsonArray solidRegionsArray;
    for (const FootprintSolidRegion& solidRegion : data.solidRegions()) {
        solidRegionsArray.append(toJson(solidRegion));
    }
    json["solid_regions"] = solidRegionsArray;

    // 外形轮廓
    QJsonArray outlinesArray;
    for (const FootprintOutline& outline : data.outlines()) {
        outlinesArray.append(toJson(outline));
    }
    json["outlines"] = outlinesArray;

    // 层定位
    QJsonArray layersArray;
    for (const LayerDefinition& layer : data.layers()) {
        layersArray.append(toJson(layer));
    }
    json["layers"] = layersArray;

    // 对象可见
    QJsonArray objectVisibilitiesArray;
    for (const ObjectVisibility& visibility : data.objectVisibilities()) {
        objectVisibilitiesArray.append(toJson(visibility));
    }
    json["object_visibilities"] = objectVisibilitiesArray;

    return json;
}

bool FootprintDataSerializer::fromJson(FootprintData& data, const QJsonObject& json) {
    // 读取基本信息
    if (json.contains("info") && json["info"].isObject()) {
        FootprintInfo info;
        if (!fromJson(info, json["info"].toObject())) {
            qWarning() << "Failed to parse footprint info";
            return false;
        }
        data.setInfo(info);
    }

    // 读取边界
    if (json.contains("bbox") && json["bbox"].isObject()) {
        FootprintBBox bbox;
        if (!fromJson(bbox, json["bbox"].toObject())) {
            qWarning() << "Failed to parse footprint bbox";
            return false;
        }
        data.setBbox(bbox);
    }

    // 读取焊盘
    if (json.contains("pads") && json["pads"].isArray()) {
        QJsonArray padsArray = json["pads"].toArray();
        QList<FootprintPad> pads;
        for (const QJsonValue& value : padsArray) {
            if (value.isObject()) {
                FootprintPad pad;
                if (fromJson(pad, value.toObject())) {
                    pads.append(pad);
                }
            }
        }
        data.setPads(pads);
    }

    // 读取走线
    if (json.contains("tracks") && json["tracks"].isArray()) {
        QJsonArray tracksArray = json["tracks"].toArray();
        QList<FootprintTrack> tracks;
        for (const QJsonValue& value : tracksArray) {
            if (value.isObject()) {
                FootprintTrack track;
                if (fromJson(track, value.toObject())) {
                    tracks.append(track);
                }
            }
        }
        data.setTracks(tracks);
    }

    // 读取
    if (json.contains("holes") && json["holes"].isArray()) {
        QJsonArray holesArray = json["holes"].toArray();
        QList<FootprintHole> holes;
        for (const QJsonValue& value : holesArray) {
            if (value.isObject()) {
                FootprintHole hole;
                if (fromJson(hole, value.toObject())) {
                    holes.append(hole);
                }
            }
        }
        data.setHoles(holes);
    }

    // 读取
    if (json.contains("circles") && json["circles"].isArray()) {
        QJsonArray circlesArray = json["circles"].toArray();
        QList<FootprintCircle> circles;
        for (const QJsonValue& value : circlesArray) {
            if (value.isObject()) {
                FootprintCircle circle;
                if (fromJson(circle, value.toObject())) {
                    circles.append(circle);
                }
            }
        }
        data.setCircles(circles);
    }

    // 读取矩形
    if (json.contains("rectangles") && json["rectangles"].isArray()) {
        QJsonArray rectanglesArray = json["rectangles"].toArray();
        QList<FootprintRectangle> rectangles;
        for (const QJsonValue& value : rectanglesArray) {
            if (value.isObject()) {
                FootprintRectangle rect;
                if (fromJson(rect, value.toObject())) {
                    rectangles.append(rect);
                }
            }
        }
        data.setRectangles(rectangles);
    }

    // 读取圆弧
    if (json.contains("arcs") && json["arcs"].isArray()) {
        QJsonArray arcsArray = json["arcs"].toArray();
        QList<FootprintArc> arcs;
        for (const QJsonValue& value : arcsArray) {
            if (value.isObject()) {
                FootprintArc arc;
                if (fromJson(arc, value.toObject())) {
                    arcs.append(arc);
                }
            }
        }
        data.setArcs(arcs);
    }

    // 读取文本
    if (json.contains("texts") && json["texts"].isArray()) {
        QJsonArray textsArray = json["texts"].toArray();
        QList<FootprintText> texts;
        for (const QJsonValue& value : textsArray) {
            if (value.isObject()) {
                FootprintText text;
                if (fromJson(text, value.toObject())) {
                    texts.append(text);
                }
            }
        }
        data.setTexts(texts);
    }

    // 读取实体填充区域
    if (json.contains("solid_regions") && json["solid_regions"].isArray()) {
        QJsonArray solidRegionsArray = json["solid_regions"].toArray();
        QList<FootprintSolidRegion> solidRegions;
        for (const QJsonValue& value : solidRegionsArray) {
            if (value.isObject()) {
                FootprintSolidRegion solidRegion;
                if (fromJson(solidRegion, value.toObject())) {
                    solidRegions.append(solidRegion);
                }
            }
        }
        data.setSolidRegions(solidRegions);
    }

    // 读取外形轮廓
    if (json.contains("outlines") && json["outlines"].isArray()) {
        QJsonArray outlinesArray = json["outlines"].toArray();
        QList<FootprintOutline> outlines;
        for (const QJsonValue& value : outlinesArray) {
            if (value.isObject()) {
                FootprintOutline outline;
                if (fromJson(outline, value.toObject())) {
                    outlines.append(outline);
                }
            }
        }
        data.setOutlines(outlines);
    }

    // 读取层定位
    if (json.contains("layers") && json["layers"].isArray()) {
        QJsonArray layersArray = json["layers"].toArray();
        QList<LayerDefinition> layers;
        for (const QJsonValue& value : layersArray) {
            if (value.isObject()) {
                LayerDefinition layer;
                if (fromJson(layer, value.toObject())) {
                    layers.append(layer);
                }
            }
        }
        data.setLayers(layers);
    }

    // 读取对象可见
    if (json.contains("object_visibilities") && json["object_visibilities"].isArray()) {
        QJsonArray objectVisibilitiesArray = json["object_visibilities"].toArray();
        QList<ObjectVisibility> objectVisibilities;
        for (const QJsonValue& value : objectVisibilitiesArray) {
            if (value.isObject()) {
                ObjectVisibility visibility;
                if (fromJson(visibility, value.toObject())) {
                    objectVisibilities.append(visibility);
                }
            }
        }
        data.setObjectVisibilities(objectVisibilities);
    }

    return true;
}

}  // namespace EasyKiConverter
