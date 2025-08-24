#ifndef EASYKI_CONVERTER_EASYEDA_API_H
#define EASYKI_CONVERTER_EASYEDA_API_H

#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief EasyEDA API接口类，用于与EasyEDA服务器通信获取组件数据
 * EasyEDA API interface class for communicating with EasyEDA server to fetch component data
 */
class EasyedaApi {
private:
    std::map<std::string, std::string> headers;
    static const std::string API_ENDPOINT;
    static const std::string ENDPOINT_3D_MODEL;
    static const std::string ENDPOINT_3D_MODEL_STEP;

public:
    /**
     * @brief 初始化API客户端，设置请求头信息
     * Initialize API client and setup request headers
     */
    EasyedaApi();

    /**
     * @brief 从EasyEDA API获取指定LCSC ID的组件信息
     * Fetch component information from EasyEDA API for specified LCSC ID
     * 
     * @param lcsc_id LCSC组件ID，应以'C'开头 / LCSC component ID, should start with 'C'
     * @return API响应数据，失败时返回空对象 / API response data, empty object on failure
     */
    json get_info_from_easyeda_api(const std::string& lcsc_id);

    /**
     * @brief 获取指定LCSC ID的组件CAD数据（包含符号、封装、3D模型等信息）
     * Fetch CAD data for specified LCSC ID (includes symbol, footprint, 3D model info)
     * 
     * @param lcsc_id LCSC组件ID / LCSC component ID
     * @return 组件的完整CAD数据 / Complete CAD data of the component
     */
    json get_cad_data_of_component(const std::string& lcsc_id);

    /**
     * @brief 获取原始3D模型数据（OBJ格式）
     * Fetch raw 3D model data (OBJ format)
     * 
     * @param uuid 3D模型的UUID标识符 / UUID identifier for 3D model
     * @return 3D模型OBJ文件内容，失败返回空字符串 / 3D model OBJ file content, empty string on failure
     */
    std::string get_raw_3d_model_obj(const std::string& uuid);

    /**
     * @brief 获取STEP格式的3D模型数据
     * Fetch 3D model data in STEP format
     * 
     * @param uuid 3D模型的UUID标识符 / UUID identifier for 3D model
     * @return STEP格式的3D模型二进制数据，失败返回空 / 3D model binary data in STEP format, empty on failure
     */
    std::vector<unsigned char> get_step_3d_model(const std::string& uuid);
};

#endif // EASYKI_CONVERTER_EASYEDA_API_H