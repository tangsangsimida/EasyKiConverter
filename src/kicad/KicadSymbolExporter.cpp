#include "KicadSymbolExporter.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <filesystem>

KicadSymbolExporter::KicadSymbolExporter(KicadVersion version) : kicad_version(version) {}

int KicadSymbolExporter::px_to_mil(double dim) {
    return static_cast<int>(10 * dim);
}

double KicadSymbolExporter::px_to_mm(double dim) {
    return 10.0 * dim * 0.0254;
}

std::vector<KiSymbolPin> KicadSymbolExporter::convert_ee_pins(const std::vector<EeSymbolPin>& ee_pins, 
                                                              const EeSymbolBbox& ee_bbox) {
    std::vector<KiSymbolPin> kicad_pins;
    
    // 根据KiCad版本选择转换函数
    std::function<double(double)> to_ki;
    if (kicad_version == KicadVersion::v5) {
        to_ki = [this](double dim) { return static_cast<double>(px_to_mil(dim)); };
    } else {
        to_ki = [this](double dim) { return px_to_mm(dim); };
    }
    
    for (const auto& ee_pin : ee_pins) {
        KiSymbolPin ki_pin;
        
        try {
            // 提取引脚长度
            std::string path = ee_pin.pin_path.path;
            int pin_length = 0;
            size_t h_pos = path.find_last_of('h');
            if (h_pos != std::string::npos && h_pos + 1 < path.length()) {
                pin_length = std::abs(std::stoi(path.substr(h_pos + 1)));
            }
            
            // 设置引脚属性
            ki_pin.name = ee_pin.name.text;
            ki_pin.number = ee_pin.settings.spice_pin_number;
            
            // 根据EasyEDA引脚类型设置KiCad引脚类型
            if (ee_pin.settings.type == EasyedaPinType::_input) {
                ki_pin.type = KiPinType::_input;
            } else if (ee_pin.settings.type == EasyedaPinType::output) {
                ki_pin.type = KiPinType::output;
            } else if (ee_pin.settings.type == EasyedaPinType::bidirectional) {
                ki_pin.type = KiPinType::bidirectional;
            } else if (ee_pin.settings.type == EasyedaPinType::power) {
                ki_pin.type = KiPinType::power_in;
            } else {
                ki_pin.type = KiPinType::unspecified;
            }
            
            // 设置引脚长度
            ki_pin.length = static_cast<int>(to_ki(pin_length));
            
            // 设置引脚方向
            ki_pin.orientation = ee_pin.settings.rotation;
            
            // 设置引脚位置
            double pos_x = std::stod(ee_pin.settings.pos_x);
            double pos_y = std::stod(ee_pin.settings.pos_y);
            double bbox_x = std::stod(ee_bbox.x);
            double bbox_y = std::stod(ee_bbox.y);
            
            ki_pin.pos_x = static_cast<int>(to_ki(pos_x - bbox_x));
            ki_pin.pos_y = static_cast<int>(-to_ki(pos_y - bbox_y)); // 注意Y轴翻转
            
            kicad_pins.push_back(ki_pin);
        } catch (const std::exception& e) {
            // 忽略转换失败的引脚，继续处理其他引脚
            continue;
        }
    }
    
    return kicad_pins;
}

std::vector<KiSymbolRectangle> KicadSymbolExporter::convert_ee_rectangles(const std::vector<json>& ee_rectangles,
                                                                          const EeSymbolBbox& ee_bbox) {
    auto to_ki = [this](double dim) -> int {
        return (kicad_version == KicadVersion::v5) ? px_to_mil(dim) : static_cast<int>(px_to_mm(dim));
    };

    std::vector<KiSymbolRectangle> kicad_rectangles;
    for (const auto& ee_rectangle : ee_rectangles) {
        KiSymbolRectangle ki_rectangle;
        ki_rectangle.pos_x0 = to_ki(ee_rectangle.value("pos_x", 0.0) - std::stod(ee_bbox.x));
        ki_rectangle.pos_y0 = -to_ki(ee_rectangle.value("pos_y", 0.0) - std::stod(ee_bbox.y));
        ki_rectangle.pos_x1 = to_ki(ee_rectangle.value("width", 0.0)) + ki_rectangle.pos_x0;
        ki_rectangle.pos_y1 = -to_ki(ee_rectangle.value("height", 0.0)) + ki_rectangle.pos_y0;

        kicad_rectangles.push_back(ki_rectangle);
    }

    return kicad_rectangles;
}

std::vector<KiSymbolCircle> KicadSymbolExporter::convert_ee_circles(const std::vector<json>& ee_circles,
                                                                    const EeSymbolBbox& ee_bbox) {
    auto to_ki = [this](double dim) -> int {
        return (kicad_version == KicadVersion::v5) ? px_to_mil(dim) : static_cast<int>(px_to_mm(dim));
    };

    std::vector<KiSymbolCircle> kicad_circles;
    for (const auto& ee_circle : ee_circles) {
        KiSymbolCircle ki_circle;
        ki_circle.pos_x = to_ki(ee_circle.value("center_x", 0.0) - std::stod(ee_bbox.x));
        ki_circle.pos_y = -to_ki(ee_circle.value("center_y", 0.0) - std::stod(ee_bbox.y));
        ki_circle.radius = to_ki(ee_circle.value("radius", 0.0));
        ki_circle.background_filling = ee_circle.value("fill_color", "");

        kicad_circles.push_back(ki_circle);
    }

    return kicad_circles;
}

std::vector<KiSymbolCircle> KicadSymbolExporter::convert_ee_ellipses(const std::vector<json>& ee_ellipses,
                                                                     const EeSymbolBbox& ee_bbox) {
    auto to_ki = [this](double dim) -> int {
        return (kicad_version == KicadVersion::v5) ? px_to_mil(dim) : static_cast<int>(px_to_mm(dim));
    };

    // Ellipses are not supported in Kicad -> If it's not a real ellipse, but just a circle
    std::vector<KiSymbolCircle> kicad_circles;
    for (const auto& ee_ellipse : ee_ellipses) {
        double radius_x = ee_ellipse.value("radius_x", 0.0);
        double radius_y = ee_ellipse.value("radius_y", 0.0);
        
        // 只有当椭圆实际上是圆时才转换
        if (radius_x == radius_y) {
            KiSymbolCircle ki_circle;
            ki_circle.pos_x = to_ki(ee_ellipse.value("center_x", 0.0) - std::stod(ee_bbox.x));
            ki_circle.pos_y = -to_ki(ee_ellipse.value("center_y", 0.0) - std::stod(ee_bbox.y));
            ki_circle.radius = to_ki(radius_x);
            
            kicad_circles.push_back(ki_circle);
        }
    }

    return kicad_circles;
}

std::vector<KiSymbolArc> KicadSymbolExporter::convert_ee_arcs(const std::vector<json>& ee_arcs,
                                                              const EeSymbolBbox& ee_bbox) {
    auto to_ki = [this](double dim) -> int {
        return (kicad_version == KicadVersion::v5) ? px_to_mil(dim) : static_cast<int>(px_to_mm(dim));
    };

    std::vector<KiSymbolArc> kicad_arcs;
    // 简化实现，实际应该解析SVG路径
    for (const auto& ee_arc : ee_arcs) {
        // 这里应该实现实际的弧线转换逻辑
        // 为简化起见，暂时跳过复杂解析
        std::cerr << "Arc conversion not fully implemented" << std::endl;
    }

    return kicad_arcs;
}

std::vector<KiSymbolPolygon> KicadSymbolExporter::convert_ee_polylines(const std::vector<json>& ee_polylines,
                                                                       const EeSymbolBbox& ee_bbox) {
    auto to_ki = [this](double dim) -> int {
        return (kicad_version == KicadVersion::v5) ? px_to_mil(dim) : static_cast<int>(px_to_mm(dim));
    };

    std::vector<KiSymbolPolygon> kicad_polygons;
    for (const auto& ee_polyline : ee_polylines) {
        std::string points_str = ee_polyline.value("points", "");
        // 解析点字符串
        // 这里简化处理
        
        KiSymbolPolygon kicad_polygon;
        // 简化的点处理
        kicad_polygon.points_number = 0;
        kicad_polygon.is_closed = false;
        
        kicad_polygons.push_back(kicad_polygon);
    }

    return kicad_polygons;
}

std::vector<KiSymbolPolygon> KicadSymbolExporter::convert_ee_polygons(const std::vector<json>& ee_polygons,
                                                                      const EeSymbolBbox& ee_bbox) {
    return convert_ee_polylines(ee_polygons, ee_bbox);
}

std::pair<std::vector<KiSymbolPolygon>, std::vector<KiSymbolPolygon>> 
KicadSymbolExporter::convert_ee_paths(const std::vector<json>& ee_paths,
                                      const EeSymbolBbox& ee_bbox) {
    std::vector<KiSymbolPolygon> kicad_polygons;
    std::vector<KiSymbolPolygon> kicad_beziers;
    
    // 简化实现
    return std::make_pair(kicad_polygons, kicad_beziers);
}

bool KicadSymbolExporter::exportSymbol(const json& easyeda_data, const std::string& file_path) {
    try {
        // 打开输出文件
        std::ofstream outFile(file_path);
        if (!outFile.is_open()) {
            std::cerr << "Failed to create output file: " << file_path << std::endl;
            return false;
        }
        
        // 写入KiCad符号文件头部
        outFile << "(symbol \"" << "test_symbol" << "\"\n";
        outFile << "  (in_bom yes)\n";
        outFile << "  (on_board yes)\n";
        
        // 写入文件尾部
        outFile << ")\n";
        
        // 关闭文件
        outFile.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error exporting symbol: " << e.what() << std::endl;
        return false;
    }
}

bool KicadSymbolExporter::ensureDirectoryExists(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }
}

bool KicadSymbolExporter::exportSymbolToLibrary(const json& easyeda_data, const std::string& export_path, 
                                               const std::string& library_name) {
    try {
        // 创建库目录结构
        std::string footprint_lib_dir = export_path + "/" + library_name + ".pretty";
        std::string model3d_lib_dir = export_path + "/" + library_name + ".3dshapes";
        
        // 确保目录存在
        if (!ensureDirectoryExists(export_path)) {
            std::cerr << "Failed to create export directory: " << export_path << std::endl;
            return false;
        }
        
        if (!ensureDirectoryExists(footprint_lib_dir)) {
            std::cerr << "Failed to create footprint library directory: " << footprint_lib_dir << std::endl;
            return false;
        }
        
        if (!ensureDirectoryExists(model3d_lib_dir)) {
            std::cerr << "Failed to create 3D model library directory: " << model3d_lib_dir << std::endl;
            return false;
        }
        
        // 导出符号到顶层目录中的符号文件
        std::string symbol_file_path = export_path + "/" + library_name + ".kicad_sym";
        return exportSymbol(easyeda_data, symbol_file_path);
        
    } catch (const std::exception& e) {
        std::cerr << "Error exporting symbol to library: " << e.what() << std::endl;
        return false;
    }
}