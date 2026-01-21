#include "FootprintData.h"
#include <QDebug>

namespace EasyKiConverter
{

    // ==================== FootprintInfo ====================

    QJsonObject FootprintInfo::toJson() const
    {
        QJsonObject json;
        json["name"] = name;
        json["type"] = type;
        json["model_3d_name"] = model3DName;

        // EasyEDA API 原始字段
        json["uuid"] = uuid;
        json["doc_type"] = docType;
        json["datastrid"] = datastrid;
        json["writable"] = writable;
        json["update_time"] = updateTime;

        // 编辑器信息
        json["editor_version"] = editorVersion;

        // 项目信息
        json["puuid"] = puuid;
        json["utime"] = utime;
        json["import_flag"] = importFlag;
        json["has_id_flag"] = hasIdFlag;
        json["newg_id"] = newgId;

        // 附加参数
        json["link"] = link;
        json["contributor"] = contributor;
        json["uuid_3d"] = uuid3d;

        // 画布信息
        json["canvas"] = canvas;

        // 层定义
        json["layers"] = layers;

        // 对象可见性
        json["objects"] = objects;

        return json;
    }

    bool FootprintInfo::fromJson(const QJsonObject &json)
    {
        name = json["name"].toString();
        type = json["type"].toString();
        model3DName = json["model_3d_name"].toString();

        // EasyEDA API 原始字段
        uuid = json["uuid"].toString();
        docType = json["doc_type"].toString();
        datastrid = json["datastrid"].toString();
        writable = json["writable"].toBool(false);
        updateTime = json["update_time"].toVariant().toLongLong();

        // 编辑器信息
        editorVersion = json["editor_version"].toString();

        // 项目信息
        puuid = json["puuid"].toString();
        utime = json["utime"].toVariant().toLongLong();
        importFlag = json["import_flag"].toBool(false);
        hasIdFlag = json["has_id_flag"].toBool(false);
        newgId = json["newg_id"].toBool(false);

        // 附加参数
        link = json["link"].toString();
        contributor = json["contributor"].toString();
        uuid3d = json["uuid_3d"].toString();

        // 画布信息
        canvas = json["canvas"].toString();

        // 层定义
        layers = json["layers"].toString();

        // 对象可见性
        objects = json["objects"].toString();

        return true;
    }

    // ==================== FootprintBBox ====================

    QJsonObject FootprintBBox::toJson() const
    {
        QJsonObject json;
        json["x"] = x;
        json["y"] = y;
        json["width"] = width;
        json["height"] = height;
        return json;
    }

    bool FootprintBBox::fromJson(const QJsonObject &json)
    {
        x = json["x"].toDouble();
        y = json["y"].toDouble();
        width = json["width"].toDouble();
        height = json["height"].toDouble();
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

    // ==================== FootprintSolidRegion ====================

    QJsonObject FootprintSolidRegion::toJson() const
    {
        QJsonObject json;
        json["path"] = path;
        json["layer_id"] = layerId;
        json["fill_style"] = fillStyle;
        json["id"] = id;
        json["is_keep_out"] = isKeepOut;
        json["is_locked"] = isLocked;
        return json;
    }

    bool FootprintSolidRegion::fromJson(const QJsonObject &json)
    {
        path = json["path"].toString();
        layerId = json["layer_id"].toInt();
        fillStyle = json["fill_style"].toString();
        id = json["id"].toString();
        isKeepOut = json["is_keep_out"].toBool();
        isLocked = json["is_locked"].toBool();
        return true;
    }

    // ==================== FootprintOutline ====================

    QJsonObject FootprintOutline::toJson() const
    {
        QJsonObject json;
        json["path"] = path;
        json["layer_id"] = layerId;
        json["stroke_width"] = strokeWidth;
        json["id"] = id;
        json["is_locked"] = isLocked;
        return json;
    }

    bool FootprintOutline::fromJson(const QJsonObject &json)
    {
        path = json["path"].toString();
        layerId = json["layer_id"].toInt();
        strokeWidth = json["stroke_width"].toDouble();
        id = json["id"].toString();
        isLocked = json["is_locked"].toBool();
        return true;
    }

    // ==================== LayerDefinition ====================

    QJsonObject LayerDefinition::toJson() const
    {
        QJsonObject json;
        json["layer_id"] = layerId;
        json["name"] = name;
        json["color"] = color;
        json["is_visible"] = isVisible;
        json["is_used_for_manufacturing"] = isUsedForManufacturing;
        json["expansion"] = expansion;
        return json;
    }

    bool LayerDefinition::fromJson(const QJsonObject &json)
    {
        layerId = json["layer_id"].toInt();
        name = json["name"].toString();
        color = json["color"].toString();
        isVisible = json["is_visible"].toBool();
        isUsedForManufacturing = json["is_used_for_manufacturing"].toBool();
        expansion = json["expansion"].toDouble();
        return true;
    }

    // ==================== ObjectVisibility ====================

    QJsonObject ObjectVisibility::toJson() const
    {
        QJsonObject json;
        json["object_type"] = objectType;
        json["is_enabled"] = isEnabled;
        json["is_visible"] = isVisible;
        return json;
    }

    bool ObjectVisibility::fromJson(const QJsonObject &json)
    {
        objectType = json["object_type"].toString();
        isEnabled = json["is_enabled"].toBool();
        isVisible = json["is_visible"].toBool();
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
        for (const FootprintPad &pad : m_pads)
        {
            padsArray.append(pad.toJson());
        }
        json["pads"] = padsArray;

        // 走线
        QJsonArray tracksArray;
        for (const FootprintTrack &track : m_tracks)
        {
            tracksArray.append(track.toJson());
        }
        json["tracks"] = tracksArray;

        // 孔
        QJsonArray holesArray;
        for (const FootprintHole &hole : m_holes)
        {
            holesArray.append(hole.toJson());
        }
        json["holes"] = holesArray;

        // 圆
        QJsonArray circlesArray;
        for (const FootprintCircle &circle : m_circles)
        {
            circlesArray.append(circle.toJson());
        }
        json["circles"] = circlesArray;

        // 矩形
        QJsonArray rectanglesArray;
        for (const FootprintRectangle &rect : m_rectangles)
        {
            rectanglesArray.append(rect.toJson());
        }
        json["rectangles"] = rectanglesArray;

        // 圆弧
        QJsonArray arcsArray;
        for (const FootprintArc &arc : m_arcs)
        {
            arcsArray.append(arc.toJson());
        }
        json["arcs"] = arcsArray;

        // 文本
        QJsonArray textsArray;
        for (const FootprintText &text : m_texts)
        {
            textsArray.append(text.toJson());
        }
        json["texts"] = textsArray;

        // 实体填充区域
        QJsonArray solidRegionsArray;
        for (const FootprintSolidRegion &solidRegion : m_solidRegions)
        {
            solidRegionsArray.append(solidRegion.toJson());
        }
        json["solid_regions"] = solidRegionsArray;

        // 外形轮廓
        QJsonArray outlinesArray;
        for (const FootprintOutline &outline : m_outlines)
        {
            outlinesArray.append(outline.toJson());
        }
        json["outlines"] = outlinesArray;

        // 层定义
        QJsonArray layersArray;
        for (const LayerDefinition &layer : m_layers)
        {
            layersArray.append(layer.toJson());
        }
        json["layers"] = layersArray;

        // 对象可见性
        QJsonArray objectVisibilitiesArray;
        for (const ObjectVisibility &visibility : m_objectVisibilities)
        {
            objectVisibilitiesArray.append(visibility.toJson());
        }
        json["object_visibilities"] = objectVisibilitiesArray;

        return json;
    }

    bool FootprintData::fromJson(const QJsonObject &json)
    {
        // 读取基本信息
        if (json.contains("info") && json["info"].isObject())
        {
            if (!m_info.fromJson(json["info"].toObject()))
            {
                qWarning() << "Failed to parse footprint info";
                return false;
            }
        }

        // 读取边界框
        if (json.contains("bbox") && json["bbox"].isObject())
        {
            if (!m_bbox.fromJson(json["bbox"].toObject()))
            {
                qWarning() << "Failed to parse footprint bbox";
                return false;
            }
        }

        // 读取焊盘
        if (json.contains("pads") && json["pads"].isArray())
        {
            QJsonArray padsArray = json["pads"].toArray();
            m_pads.clear();
            for (const QJsonValue &value : padsArray)
            {
                if (value.isObject())
                {
                    FootprintPad pad;
                    if (pad.fromJson(value.toObject()))
                    {
                        m_pads.append(pad);
                    }
                }
            }
        }

        // 读取走线
        if (json.contains("tracks") && json["tracks"].isArray())
        {
            QJsonArray tracksArray = json["tracks"].toArray();
            m_tracks.clear();
            for (const QJsonValue &value : tracksArray)
            {
                if (value.isObject())
                {
                    FootprintTrack track;
                    if (track.fromJson(value.toObject()))
                    {
                        m_tracks.append(track);
                    }
                }
            }
        }

        // 读取孔
        if (json.contains("holes") && json["holes"].isArray())
        {
            QJsonArray holesArray = json["holes"].toArray();
            m_holes.clear();
            for (const QJsonValue &value : holesArray)
            {
                if (value.isObject())
                {
                    FootprintHole hole;
                    if (hole.fromJson(value.toObject()))
                    {
                        m_holes.append(hole);
                    }
                }
            }
        }

        // 读取圆
        if (json.contains("circles") && json["circles"].isArray())
        {
            QJsonArray circlesArray = json["circles"].toArray();
            m_circles.clear();
            for (const QJsonValue &value : circlesArray)
            {
                if (value.isObject())
                {
                    FootprintCircle circle;
                    if (circle.fromJson(value.toObject()))
                    {
                        m_circles.append(circle);
                    }
                }
            }
        }

        // 读取矩形
        if (json.contains("rectangles") && json["rectangles"].isArray())
        {
            QJsonArray rectanglesArray = json["rectangles"].toArray();
            m_rectangles.clear();
            for (const QJsonValue &value : rectanglesArray)
            {
                if (value.isObject())
                {
                    FootprintRectangle rect;
                    if (rect.fromJson(value.toObject()))
                    {
                        m_rectangles.append(rect);
                    }
                }
            }
        }

        // 读取圆弧
        if (json.contains("arcs") && json["arcs"].isArray())
        {
            QJsonArray arcsArray = json["arcs"].toArray();
            m_arcs.clear();
            for (const QJsonValue &value : arcsArray)
            {
                if (value.isObject())
                {
                    FootprintArc arc;
                    if (arc.fromJson(value.toObject()))
                    {
                        m_arcs.append(arc);
                    }
                }
            }
        }

        // 读取文本
        if (json.contains("texts") && json["texts"].isArray())
        {
            QJsonArray textsArray = json["texts"].toArray();
            m_texts.clear();
            for (const QJsonValue &value : textsArray)
            {
                if (value.isObject())
                {
                    FootprintText text;
                    if (text.fromJson(value.toObject()))
                    {
                        m_texts.append(text);
                    }
                }
            }
        }

        // 读取实体填充区域
        if (json.contains("solid_regions") && json["solid_regions"].isArray())
        {
            QJsonArray solidRegionsArray = json["solid_regions"].toArray();
            m_solidRegions.clear();
            for (const QJsonValue &value : solidRegionsArray)
            {
                if (value.isObject())
                {
                    FootprintSolidRegion solidRegion;
                    if (solidRegion.fromJson(value.toObject()))
                    {
                        m_solidRegions.append(solidRegion);
                    }
                }
            }
        }

        // 读取外形轮廓
        if (json.contains("outlines") && json["outlines"].isArray())
        {
            QJsonArray outlinesArray = json["outlines"].toArray();
            m_outlines.clear();
            for (const QJsonValue &value : outlinesArray)
            {
                if (value.isObject())
                {
                    FootprintOutline outline;
                    if (outline.fromJson(value.toObject()))
                    {
                        m_outlines.append(outline);
                    }
                }
            }
        }

        // 读取层定义
        if (json.contains("layers") && json["layers"].isArray())
        {
            QJsonArray layersArray = json["layers"].toArray();
            m_layers.clear();
            for (const QJsonValue &value : layersArray)
            {
                if (value.isObject())
                {
                    LayerDefinition layer;
                    if (layer.fromJson(value.toObject()))
                    {
                        m_layers.append(layer);
                    }
                }
            }
        }

        // 读取对象可见性
        if (json.contains("object_visibilities") && json["object_visibilities"].isArray())
        {
            QJsonArray objectVisibilitiesArray = json["object_visibilities"].toArray();
            m_objectVisibilities.clear();
            for (const QJsonValue &value : objectVisibilitiesArray)
            {
                if (value.isObject())
                {
                    ObjectVisibility visibility;
                    if (visibility.fromJson(value.toObject()))
                    {
                        m_objectVisibilities.append(visibility);
                    }
                }
            }
        }

        return true;
    }

    bool FootprintData::isValid() const
    {
        // 检查基本信息
        if (m_info.name.isEmpty())
        {
            return false;
        }

        // 至少要有一个焊盘
        if (m_pads.isEmpty())
        {
            return false;
        }

        return true;
    }

    QString FootprintData::validate() const
    {
        if (m_info.name.isEmpty())
        {
            return "Footprint name is empty";
        }

        if (m_pads.isEmpty())
        {
            return "Footprint must have at least one pad";
        }

        // 检查焊盘
        for (int i = 0; i < m_pads.size(); ++i)
        {
            const FootprintPad &pad = m_pads[i];
            if (pad.number.isEmpty())
            {
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
        m_solidRegions.clear();
        m_outlines.clear();
        m_layers.clear();
        m_objectVisibilities.clear();
    }

} // namespace EasyKiConverter
