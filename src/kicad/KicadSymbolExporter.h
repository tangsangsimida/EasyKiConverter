#ifndef EASYKI_CONVERTER_KICAD_SYMBOL_EXPORTER_H
#define EASYKI_CONVERTER_KICAD_SYMBOL_EXPORTER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;

// KiCad版本枚举
enum class KicadVersion {
    v5,
    v6
};

// EasyEDA引脚类型枚举
enum class EasyedaPinType {
    unspecified,
    _input,
    output,
    bidirectional,
    power
};

// KiCad引脚类型枚举
enum class KiPinType {
    unspecified,
    _input,
    output,
    bidirectional,
    power_in
};

// KiCad引脚样式枚举
enum class KiPinStyle {
    line,
    inverted,
    clock,
    inverted_clock
};

// EasyEDA符号引脚结构
struct EeSymbolPinSettings {
    std::string spice_pin_number;
    EasyedaPinType type;
    std::string rotation;
    std::string pos_x;
    std::string pos_y;
};

struct EeSymbolPinPath {
    std::string path;
};

struct EeSymbolPinDot {
    bool is_displayed;
};

struct EeSymbolPinClock {
    bool is_displayed;
};

struct EeSymbolPinName {
    std::string text;
};

struct EeSymbolPin {
    EeSymbolPinSettings settings;
    EeSymbolPinPath pin_path;
    EeSymbolPinDot dot;
    EeSymbolPinClock clock;
    EeSymbolPinName name;
};

// EasyEDA符号边界框结构
struct EeSymbolBbox {
    std::string x;
    std::string y;
};

// EasyEDA符号信息结构
struct EeSymbolInfo {
    std::string name;
    std::string prefix;
    std::string package;
    std::string manufacturer;
    std::string datasheet;
    std::string lcsc_id;
    std::string jlc_id;
};

// EasyEDA符号矩形结构
struct EeSymbolRectangle {
    std::string x;
    std::string y;
    std::string width;
    std::string height;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

// EasyEDA符号椭圆结构
struct EeSymbolEllipse {
    std::string cx;
    std::string cy;
    std::string rx;
    std::string ry;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

// EasyEDA符号圆结构
struct EeSymbolCircle {
    std::string cx;
    std::string cy;
    std::string r;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

// EasyEDA符号弧结构
struct EeSymbolArc {
    std::string path;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

// EasyEDA符号多段线结构
struct EeSymbolPolyline {
    std::string points;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

// EasyEDA符号多边形结构
struct EeSymbolPolygon {
    std::string points;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

// EasyEDA符号路径结构
struct EeSymbolPath {
    std::string d;
    std::string stroke_width;
    std::string stroke_color;
    std::string fill_color;
    std::string id;
    std::string uuid;
};

struct EeSymbol {
    EeSymbolInfo info;
    EeSymbolBbox bbox;
    std::vector<EeSymbolPin> pins;
    std::vector<EeSymbolRectangle> rectangles;
    std::vector<EeSymbolEllipse> ellipses;
    std::vector<EeSymbolCircle> circles;
    std::vector<EeSymbolArc> arcs;
    std::vector<EeSymbolPolyline> polylines;
    std::vector<EeSymbolPolygon> polygons;
    std::vector<EeSymbolPath> paths;
};

// KiCad符号引脚结构
struct KiSymbolPin {
    std::string name;
    std::string number;
    KiPinStyle style;
    int length;
    KiPinType type;
    std::string orientation;
    int pos_x;
    int pos_y;
};

// KiCad符号矩形结构
struct KiSymbolRectangle {
    int pos_x0;
    int pos_y0;
    int pos_x1;
    int pos_y1;
};

// KiCad符号圆结构
struct KiSymbolCircle {
    int pos_x;
    int pos_y;
    int radius;
    std::string background_filling;
};

// KiCad符号弧结构
struct KiSymbolArc {
    int radius;
    int angle_start;
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int center_x;
    int center_y;
    int angle_end;
    int middle_x;
    int middle_y;
};

// KiCad符号多边形结构
struct KiSymbolPolygon {
    std::vector<std::vector<int>> points;
    int points_number;
    bool is_closed;
};

/**
 * @brief KiCad符号导出器类
 * KiCad symbol exporter class
 */
class KicadSymbolExporter {
public:
    /**
     * @brief 构造函数，初始化KiCad版本
     * Constructor, initialize KiCad version
     */
    KicadSymbolExporter(KicadVersion version);

    /**
     * @brief 将EasyEDA引脚转换为KiCad符号引脚
     * Convert EasyEDA pins to KiCad symbol pins
     */
    std::vector<KiSymbolPin> convert_ee_pins(const std::vector<EeSymbolPin>& ee_pins, 
                                             const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA矩形转换为KiCad符号矩形
     * Convert EasyEDA rectangles to KiCad symbol rectangles
     */
    std::vector<KiSymbolRectangle> convert_ee_rectangles(const std::vector<json>& ee_rectangles,
                                                         const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA圆形转换为KiCad符号圆形
     * Convert EasyEDA circles to KiCad symbol circles
     */
    std::vector<KiSymbolCircle> convert_ee_circles(const std::vector<json>& ee_circles,
                                                   const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA椭圆转换为KiCad符号圆形（仅支持圆）
     * Convert EasyEDA ellipses to KiCad symbol circles (only supports circles)
     */
    std::vector<KiSymbolCircle> convert_ee_ellipses(const std::vector<json>& ee_ellipses,
                                                    const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA弧线转换为KiCad符号弧线
     * Convert EasyEDA arcs to KiCad symbol arcs
     */
    std::vector<KiSymbolArc> convert_ee_arcs(const std::vector<json>& ee_arcs,
                                             const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA折线转换为KiCad符号多边形
     * Convert EasyEDA polylines to KiCad symbol polygons
     */
    std::vector<KiSymbolPolygon> convert_ee_polylines(const std::vector<json>& ee_polylines,
                                                      const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA多边形转换为KiCad符号多边形
     * Convert EasyEDA polygons to KiCad symbol polygons
     */
    std::vector<KiSymbolPolygon> convert_ee_polygons(const std::vector<json>& ee_polygons,
                                                     const EeSymbolBbox& ee_bbox);

    /**
     * @brief 将EasyEDA路径转换为KiCad符号多边形和贝塞尔曲线
     * Convert EasyEDA paths to KiCad symbol polygons and bezier curves
     */
    std::pair<std::vector<KiSymbolPolygon>, std::vector<KiSymbolPolygon>> 
    convert_ee_paths(const std::vector<json>& ee_paths,
                     const EeSymbolBbox& ee_bbox);

    /**
     * @brief 导出符号到KiCad文件
     * Export symbol to KiCad file
     */
    bool exportSymbol(const json& easyeda_data, const std::string& file_path);
    
    /**
     * @brief 导出符号到KiCad库文件
     * Export symbol to KiCad library file with proper directory structure
     */
    bool exportSymbolToLibrary(const json& easyeda_data, const std::string& export_path, 
                              const std::string& library_name);

private:
    KicadVersion kicad_version; ///< KiCad版本 / KiCad version

    /**
     * @brief 像素转密耳（千分之一英寸）
     * Convert pixels to mils (thousandths of an inch)
     */
    int px_to_mil(double dim);

    /**
     * @brief 像素转毫米
     * Convert pixels to millimeters
     */
    double px_to_mm(double dim);
    
    /**
     * @brief 确保目录存在
     * Ensure directory exists
     */
    bool ensureDirectoryExists(const std::string& path);
};

#endif // EASYKI_CONVERTER_KICAD_SYMBOL_EXPORTER_H