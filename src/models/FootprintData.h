#ifndef FOOTPRINTDATA_H
#define FOOTPRINTDATA_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QPointF>
#include "Model3DData.h"

namespace EasyKiConverter {

// ==================== 封装信息 ====================

struct FootprintInfo {
    QString name;
    QString type;
    QString model3DName;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
};

// ==================== 边界框 ====================

struct FootprintBBox {
    double x;
    double y;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
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
    bool fromJson(const QJsonObject &json);
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
    bool fromJson(const QJsonObject &json);
};

// ==================== 孔 ====================

struct FootprintHole {
    double centerX;
    double centerY;
    double radius;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
};

// ==================== 圆 ====================

struct FootprintCircle {
    double cx;
    double cy;
    double radius;
    double strokeWidth;
    int layerId;
    QString id;
    bool isLocked;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
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
    bool fromJson(const QJsonObject &json);
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
    bool fromJson(const QJsonObject &json);
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
    bool fromJson(const QJsonObject &json);
};

// ==================== 封装数据 ====================

class FootprintData
{
public:
    FootprintData();
    ~FootprintData() = default;

    // Getter 和 Setter 方法
    FootprintInfo info() const { return m_info; }
    void setInfo(const FootprintInfo &info) { m_info = info; }

    FootprintBBox bbox() const { return m_bbox; }
    void setBbox(const FootprintBBox &bbox) { m_bbox = bbox; }

    QList<FootprintPad> pads() const { return m_pads; }
    void setPads(const QList<FootprintPad> &pads) { m_pads = pads; }
    void addPad(const FootprintPad &pad) { m_pads.append(pad); }

    QList<FootprintTrack> tracks() const { return m_tracks; }
    void setTracks(const QList<FootprintTrack> &tracks) { m_tracks = tracks; }
    void addTrack(const FootprintTrack &track) { m_tracks.append(track); }

    QList<FootprintHole> holes() const { return m_holes; }
    void setHoles(const QList<FootprintHole> &holes) { m_holes = holes; }
    void addHole(const FootprintHole &hole) { m_holes.append(hole); }

    QList<FootprintCircle> circles() const { return m_circles; }
    void setCircles(const QList<FootprintCircle> &circles) { m_circles = circles; }
    void addCircle(const FootprintCircle &circle) { m_circles.append(circle); }

    QList<FootprintRectangle> rectangles() const { return m_rectangles; }
    void setRectangles(const QList<FootprintRectangle> &rectangles) { m_rectangles = rectangles; }
    void addRectangle(const FootprintRectangle &rect) { m_rectangles.append(rect); }

    QList<FootprintArc> arcs() const { return m_arcs; }
    void setArcs(const QList<FootprintArc> &arcs) { m_arcs = arcs; }
    void addArc(const FootprintArc &arc) { m_arcs.append(arc); }

    QList<FootprintText> texts() const { return m_texts; }
    void setTexts(const QList<FootprintText> &texts) { m_texts = texts; }
    void addText(const FootprintText &text) { m_texts.append(text); }

    Model3DData model3D() const { return m_model3D; }
    void setModel3D(const Model3DData &model3D) { m_model3D = model3D; }

    // JSON 序列化
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);

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
    Model3DData m_model3D;
};

} // namespace EasyKiConverter

#endif // FOOTPRINTDATA_H