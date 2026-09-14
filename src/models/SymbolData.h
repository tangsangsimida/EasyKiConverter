#ifndef SYMBOLDATA_H
#define SYMBOLDATA_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

// ==================== 枚举类型 ====================

/**
 * @brief 引脚类型枚举
 */
enum class PinType { Unspecified = 0, Input = 1, Output = 2, Bidirectional = 3, Power = 4 };

/**
 * @brief 引脚样式枚举
 */
enum class PinStyle { Line, Inverted, Clock, InvertedClock, InputLow, ClockLow, OutputLow, EdgeClockHigh, NonLogic };

// ==================== 符号信息 ====================

/**
 * @brief 符号信息
 */
struct SymbolInfo {
    QString name;
    QString prefix;
    QString package;
    QString manufacturer;
    QString description;
    QString datasheet;
    QString lcscId;
    QString jlcId;
    QStringList aliases;

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

    // 时间
    qint64 updateTime;
    QString updatedAt;

    // 编辑器信
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
};

// ==================== 边界====================

/**
 * @brief 边界
 */
struct SymbolBBox {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double headX = 0.0;
    double headY = 0.0;
    bool hasHeadCenter = false;
};

// ==================== 引脚相关数据结构 ====================

/**
 * @brief 引脚设置
 */
struct SymbolPinSettings {
    bool isDisplayed = true;
    PinType type = PinType::Unspecified;
    QString spicePinNumber;
    double posX = 0.0;
    double posY = 0.0;
    int rotation = 0;
    QString id;
    bool isLocked = false;
};

/**
 * @brief 引脚
 */
struct SymbolPinDot {
    double dotX = 0.0;
    double dotY = 0.0;
};

/**
 * @brief 引脚路径
 */
struct SymbolPinPath {
    QString path;
    QString color;
};

/**
 * @brief 引脚名称
 */
struct SymbolPinName {
    bool isDisplayed = false;
    double posX = 0.0;
    double posY = 0.0;
    int rotation = 0;
    QString text;
    QString textAnchor;
    QString font;
    double fontSize = 7.0;
};

/**
 * @brief 引脚圆点（第二个
 */
struct SymbolPinDotBis {
    bool isDisplayed = false;
    double circleX = 0.0;
    double circleY = 0.0;
};

/**
 * @brief 引脚时钟
 */
struct SymbolPinClock {
    bool isDisplayed = false;
    QString path;
};

/**
 * @brief 引脚
 */
struct SymbolPin {
    SymbolPinSettings settings;
    SymbolPinDot pinDot;
    SymbolPinPath pinPath;
    SymbolPinName name;
    SymbolPinName number;  ///< 引脚编号的原始文本几何属性
    SymbolPinDotBis dot;
    SymbolPinClock clock;
};

// ==================== 图形元素 ====================

/**
 * @brief 矩形
 */
struct SymbolRectangle {
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
};

/**
 * @brief 符号部分数据
 */
struct SymbolCircle {
    double centerX;
    double centerY;
    double radius;
    QString strokeColor;
    double strokeWidth;
    QString strokeStyle;
    bool fillColor;
    QString id;
    bool isLocked;
};

/**
 * @brief 圆弧
 */
struct SymbolArc {
    QList<QPointF> path;
    QString helperDots;
    QString strokeColor;
    double strokeWidth;
    QString strokeStyle;
    bool fillColor;
    QString id;
    bool isLocked;
};

/**
 * @brief 椭圆
 */
struct SymbolEllipse {
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
};

/**
 * @brief 多段
 */
struct SymbolPolyline {
    QString points;
    QString strokeColor;
    double strokeWidth;
    QString strokeStyle;
    bool fillColor;
    QString id;
    bool isLocked;
};

/**
 * @brief 多边
 */
struct SymbolPolygon {
    QString points;
    QString strokeColor;
    double strokeWidth;
    QString strokeStyle;
    bool fillColor;
    QString id;
    bool isLocked;
};

/**
 * @brief 路径
 */
struct SymbolPath {
    QString paths;
    QString strokeColor;
    double strokeWidth;
    QString strokeStyle;
    bool fillColor;
    QString id;
    bool isLocked;
};

/**
 * @brief 文本
 */
struct SymbolText {
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
};

/**
 * @brief 符号图元在源数据中的绘制顺序引用
 * @details type 使用 EasyEDA shape designator（如 P、R、C、T），index 指向对应类型列表中的元素。
 */
struct SymbolGraphicOrder {
    QString type;
    int index = -1;
};

// ==================== 符号部分 ====================

/**
 * @brief 符号部分（用于多部分符号）
 */
struct SymbolPart {
    int unitNumber = 0;  // 部分编号（从 0 开始）
    double originX = 0.0;  // 子部分的坐标原点 X（从 EasyEDA head.x）
    double originY = 0.0;  // 子部分的坐标原点 Y（从 EasyEDA head.y）
    bool commonToAllParts = false;  // 是否为公共 Part Zero；序列化键为 common_to_all_parts
    QList<SymbolPin> pins;
    QList<SymbolRectangle> rectangles;
    QList<SymbolCircle> circles;
    QList<SymbolArc> arcs;
    QList<SymbolEllipse> ellipses;
    QList<SymbolPolyline> polylines;
    QList<SymbolPolygon> polygons;
    QList<SymbolPath> paths;
    QList<SymbolText> texts;
    QList<SymbolGraphicOrder> graphicOrder;
};

// ==================== 符号数据 ====================

/**
 * @brief 符号数据
 *
 * 包含符号的所有信息，包括引脚和各种图形元
 */
class SymbolData {
public:
    SymbolData();
    ~SymbolData() = default;

    // Getter 和 Setter 方法
    SymbolInfo info() const {
        return m_info;
    }

    void setInfo(const SymbolInfo& info) {
        m_info = info;
    }

    SymbolBBox bbox() const {
        return m_bbox;
    }

    void setBbox(const SymbolBBox& bbox) {
        m_bbox = bbox;
    }

    // 单部分符号的兼容接口（向后兼容）
    QList<SymbolPin> pins() const {
        return m_pins;
    }

    void setPins(const QList<SymbolPin>& pins) {
        m_pins = pins;
    }

    void addPin(const SymbolPin& pin) {
        m_pins.append(pin);
    }

    QList<SymbolRectangle> rectangles() const {
        return m_rectangles;
    }

    void setRectangles(const QList<SymbolRectangle>& rectangles) {
        m_rectangles = rectangles;
    }

    void addRectangle(const SymbolRectangle& rect) {
        m_rectangles.append(rect);
    }

    QList<SymbolCircle> circles() const {
        return m_circles;
    }

    void setCircles(const QList<SymbolCircle>& circles) {
        m_circles = circles;
    }

    void addCircle(const SymbolCircle& circle) {
        m_circles.append(circle);
    }

    QList<SymbolArc> arcs() const {
        return m_arcs;
    }

    void setArcs(const QList<SymbolArc>& arcs) {
        m_arcs = arcs;
    }

    void addArc(const SymbolArc& arc) {
        m_arcs.append(arc);
    }

    QList<SymbolEllipse> ellipses() const {
        return m_ellipses;
    }

    void setEllipses(const QList<SymbolEllipse>& ellipses) {
        m_ellipses = ellipses;
    }

    void addEllipse(const SymbolEllipse& ellipse) {
        m_ellipses.append(ellipse);
    }

    QList<SymbolPolyline> polylines() const {
        return m_polylines;
    }

    void setPolylines(const QList<SymbolPolyline>& polylines) {
        m_polylines = polylines;
    }

    void addPolyline(const SymbolPolyline& polyline) {
        m_polylines.append(polyline);
    }

    QList<SymbolPolygon> polygons() const {
        return m_polygons;
    }

    void setPolygons(const QList<SymbolPolygon>& polygons) {
        m_polygons = polygons;
    }

    void addPolygon(const SymbolPolygon& polygon) {
        m_polygons.append(polygon);
    }

    QList<SymbolPath> paths() const {
        return m_paths;
    }

    void setPaths(const QList<SymbolPath>& paths) {
        m_paths = paths;
    }

    void addPath(const SymbolPath& path) {
        m_paths.append(path);
    }

    QList<SymbolText> texts() const {
        return m_texts;
    }

    void setTexts(const QList<SymbolText>& texts) {
        m_texts = texts;
    }

    void addText(const SymbolText& text) {
        m_texts.append(text);
    }

    QList<SymbolGraphicOrder> graphicOrder() const {
        return m_graphicOrder;
    }

    void setGraphicOrder(const QList<SymbolGraphicOrder>& order) {
        m_graphicOrder = order;
    }

    void addGraphicOrder(const SymbolGraphicOrder& order) {
        m_graphicOrder.append(order);
    }

    // 多部分符号接
    QList<SymbolPart> parts() const {
        return m_parts;
    }

    void setParts(const QList<SymbolPart>& parts) {
        m_parts = parts;
    }

    void addPart(const SymbolPart& part) {
        m_parts.append(part);
    }

    bool isMultiPart() const {
        return m_parts.size() > 1;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    /**
     * @brief 返回所有可诊断的符号数据问题
     * @details 与只返回首个错误的 validate() 不同，该接口用于导出前一次性展示所有异常图元。
     * @return 验证错误列表；为空表示未发现问题
     */
    QStringList validationErrors() const;

    // 清空数据
    void clear();

private:
    SymbolInfo m_info{};
    SymbolBBox m_bbox{};
    QList<SymbolPin> m_pins;  // 单部分符号的引脚（向后兼容）
    QList<SymbolRectangle> m_rectangles;
    QList<SymbolCircle> m_circles;
    QList<SymbolArc> m_arcs;
    QList<SymbolEllipse> m_ellipses;
    QList<SymbolPolyline> m_polylines;
    QList<SymbolPolygon> m_polygons;
    QList<SymbolPath> m_paths;
    QList<SymbolText> m_texts;
    QList<SymbolGraphicOrder> m_graphicOrder;
    QList<SymbolPart> m_parts;  // 多部分符号的部分列表
};

}  // namespace EasyKiConverter

#endif  // SYMBOLDATA_H
