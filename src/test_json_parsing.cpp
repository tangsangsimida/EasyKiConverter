#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "easyeda/EasyedaApi.h"

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
    std::cout << "EasyKiConverter - JSON解析测试" << std::endl;
    std::cout << "=============================" << std::endl;
    
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <JSON文件路径>" << std::endl;
        std::cout << "示例: " << argv[0] << " test_data/C2040.json" << std::endl;
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
        std::cout << "JSON数据大小: " << cad_data.size() << " 个顶级元素" << std::endl;
        
        // 显示基本信息
        if (cad_data.contains("name")) {
            std::cout << "组件名称: " << cad_data["name"] << std::endl;
        } else {
            std::cout << "未找到组件名称字段" << std::endl;
        }
        
        if (cad_data.contains("lcsc")) {
            std::cout << "LCSC信息: " << cad_data["lcsc"].dump() << std::endl;
        } else {
            std::cout << "未找到LCSC信息字段" << std::endl;
        }
        
        // 检查各种数据是否存在
        std::cout << "\n数据检查:" << std::endl;
        std::cout << "符号数据: " << (cad_data.contains("symbol") ? "存在" : "不存在") << std::endl;
        std::cout << "封装数据: " << (cad_data.contains("footprint") ? "存在" : "不存在") << std::endl;
        std::cout << "3D模型数据: " << (cad_data.contains("3d_model") ? "存在" : "不存在") << std::endl;
        
        // 显示前几个键名
        std::cout << "\n顶层键名:" << std::endl;
        int count = 0;
        for (auto it = cad_data.begin(); it != cad_data.end() && count < 10; ++it, ++count) {
            std::cout << "  " << it.key() << std::endl;
        }
        if (cad_data.size() > 10) {
            std::cout << "  ... 还有 " << (cad_data.size() - 10) << " 个键" << std::endl;
        }
        
        // 检查dataStr字段
        if (cad_data.contains("dataStr")) {
            std::cout << "\n发现dataStr字段，尝试解析其内容..." << std::endl;
            
            // 确保dataStr是字符串类型
            if (cad_data["dataStr"].is_string()) {
                std::string dataStr = cad_data["dataStr"];
                std::cout << "dataStr长度: " << dataStr.length() << " 字符" << std::endl;
                
                if (!dataStr.empty()) {
                    try {
                        // 尝试解析dataStr中的JSON数据
                        json inner_data = json::parse(dataStr);
                        std::cout << "dataStr中包含JSON数据，大小: " << inner_data.size() << " 个元素" << std::endl;
                        
                        // 显示inner_data的键名
                        std::cout << "dataStr中的键名:" << std::endl;
                        int inner_count = 0;
                        for (auto it = inner_data.begin(); it != inner_data.end() && inner_count < 10; ++it, ++inner_count) {
                            std::cout << "  " << it.key() << std::endl;
                        }
                        if (inner_data.size() > 10) {
                            std::cout << "  ... 还有 " << (inner_data.size() - 10) << " 个键" << std::endl;
                        }
                        
                        // 检查是否有我们需要的数据类型
                        std::cout << "\n在dataStr中检查:" << std::endl;
                        std::cout << "  符号数据: " << (inner_data.contains("symbol") ? "存在" : "不存在") << std::endl;
                        std::cout << "  封装数据: " << (inner_data.contains("footprint") ? "存在" : "不存在") << std::endl;
                        std::cout << "  3D模型数据: " << (inner_data.contains("3d_model") ? "存在" : "不存在") << std::endl;
                    } catch (const json::exception& e) {
                        std::cout << "dataStr不是有效的JSON格式: " << e.what() << std::endl;
                    }
                }
            } else {
                std::cout << "dataStr不是字符串类型" << std::endl;
            }
        } else {
            std::cout << "\n未找到dataStr字段" << std::endl;
        }
        
        // 测试EasyedaApi类的解析功能
        std::cout << "\n测试EasyedaApi类的解析功能:" << std::endl;
        EasyedaApi api;
        // 这里只是测试类是否能正常初始化，实际网络请求不会执行
        
        std::cout << "\n测试完成!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}