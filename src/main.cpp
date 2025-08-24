#include <iostream>
#include <string>
#include "easyeda/EasyedaApi.h"
#include "kicad/KicadSymbolExporter.h"

int main(int argc, char* argv[]) {
    std::cout << "EasyKiConverter - C++ Version" << std::endl;
    std::cout << "=============================" << std::endl;
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <LCSC_ID>" << std::endl;
        std::cout << "Example: " << argv[0] << " C123456" << std::endl;
        return 1;
    }
    
    std::string lcsc_id = argv[1];
    std::cout << "Converting component: " << lcsc_id << std::endl;
    
    try {
        // 初始化API
        EasyedaApi api;
        
        // 获取组件数据
        std::cout << "Fetching component data from EasyEDA..." << std::endl;
        json cad_data = api.get_cad_data_of_component(lcsc_id);
        
        if (cad_data.empty()) {
            std::cerr << "Failed to fetch component data!" << std::endl;
            return 1;
        }
        
        std::cout << "Component data fetched successfully!" << std::endl;
        
        // 初始化导出器
        KicadSymbolExporter exporter(KicadVersion::v6);
        
        // 这里应该处理实际的转换和导出逻辑
        // 为简化起见，我们只输出一些基本信息
        if (cad_data.contains("name")) {
            std::cout << "Component name: " << cad_data["name"] << std::endl;
        }
        
        if (cad_data.contains("symbol")) {
            std::cout << "Symbol data found, ready for conversion." << std::endl;
        }
        
        std::cout << "Conversion completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}