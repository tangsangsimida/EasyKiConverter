#ifndef FOOTPRINTDATA_H
#define FOOTPRINTDATA_H

#include "Model3DData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter {

// ==================== 封装信息 ====================

struct FootprintInfo {
    QString name;
    QString type;
    QString model3DName;

    // EasyEDA API 原始字段
    QString uuid;
    QString docType;
    QString datastrid;
    bool writable;
    qint64 updateTime;

    // 编辑器信
    QString editorVersion;

    // 项目信息
    QString puuid;
    qint64 utime;
    bool importFlag;
    bool hasIdFlag;
    bool newgId;

    // 附加参数
    QString link;
    QString contributor;
    QString uuid3d;

    // 画布信息
    QString canvas;

    // 层定位
    QString layers;

    // 对象可见
    QString objects;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 边界====================

struct FootprintBBox {
    double x;
    double y;
    double width;
    double height;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 焊盘 ====================

struct FootprintPad {
    QString shape;
    double centerX;
    double centerY;
    double width;
    double height;
    int layerId;
    QString net;
    QString number;
    double holeRadius;
    QString points;
    double rotation;
    QString id;
    double holeLength;
    QString holePoint;
    bool isPlated;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 走线 ====================

struct FootprintTrack {
    double strokeWidth;
    int layerId;
    QString net;
    QString points;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 圆弧 ====================

struct FootprintHole {
    double centerX;
    double centerY;
    double radius;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 圆弧 ====================

struct FootprintCircle {
    double cx;
    double cy;
    double radius;
    double strokeWidth;
    int layerId;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 矩形 ====================

struct FootprintRectangle {
    double x;
    double y;
    double width;
    double height;
    double strokeWidth;
    QString id;
    int layerId;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 圆弧 ====================

struct FootprintArc {
    double strokeWidth;
    int layerId;
    QString net;
    QString path;
    QString helperDots;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 文本 ====================

struct FootprintText {
    QString type;
    double centerX;
    double centerY;
    double strokeWidth;
    int rotation;
    QString mirror;
    int layerId;
    QString net;
    double fontSize;
    QString text;
    QString textPath;
    bool isDisplayed;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 实体填充区域 ====================
/**
 * @brief 实体填充区域，用于禁止布线区或元件占位区
 */
struct FootprintSolidRegion {
    QString path;       // 路径数据（如 "M x y L x y Z"）
    int layerId;        // 所属层（通常ComponentShapeLayer，ID=99）
    QString fillStyle;  // 填充样式（solid, none等）
    QString id;         // 唯一标识
    bool isKeepOut;     // 是否为禁止布线区
    bool isLocked;      // 是否锁定

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 外形轮廓 ====================
/**
 * @brief 器件外形轮廓，用于丝印标识和装配
 */
struct FootprintOutline {
    QString path;        // SVG 路径或多边形点序
    int layerId;         // 所属层（通常TopSilkLayer 或 3DModel 层）
    double strokeWidth;  // 线宽
    QString id;          // 唯一标识
    bool isLocked;       // 是否锁定

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 层定位====================
/**
 * @brief PCB 层定义信
     */
struct LayerDefinition {
    int layerId;                  // 层 ID
    QString name;                 // 层名
    QString color;                // 层颜色（#RRGGBB）
    bool isVisible;               // 是否可见
    bool isUsedForManufacturing;  // 是否用于制
    double expansion;             // 扩展值（如阻焊层扩展

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 对象可见性配====================
/**
 * @brief 对象类型可见性配
     */
struct ObjectVisibility {
    QString objectType;  // 对象类型（Pad, Track, Text等）
    bool isEnabled;      // 是否启用
    bool isVisible;      // 是否可见

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 封装数据 ====================

class FootprintData {
public:
    FootprintData();
    ~FootprintData() = default;

    // Getter 和 Setter 方法
    FootprintInfo info() const {
        return m_info;
    }
    void setInfo(const FootprintInfo& info) {
        m_info = info;
    }

    FootprintBBox bbox() const {
        return m_bbox;
    }
    void setBbox(const FootprintBBox& bbox) {
        m_bbox = bbox;
    }

    QList<FootprintPad> pads() const {
        return m_pads;
    }
    void setPads(const QList<FootprintPad>& pads) {
        m_pads = pads;
    }
    void addPad(const FootprintPad& pad) {
        m_pads.append(pad);
    }

    QList<FootprintTrack> tracks() const {
        return m_tracks;
    }
    void setTracks(const QList<FootprintTrack>& tracks) {
        m_tracks = tracks;
    }
    void addTrack(const FootprintTrack& track) {
        m_tracks.append(track);
    }

    QList<FootprintHole> holes() const {
        return m_holes;
    }
    void setHoles(const QList<FootprintHole>& holes) {
        m_holes = holes;
    }
    void addHole(const FootprintHole& hole) {
        m_holes.append(hole);
    }

    QList<FootprintCircle> circles() const {
        return m_circles;
    }
    void setCircles(const QList<FootprintCircle>& circles) {
        m_circles = circles;
    }
    void addCircle(const FootprintCircle& circle) {
        m_circles.append(circle);
    }

    QList<FootprintRectangle> rectangles() const {
        return m_rectangles;
    }
    void setRectangles(const QList<FootprintRectangle>& rectangles) {
        m_rectangles = rectangles;
    }
    void addRectangle(const FootprintRectangle& rect) {
        m_rectangles.append(rect);
    }

    QList<FootprintArc> arcs() const {
        return m_arcs;
    }
    void setArcs(const QList<FootprintArc>& arcs) {
        m_arcs = arcs;
    }
    void addArc(const FootprintArc& arc) {
        m_arcs.append(arc);
    }

    QList<FootprintText> texts() const {
        return m_texts;
    }
    void setTexts(const QList<FootprintText>& texts) {
        m_texts = texts;
    }
    void addText(const FootprintText& text) {
        m_texts.append(text);
    }

    QList<FootprintSolidRegion> solidRegions() const {
        return m_solidRegions;
    }
    void setSolidRegions(const QList<FootprintSolidRegion>& solidRegions) {
        m_solidRegions = solidRegions;
    }
    void addSolidRegion(const FootprintSolidRegion& solidRegion) {
        m_solidRegions.append(solidRegion);
    }

    QList<FootprintOutline> outlines() const {
        return m_outlines;
    }
    void setOutlines(const QList<FootprintOutline>& outlines) {
        m_outlines = outlines;
    }
    void addOutline(const FootprintOutline& outline) {
        m_outlines.append(outline);
    }

    QList<LayerDefinition> layers() const {
        return m_layers;
    }
    void setLayers(const QList<LayerDefinition>& layers) {
        m_layers = layers;
    }
    void addLayer(const LayerDefinition& layer) {
        m_layers.append(layer);
    }

    QList<ObjectVisibility> objectVisibilities() const {
        return m_objectVisibilities;
    }
    void setObjectVisibilities(const QList<ObjectVisibility>& objectVisibilities) {
        m_objectVisibilities = objectVisibilities;
    }
    void addObjectVisibility(const ObjectVisibility& objectVisibility) {
        m_objectVisibilities.append(objectVisibility);
    }

    Model3DData model3D() const {
        return m_model3D;
    }
    void setModel3D(const Model3DData& model3D) {
        m_model3D = model3D;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    // 清空数据
    void clear();

private:
    FootprintInfo m_info;
    FootprintBBox m_bbox;
    QList<FootprintPad> m_pads;
    QList<FootprintTrack> m_tracks;
    QList<FootprintHole> m_holes;
    QList<FootprintCircle> m_circles;
    QList<FootprintRectangle> m_rectangles;
    QList<FootprintArc> m_arcs;
    QList<FootprintText> m_texts;
    QList<FootprintSolidRegion> m_solidRegions;
    QList<FootprintOutline> m_outlines;
    QList<LayerDefinition> m_layers;
    QList<ObjectVisibility> m_objectVisibilities;
    Model3DData m_model3D;
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTDATA_H
