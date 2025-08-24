#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "easyeda/EasyedaApi.h"
#include "kicad/KicadSymbolExporter.h"

using json = nlohmann::json;

// 从文件读取JSON数据
json read_json_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return json({});
    }
    
    try {
        json data;
        file >> data;
        return data;
    } catch (const std::exception& e) {
        std::cerr << "解析JSON文件时出错: " << e.what() << std::endl;
        return json({});
    }
}

int main(int argc, char* argv[]) {
    std::cout << "EasyKiConverter - 本地数据测试" << std::endl;
    std::cout << "=============================" << std::endl;
    
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <JSON文件路径>" << std::endl;
        std::cout << "示例: " << argv[0] << " test_data/C12345.json" << std::endl;
        return 1;
    }
    
    std::string json_file = argv[1];
    std::cout << "读取本地JSON文件: " << json_file << std::endl;
    
    try {
        // 从文件读取数据
        json cad_data = read_json_from_file(json_file);
        
        if (cad_data.empty()) {
            std::cerr << "读取或解析JSON文件失败!" << std::endl;
            return 1;
        }
        
        std::cout << "JSON数据读取成功!" << std::endl;
        
        // 显示基本信息
        if (cad_data.contains("name")) {
            std::cout << "组件名称: " << cad_data["name"] << std::endl;
        }
        
        if (cad_data.contains("lcsc")) {
            std::cout << "LCSC信息: " << cad_data["lcsc"].dump() << std::endl;
        }
        
        // 检查各种数据是否存在
        std::cout << "\n数据检查:" << std::endl;
        std::cout << "符号数据: " << (cad_data.contains("symbol") ? "存在" : "不存在") << std::endl;
        std::cout << "封装数据: " << (cad_data.contains("footprint") ? "存在" : "不存在") << std::endl;
        std::cout << "3D模型数据: " << (cad_data.contains("3d_model") ? "存在" : "不存在") << std::endl;
        
        // 初始化导出器
        KicadSymbolExporter exporter(KicadVersion::v6);
        
        std::cout << "\n测试完成!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}