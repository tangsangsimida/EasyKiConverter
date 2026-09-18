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
    json["description"] = info.description;

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

// 从 JSON 恢复封装基本信息及其来源字段。
bool FootprintDataSerializer::fromJson(FootprintInfo& info, const QJsonObject& json) {
    info.name = json["name"].toString();
    info.type = json["type"].toString();
    info.model3DName = json["model_3d_name"].toString();
    info.description = json["description"].toString();

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
    json["validation_errors"] = QJsonArray::fromStringList(data.validationErrors());

    return json;
}

// 按数据类别逐项恢复完整封装，并保留可诊断的导入错误。
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

    if (json.contains("validation_errors") && json["validation_errors"].isArray()) {
        for (const QJsonValue& value : json["validation_errors"].toArray())
            data.addValidationError(value.toString());
    }

    return true;
}

}  // namespace EasyKiConverter
