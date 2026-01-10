#include "FootprintData.h"
#include <QDebug>

namespace EasyKiConverter {

// ==================== FootprintInfo ====================

QJsonObject FootprintInfo::toJson() const
{
    QJsonObject json;
    json["name"] = name;
    json["type"] = type;
    json["model_3d_name"] = model3DName;
    return json;
}

bool FootprintInfo::fromJson(const QJsonObject &json)
{
    name = json["name"].toString();
    type = json["type"].toString();
    model3DName = json["model_3d_name"].toString();
    return true;
}

// ==================== FootprintBBox ====================

QJsonObject FootprintBBox::toJson() const
{
    QJsonObject json;
    json["x"] = x;
    json["y"] = y;
    return json;
}

bool FootprintBBox::fromJson(const QJsonObject &json)
{
    x = json["x"].toDouble();
    y = json["y"].toDouble();
    return true;
}

// ==================== FootprintPad ====================

QJsonObject FootprintPad::toJson() const
{
    QJsonObject json;
    json["shape"] = shape;
    json["center_x"] = centerX;
    json["center_y"] = centerY;
    json["width"] = width;
    json["height"] = height;
    json["layer_id"] = layerId;
    json["net"] = net;
    json["number"] = number;
    json["hole_radius"] = holeRadius;
    json["points"] = points;
    json["rotation"] = rotation;
    json["id"] = id;
    json["hole_length"] = holeLength;
    json["hole_point"] = holePoint;
    json["is_plated"] = isPlated;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintPad::fromJson(const QJsonObject &json)
{
    shape = json["shape"].toString();
    centerX = json["center_x"].toDouble();
    centerY = json["center_y"].toDouble();
    width = json["width"].toDouble();
    height = json["height"].toDouble();
    layerId = json["layer_id"].toInt();
    net = json["net"].toString();
    number = json["number"].toString();
    holeRadius = json["hole_radius"].toDouble();
    points = json["points"].toString();
    rotation = json["rotation"].toDouble();
    id = json["id"].toString();
    holeLength = json["hole_length"].toDouble();
    holePoint = json["hole_point"].toString();
    isPlated = json["is_plated"].toBool();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintTrack ====================

QJsonObject FootprintTrack::toJson() const
{
    QJsonObject json;
    json["stroke_width"] = strokeWidth;
    json["layer_id"] = layerId;
    json["net"] = net;
    json["points"] = points;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintTrack::fromJson(const QJsonObject &json)
{
    strokeWidth = json["stroke_width"].toDouble();
    layerId = json["layer_id"].toInt();
    net = json["net"].toString();
    points = json["points"].toString();
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintHole ====================

QJsonObject FootprintHole::toJson() const
{
    QJsonObject json;
    json["center_x"] = centerX;
    json["center_y"] = centerY;
    json["radius"] = radius;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintHole::fromJson(const QJsonObject &json)
{
    centerX = json["center_x"].toDouble();
    centerY = json["center_y"].toDouble();
    radius = json["radius"].toDouble();
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintCircle ====================

QJsonObject FootprintCircle::toJson() const
{
    QJsonObject json;
    json["cx"] = cx;
    json["cy"] = cy;
    json["radius"] = radius;
    json["stroke_width"] = strokeWidth;
    json["layer_id"] = layerId;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintCircle::fromJson(const QJsonObject &json)
{
    cx = json["cx"].toDouble();
    cy = json["cy"].toDouble();
    radius = json["radius"].toDouble();
    strokeWidth = json["stroke_width"].toDouble();
    layerId = json["layer_id"].toInt();
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintRectangle ====================

QJsonObject FootprintRectangle::toJson() const
{
    QJsonObject json;
    json["x"] = x;
    json["y"] = y;
    json["width"] = width;
    json["height"] = height;
    json["stroke_width"] = strokeWidth;
    json["id"] = id;
    json["layer_id"] = layerId;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintRectangle::fromJson(const QJsonObject &json)
{
    x = json["x"].toDouble();
    y = json["y"].toDouble();
    width = json["width"].toDouble();
    height = json["height"].toDouble();
    strokeWidth = json["stroke_width"].toDouble();
    id = json["id"].toString();
    layerId = json["layer_id"].toInt();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintArc ====================

QJsonObject FootprintArc::toJson() const
{
    QJsonObject json;
    json["stroke_width"] = strokeWidth;
    json["layer_id"] = layerId;
    json["net"] = net;
    json["path"] = path;
    json["helper_dots"] = helperDots;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintArc::fromJson(const QJsonObject &json)
{
    strokeWidth = json["stroke_width"].toDouble();
    layerId = json["layer_id"].toInt();
    net = json["net"].toString();
    path = json["path"].toString();
    helperDots = json["helper_dots"].toString();
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintText ====================

QJsonObject FootprintText::toJson() const
{
    QJsonObject json;
    json["type"] = type;
    json["center_x"] = centerX;
    json["center_y"] = centerY;
    json["stroke_width"] = strokeWidth;
    json["rotation"] = rotation;
    json["mirror"] = mirror;
    json["layer_id"] = layerId;
    json["net"] = net;
    json["font_size"] = fontSize;
    json["text"] = text;
    json["text_path"] = textPath;
    json["is_displayed"] = isDisplayed;
    json["id"] = id;
    json["is_locked"] = isLocked;
    return json;
}

bool FootprintText::fromJson(const QJsonObject &json)
{
    type = json["type"].toString();
    centerX = json["center_x"].toDouble();
    centerY = json["center_y"].toDouble();
    strokeWidth = json["stroke_width"].toDouble();
    rotation = json["rotation"].toInt();
    mirror = json["mirror"].toString();
    layerId = json["layer_id"].toInt();
    net = json["net"].toString();
    fontSize = json["font_size"].toDouble();
    text = json["text"].toString();
    textPath = json["text_path"].toString();
    isDisplayed = json["is_displayed"].toBool();
    id = json["id"].toString();
    isLocked = json["is_locked"].toBool();
    return true;
}

// ==================== FootprintData ====================

FootprintData::FootprintData()
{
}

QJsonObject FootprintData::toJson() const
{
    QJsonObject json;

    // 基本信息
    json["info"] = m_info.toJson();

    // 边界框
    json["bbox"] = m_bbox.toJson();

    // 焊盘
    QJsonArray padsArray;
    for (const FootprintPad &pad : m_pads) {
        padsArray.append(pad.toJson());
    }
    json["pads"] = padsArray;

    // 走线
    QJsonArray tracksArray;
    for (const FootprintTrack &track : m_tracks) {
        tracksArray.append(track.toJson());
    }
    json["tracks"] = tracksArray;

    // 孔
    QJsonArray holesArray;
    for (const FootprintHole &hole : m_holes) {
        holesArray.append(hole.toJson());
    }
    json["holes"] = holesArray;

    // 圆
    QJsonArray circlesArray;
    for (const FootprintCircle &circle : m_circles) {
        circlesArray.append(circle.toJson());
    }
    json["circles"] = circlesArray;

    // 矩形
    QJsonArray rectanglesArray;
    for (const FootprintRectangle &rect : m_rectangles) {
        rectanglesArray.append(rect.toJson());
    }
    json["rectangles"] = rectanglesArray;

    // 圆弧
    QJsonArray arcsArray;
    for (const FootprintArc &arc : m_arcs) {
        arcsArray.append(arc.toJson());
    }
    json["arcs"] = arcsArray;

    // 文本
    QJsonArray textsArray;
    for (const FootprintText &text : m_texts) {
        textsArray.append(text.toJson());
    }
    json["texts"] = textsArray;

    return json;
}

bool FootprintData::fromJson(const QJsonObject &json)
{
    // 读取基本信息
    if (json.contains("info") && json["info"].isObject()) {
        if (!m_info.fromJson(json["info"].toObject())) {
            qWarning() << "Failed to parse footprint info";
            return false;
        }
    }

    // 读取边界框
    if (json.contains("bbox") && json["bbox"].isObject()) {
        if (!m_bbox.fromJson(json["bbox"].toObject())) {
            qWarning() << "Failed to parse footprint bbox";
            return false;
        }
    }

    // 读取焊盘
    if (json.contains("pads") && json["pads"].isArray()) {
        QJsonArray padsArray = json["pads"].toArray();
        m_pads.clear();
        for (const QJsonValue &value : padsArray) {
            if (value.isObject()) {
                FootprintPad pad;
                if (pad.fromJson(value.toObject())) {
                    m_pads.append(pad);
                }
            }
        }
    }

    // 读取走线
    if (json.contains("tracks") && json["tracks"].isArray()) {
        QJsonArray tracksArray = json["tracks"].toArray();
        m_tracks.clear();
        for (const QJsonValue &value : tracksArray) {
            if (value.isObject()) {
                FootprintTrack track;
                if (track.fromJson(value.toObject())) {
                    m_tracks.append(track);
                }
            }
        }
    }

    // 读取孔
    if (json.contains("holes") && json["holes"].isArray()) {
        QJsonArray holesArray = json["holes"].toArray();
        m_holes.clear();
        for (const QJsonValue &value : holesArray) {
            if (value.isObject()) {
                FootprintHole hole;
                if (hole.fromJson(value.toObject())) {
                    m_holes.append(hole);
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
                FootprintCircle circle;
                if (circle.fromJson(value.toObject())) {
                    m_circles.append(circle);
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
                FootprintRectangle rect;
                if (rect.fromJson(value.toObject())) {
                    m_rectangles.append(rect);
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
                FootprintArc arc;
                if (arc.fromJson(value.toObject())) {
                    m_arcs.append(arc);
                }
            }
        }
    }

    // 读取文本
    if (json.contains("texts") && json["texts"].isArray()) {
        QJsonArray textsArray = json["texts"].toArray();
        m_texts.clear();
        for (const QJsonValue &value : textsArray) {
            if (value.isObject()) {
                FootprintText text;
                if (text.fromJson(value.toObject())) {
                    m_texts.append(text);
                }
            }
        }
    }

    return true;
}

bool FootprintData::isValid() const
{
    // 检查基本信息
    if (m_info.name.isEmpty()) {
        return false;
    }

    // 至少要有一个焊盘
    if (m_pads.isEmpty()) {
        return false;
    }

    return true;
}

QString FootprintData::validate() const
{
    if (m_info.name.isEmpty()) {
        return "Footprint name is empty";
    }

    if (m_pads.isEmpty()) {
        return "Footprint must have at least one pad";
    }

    // 检查焊盘
    for (int i = 0; i < m_pads.size(); ++i) {
        const FootprintPad &pad = m_pads[i];
        if (pad.number.isEmpty()) {
            return QString("Pad %1 has empty number").arg(i);
        }
    }

    return QString(); // 返回空字符串表示验证通过
}

void FootprintData::clear()
{
    m_info = FootprintInfo();
    m_bbox = FootprintBBox();
    m_pads.clear();
    m_tracks.clear();
    m_holes.clear();
    m_circles.clear();
    m_rectangles.clear();
    m_arcs.clear();
    m_texts.clear();
}

} // namespace EasyKiConverter