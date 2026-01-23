#ifndef SYMBOLDATA_H
#define SYMBOLDATA_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QPointF>
#include <QRectF>

namespace EasyKiConverter
{

    // ==================== 枚举类型 ====================

    /**
     * @brief 引脚类型枚举
     */
    enum class PinType
    {
        Unspecified = 0,
        Input = 1,
        Output = 2,
        Bidirectional = 3,
        Power = 4
    };

    /**
     * @brief 引脚样式枚举
     */
    enum class PinStyle
    {
        Line,
        Inverted,
        Clock,
        InvertedClock,
        InputLow,
        ClockLow,
        OutputLow,
        EdgeClockHigh,
        NonLogic
    };

    // ==================== 符号信息 ====================

    /**
     * @brief 符号信息
     */
    struct SymbolInfo
    {
        QString name;
        QString prefix;
        QString package;
        QString manufacturer;
        QString description;
        QString datasheet;
        QString lcscId;
        QString jlcId;

        // EasyEDA API 原始字段
        QString uuid;
        QString title;
        QString docType;
        QString type;
        QString thumb;
        QString datastrid;
        bool jlcOnSale;
        bool writable;
        bool isFavorite;
        bool verify;
        bool smt;

        // 时间�?
        qint64 updateTime;
        QString updatedAt;

        // 编辑器信�?
        QString editorVersion;

        // 项目信息
        QString puuid;
        qint64 utime;
        bool importFlag;
        bool hasIdFlag;

        // 附加参数
        QString timeStamp;
        QString subpartNo;
        QString supplierPart;
        QString supplier;
        QString manufacturerPart;
        QString jlcpcbPartClass;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    // ==================== 边界�?====================

    /**
     * @brief 边界�?
     */
    struct SymbolBBox
    {
        double x;
        double y;
        double width;
        double height;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    // ==================== 引脚相关数据结构 ====================

    /**
     * @brief 引脚设置
     */
    struct SymbolPinSettings
    {
        bool isDisplayed;
        PinType type;
        QString spicePinNumber;
        double posX;
        double posY;
        int rotation;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 引脚�?
     */
    struct SymbolPinDot
    {
        double dotX;
        double dotY;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 引脚路径
     */
    struct SymbolPinPath
    {
        QString path;
        QString color;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 引脚名称
     */
    struct SymbolPinName
    {
        bool isDisplayed;
        double posX;
        double posY;
        int rotation;
        QString text;
        QString textAnchor;
        QString font;
        double fontSize;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 引脚圆点（第二个�?
     */
    struct SymbolPinDotBis
    {
        bool isDisplayed;
        double circleX;
        double circleY;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 引脚时钟
     */
    struct SymbolPinClock
    {
        bool isDisplayed;
        QString path;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 引脚
     */
    struct SymbolPin
    {
        SymbolPinSettings settings;
        SymbolPinDot pinDot;
        SymbolPinPath pinPath;
        SymbolPinName name;
        SymbolPinDotBis dot;
        SymbolPinClock clock;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    // ==================== 图形元素 ====================

    /**
     * @brief 矩形
     */
    struct SymbolRectangle
    {
        double posX;
        double posY;
        double rx;
        double ry;
        double width;
        double height;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        QString fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief �?
     */
    struct SymbolCircle
    {
        double centerX;
        double centerY;
        double radius;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        bool fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 圆弧
     */
    struct SymbolArc
    {
        QList<QPointF> path;
        QString helperDots;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        bool fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 椭圆
     */
    struct SymbolEllipse
    {
        double centerX;
        double centerY;
        double radiusX;
        double radiusY;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        bool fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 多段�?
     */
    struct SymbolPolyline
    {
        QString points;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        bool fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 多边�?
     */
    struct SymbolPolygon
    {
        QString points;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        bool fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 路径
     */
    struct SymbolPath
    {
        QString paths;
        QString strokeColor;
        double strokeWidth;
        QString strokeStyle;
        bool fillColor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    /**
     * @brief 文本
     */
    struct SymbolText
    {
        QString mark;
        double posX;
        double posY;
        double rotation;
        QString color;
        QString font;
        double textSize;
        bool bold;
        QString italic;
        QString baseline;
        QString type;
        QString text;
        bool visible;
        QString anchor;
        QString id;
        bool isLocked;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    // ==================== 符号部分 ====================

    /**
     * @brief 符号部分（用于多部分符号�?
     */
    struct SymbolPart
    {
        int unitNumber; // 部分编号（从 0 开始）
        QList<SymbolPin> pins;
        QList<SymbolRectangle> rectangles;
        QList<SymbolCircle> circles;
        QList<SymbolArc> arcs;
        QList<SymbolEllipse> ellipses;
        QList<SymbolPolyline> polylines;
        QList<SymbolPolygon> polygons;
        QList<SymbolPath> paths;
        QList<SymbolText> texts;

        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);
    };

    // ==================== 符号数据 ====================

    /**
     * @brief 符号数据�?
     *
     * 包含符号的所有信息，包括引脚和各种图形元�?
     */
    class SymbolData
    {
    public:
        SymbolData();
        ~SymbolData() = default;

        // Getter �?Setter 方法
        SymbolInfo info() const { return m_info; }
        void setInfo(const SymbolInfo &info) { m_info = info; }

        SymbolBBox bbox() const { return m_bbox; }
        void setBbox(const SymbolBBox &bbox) { m_bbox = bbox; }

        // 单部分符号的兼容接口（向后兼容）
        QList<SymbolPin> pins() const { return m_pins; }
        void setPins(const QList<SymbolPin> &pins) { m_pins = pins; }
        void addPin(const SymbolPin &pin) { m_pins.append(pin); }

        QList<SymbolRectangle> rectangles() const { return m_rectangles; }
        void setRectangles(const QList<SymbolRectangle> &rectangles) { m_rectangles = rectangles; }
        void addRectangle(const SymbolRectangle &rect) { m_rectangles.append(rect); }

        QList<SymbolCircle> circles() const { return m_circles; }
        void setCircles(const QList<SymbolCircle> &circles) { m_circles = circles; }
        void addCircle(const SymbolCircle &circle) { m_circles.append(circle); }

        QList<SymbolArc> arcs() const { return m_arcs; }
        void setArcs(const QList<SymbolArc> &arcs) { m_arcs = arcs; }
        void addArc(const SymbolArc &arc) { m_arcs.append(arc); }

        QList<SymbolEllipse> ellipses() const { return m_ellipses; }
        void setEllipses(const QList<SymbolEllipse> &ellipses) { m_ellipses = ellipses; }
        void addEllipse(const SymbolEllipse &ellipse) { m_ellipses.append(ellipse); }

        QList<SymbolPolyline> polylines() const { return m_polylines; }
        void setPolylines(const QList<SymbolPolyline> &polylines) { m_polylines = polylines; }
        void addPolyline(const SymbolPolyline &polyline) { m_polylines.append(polyline); }

        QList<SymbolPolygon> polygons() const { return m_polygons; }
        void setPolygons(const QList<SymbolPolygon> &polygons) { m_polygons = polygons; }
        void addPolygon(const SymbolPolygon &polygon) { m_polygons.append(polygon); }

        QList<SymbolPath> paths() const { return m_paths; }
        void setPaths(const QList<SymbolPath> &paths) { m_paths = paths; }
        void addPath(const SymbolPath &path) { m_paths.append(path); }

        QList<SymbolText> texts() const { return m_texts; }
        void setTexts(const QList<SymbolText> &texts) { m_texts = texts; }
        void addText(const SymbolText &text) { m_texts.append(text); }

        // 多部分符号接�?
        QList<SymbolPart> parts() const { return m_parts; }
        void setParts(const QList<SymbolPart> &parts) { m_parts = parts; }
        void addPart(const SymbolPart &part) { m_parts.append(part); }
        bool isMultiPart() const { return m_parts.size() > 1; }

        // JSON 序列�?
        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);

        // 数据验证
        bool isValid() const;
        QString validate() const;

        // 清空数据
        void clear();

    private:
        SymbolInfo m_info;
        SymbolBBox m_bbox;
        QList<SymbolPin> m_pins; // 单部分符号的引脚（向后兼容）
        QList<SymbolRectangle> m_rectangles;
        QList<SymbolCircle> m_circles;
        QList<SymbolArc> m_arcs;
        QList<SymbolEllipse> m_ellipses;
        QList<SymbolPolyline> m_polylines;
        QList<SymbolPolygon> m_polygons;
        QList<SymbolPath> m_paths;
        QList<SymbolText> m_texts;
        QList<SymbolPart> m_parts; // 多部分符号的部分列表
    };

} // namespace EasyKiConverter

#endif // SYMBOLDATA_H
