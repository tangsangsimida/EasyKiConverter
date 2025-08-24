#ifndef EASYKI_CONVERTER_EASYEDA_IMPORTER_H
#define EASYKI_CONVERTER_EASYEDA_IMPORTER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../kicad/KicadSymbolExporter.h"

using json = nlohmann::json;

class EasyedaSymbolImporter {
public:
    explicit EasyedaSymbolImporter(const json& easyeda_cp_cad_data);
    EeSymbol get_symbol() const;

private:
    json input_data;
    EeSymbol output_symbol;

    EeSymbol extract_easyeda_data(const json& ee_data, const json& ee_data_info);
    void add_easyeda_pin(const std::string& pin_data, EeSymbol& ee_symbol);
    void add_easyeda_rectangle(const std::string& rectangle_data, EeSymbol& ee_symbol);
    void add_easyeda_polyline(const std::string& polyline_data, EeSymbol& ee_symbol);
    void add_easyeda_polygon(const std::string& polygon_data, EeSymbol& ee_symbol);
    void add_easyeda_path(const std::string& path_data, EeSymbol& ee_symbol);
    void add_easyeda_circle(const std::string& circle_data, EeSymbol& ee_symbol);
    void add_easyeda_ellipse(const std::string& ellipse_data, EeSymbol& ee_symbol);
    void add_easyeda_arc(const std::string& arc_data, EeSymbol& ee_symbol);
    
    // Helper functions to split strings
    std::vector<std::string> split_string(const std::string& str, char delimiter);
    std::vector<std::vector<std::string>> split_segments(const std::string& str);
};

#endif // EASYKI_CONVERTER_EASYEDA_IMPORTER_H