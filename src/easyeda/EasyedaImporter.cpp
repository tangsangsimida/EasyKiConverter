#include "EasyedaImporter.h"
#include <iostream>
#include <sstream>

EasyedaSymbolImporter::EasyedaSymbolImporter(const json& easyeda_cp_cad_data) 
    : input_data(easyeda_cp_cad_data) {
    // 检查是否有dataStr字段并且是字符串类型
    if (easyeda_cp_cad_data.contains("dataStr") && easyeda_cp_cad_data["dataStr"].is_string()) {
        try {
            // 解析dataStr中的JSON数据
            std::string dataStr = easyeda_cp_cad_data["dataStr"];
            json dataStrJson = json::parse(dataStr);
            
            // 获取c_para信息
            if (dataStrJson.contains("head") && dataStrJson["head"].contains("c_para")) {
                json c_para = dataStrJson["head"]["c_para"];
                output_symbol = extract_easyeda_data(dataStrJson, c_para);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing dataStr: " << e.what() << std::endl;
        }
    }
}

EeSymbol EasyedaSymbolImporter::get_symbol() const {
    return output_symbol;
}

EeSymbol EasyedaSymbolImporter::extract_easyeda_data(const json& ee_data, const json& ee_data_info) {
    EeSymbol new_ee_symbol;
    
    // 提取符号信息
    if (ee_data_info.contains("name")) {
        new_ee_symbol.info.name = ee_data_info["name"];
    }
    
    if (ee_data_info.contains("pre")) {
        new_ee_symbol.info.prefix = ee_data_info["pre"];
    }
    
    if (ee_data_info.contains("package")) {
        new_ee_symbol.info.package = ee_data_info["package"];
    }
    
    if (ee_data_info.contains("BOM_Manufacturer")) {
        new_ee_symbol.info.manufacturer = ee_data_info["BOM_Manufacturer"];
    }
    
    // 提取边界框信息
    if (ee_data.contains("head")) {
        if (ee_data["head"].contains("x")) {
            new_ee_symbol.bbox.x = std::to_string(ee_data["head"]["x"].get<int>());
        }
        
        if (ee_data["head"].contains("y")) {
            new_ee_symbol.bbox.y = std::to_string(ee_data["head"]["y"].get<int>());
        }
    }
    
    // 处理形状数据
    if (ee_data.contains("shape") && ee_data["shape"].is_array()) {
        for (const auto& line : ee_data["shape"]) {
            if (line.is_string()) {
                std::string line_str = line;
                std::vector<std::string> parts = split_string(line_str, '~');
                
                if (!parts.empty()) {
                    std::string designator = parts[0];
                    
                    if (designator == "P") {
                        add_easyeda_pin(line_str, new_ee_symbol);
                    } else if (designator == "R") {
                        add_easyeda_rectangle(line_str, new_ee_symbol);
                    } else if (designator == "E") {
                        add_easyeda_ellipse(line_str, new_ee_symbol);
                    } else if (designator == "C") {
                        add_easyeda_circle(line_str, new_ee_symbol);
                    } else if (designator == "A") {
                        add_easyeda_arc(line_str, new_ee_symbol);
                    } else if (designator == "PL") {
                        add_easyeda_polyline(line_str, new_ee_symbol);
                    } else if (designator == "PG") {
                        add_easyeda_polygon(line_str, new_ee_symbol);
                    } else if (designator == "PT") {
                        add_easyeda_path(line_str, new_ee_symbol);
                    }
                }
            }
        }
    }
    
    return new_ee_symbol;
}

void EasyedaSymbolImporter::add_easyeda_pin(const std::string& pin_data, EeSymbol& ee_symbol) {
    std::vector<std::vector<std::string>> segments = split_segments(pin_data);
    
    if (segments.size() >= 7) {
        EeSymbolPin pin;
        
        // 设置引脚设置信息
        if (segments[0].size() >= 6) {
            pin.settings.spice_pin_number = segments[0][1];
            // 类型需要转换为枚举值
            if (segments[0][2] == "0") {
                pin.settings.type = EasyedaPinType::_input;
            } else if (segments[0][2] == "1") {
                pin.settings.type = EasyedaPinType::output;
            } else if (segments[0][2] == "2") {
                pin.settings.type = EasyedaPinType::bidirectional;
            } else if (segments[0][2] == "3") {
                pin.settings.type = EasyedaPinType::power;
            } else {
                pin.settings.type = EasyedaPinType::unspecified;
            }
            pin.settings.rotation = segments[0][3];
            pin.settings.pos_x = segments[0][4];
            pin.settings.pos_y = segments[0][5];
        }
        
        // 设置引脚路径信息
        if (segments[2].size() >= 1) {
            pin.pin_path.path = segments[2][0];
        }
        
        // 设置引脚名称信息
        if (segments[3].size() >= 1) {
            pin.name.text = segments[3][0];
        }
        
        // 设置引脚圆点信息（dot字段）
        if (segments[5].size() >= 1) {
            pin.dot.is_displayed = (segments[5][0] == "1");
        }
        
        // 设置引脚时钟信息
        if (segments[6].size() >= 1) {
            pin.clock.is_displayed = (segments[6][0] == "1");
        }
        
        ee_symbol.pins.push_back(pin);
    }
}

void EasyedaSymbolImporter::add_easyeda_rectangle(const std::string& rectangle_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(rectangle_data, '~');
    
    if (parts.size() >= 10) {
        EeSymbolRectangle rectangle;
        rectangle.x = parts[1];
        rectangle.y = parts[2];
        rectangle.width = parts[3];
        rectangle.height = parts[4];
        rectangle.stroke_width = parts[5];
        rectangle.stroke_color = parts[6];
        rectangle.fill_color = parts[7];
        rectangle.id = parts[8];
        rectangle.uuid = parts[9];
        
        ee_symbol.rectangles.push_back(rectangle);
    }
}

void EasyedaSymbolImporter::add_easyeda_polyline(const std::string& polyline_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(polyline_data, '~');
    
    if (parts.size() >= 7) {
        EeSymbolPolyline polyline;
        polyline.points = parts[1];
        polyline.stroke_width = parts[2];
        polyline.stroke_color = parts[3];
        polyline.fill_color = parts[4];
        polyline.id = parts[5];
        polyline.uuid = parts[6];
        
        ee_symbol.polylines.push_back(polyline);
    }
}

void EasyedaSymbolImporter::add_easyeda_polygon(const std::string& polygon_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(polygon_data, '~');
    
    if (parts.size() >= 7) {
        EeSymbolPolygon polygon;
        polygon.points = parts[1];
        polygon.stroke_width = parts[2];
        polygon.stroke_color = parts[3];
        polygon.fill_color = parts[4];
        polygon.id = parts[5];
        polygon.uuid = parts[6];
        
        ee_symbol.polygons.push_back(polygon);
    }
}

void EasyedaSymbolImporter::add_easyeda_path(const std::string& path_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(path_data, '~');
    
    if (parts.size() >= 7) {
        EeSymbolPath path;
        path.d = parts[1];
        path.stroke_width = parts[2];
        path.stroke_color = parts[3];
        path.fill_color = parts[4];
        path.id = parts[5];
        path.uuid = parts[6];
        
        ee_symbol.paths.push_back(path);
    }
}

void EasyedaSymbolImporter::add_easyeda_circle(const std::string& circle_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(circle_data, '~');
    
    if (parts.size() >= 9) {
        EeSymbolCircle circle;
        circle.cx = parts[1];
        circle.cy = parts[2];
        circle.r = parts[3];
        circle.stroke_width = parts[4];
        circle.stroke_color = parts[5];
        circle.fill_color = parts[6];
        circle.id = parts[7];
        circle.uuid = parts[8];
        
        ee_symbol.circles.push_back(circle);
    }
}

void EasyedaSymbolImporter::add_easyeda_ellipse(const std::string& ellipse_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(ellipse_data, '~');
    
    if (parts.size() >= 10) {
        EeSymbolEllipse ellipse;
        ellipse.cx = parts[1];
        ellipse.cy = parts[2];
        ellipse.rx = parts[3];
        ellipse.ry = parts[4];
        ellipse.stroke_width = parts[5];
        ellipse.stroke_color = parts[6];
        ellipse.fill_color = parts[7];
        ellipse.id = parts[8];
        ellipse.uuid = parts[9];
        
        ee_symbol.ellipses.push_back(ellipse);
    }
}

void EasyedaSymbolImporter::add_easyeda_arc(const std::string& arc_data, EeSymbol& ee_symbol) {
    std::vector<std::string> parts = split_string(arc_data, '~');
    
    if (parts.size() >= 7) {
        EeSymbolArc arc;
        arc.path = parts[1];
        arc.stroke_width = parts[2];
        arc.stroke_color = parts[3];
        arc.fill_color = parts[4];
        arc.id = parts[5];
        arc.uuid = parts[6];
        
        ee_symbol.arcs.push_back(arc);
    }
}

std::vector<std::string> EasyedaSymbolImporter::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    
    return result;
}

std::vector<std::vector<std::string>> EasyedaSymbolImporter::split_segments(const std::string& str) {
    std::vector<std::vector<std::string>> result;
    std::vector<std::string> segments = split_string(str, '^');
    
    for (const auto& segment : segments) {
        if (!segment.empty() && segment[0] == '^') {
            // Skip the first '^' character
            result.push_back(split_string(segment.substr(1), '~'));
        } else {
            result.push_back(split_string(segment, '~'));
        }
    }
    
    return result;
}