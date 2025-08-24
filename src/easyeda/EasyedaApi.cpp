#include "EasyedaApi.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <zlib.h>

// 添加gzip解压缩函数声明
std::string gzip_decompress(const std::string& data);

const std::string EasyedaApi::API_ENDPOINT = "https://easyeda.com/api/products/{lcsc_id}/components?version=6.4.19.5";
const std::string EasyedaApi::ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/{uuid}";
const std::string EasyedaApi::ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/{uuid}";

// 回调函数用于处理HTTP响应数据
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

// gzip解压缩函数实现
std::string gzip_decompress(const std::string& data) {
    if (data.empty()) {
        return data;
    }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
        std::cerr << "inflateInit2 failed" << std::endl;
        return "";
    }

    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();

    int ret;
    char outbuffer[32768];
    std::string outstring;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, Z_SYNC_FLUSH);

        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        std::cerr << "Exception during zlib decompression: (" << ret << ") " << zs.msg << std::endl;
        return "";
    }

    return outstring;
}

EasyedaApi::EasyedaApi() {
    headers["Accept-Encoding"] = "gzip, deflate";
    headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
    headers["User-Agent"] = "easyeda2kicad v1.0.0";
}

json EasyedaApi::get_info_from_easyeda_api(const std::string& lcsc_id) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        // 构造URL
        std::string url = API_ENDPOINT;
        size_t pos = url.find("{lcsc_id}");
        if (pos != std::string::npos) {
            url.replace(pos, 9, lcsc_id);
        }

        // 设置请求选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // 跟随重定向
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 设置超时时间
        curl_easy_setopt(curl, CURLOPT_USERAGENT, headers["User-Agent"].c_str());

        // 设置请求头
        struct curl_slist* curl_headers = nullptr;
        for (const auto& header : headers) {
            std::string header_str = header.first + ": " + header.second;
            curl_headers = curl_slist_append(curl_headers, header_str.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

        // 执行请求
        res = curl_easy_perform(curl);

        // 清理
        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            return json({});
        }

        // 检查响应是否为空
        if (response.empty()) {
            std::cerr << "Empty response from EasyEDA API" << std::endl;
            return json({});
        }

        // 检查响应是否是gzip压缩的（以0x1F 0x8B开头）
        std::string decompressed_response = response;
        if (response.size() >= 2 && static_cast<unsigned char>(response[0]) == 0x1F 
            && static_cast<unsigned char>(response[1]) == 0x8B) {
            // 解压缩gzip数据
            decompressed_response = gzip_decompress(response);
            if (decompressed_response.empty()) {
                std::cerr << "Failed to decompress gzip data" << std::endl;
                return json({});
            }
        }

        // 检查响应是否是有效的JSON
        // 如果响应以'<'开头，可能是HTML错误页面
        if (!decompressed_response.empty() && decompressed_response[0] == '<') {
            std::cerr << "Received HTML instead of JSON. Response starts with: " 
                      << decompressed_response.substr(0, std::min(100, (int)decompressed_response.length())) << std::endl;
            return json({});
        }

        // 解析JSON响应
        try {
            json api_response = json::parse(decompressed_response);
            
            if (api_response.empty() || 
                (api_response.contains("code") && api_response.value("success", true) == false)) {
                std::cerr << "API response indicates failure" << std::endl;
                return json({});
            }

            return api_response;
        } catch (const json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            std::cerr << "Response (first 500 chars): " << decompressed_response.substr(0, std::min(500, (int)decompressed_response.length())) << std::endl;
            return json({});
        }
    }

    return json({});
}

json EasyedaApi::get_cad_data_of_component(const std::string& lcsc_id) {
    json cp_cad_info = get_info_from_easyeda_api(lcsc_id);
    if (cp_cad_info.empty()) {
        return json({});
    }
    
    if (cp_cad_info.contains("result")) {
        return cp_cad_info["result"];
    }
    
    return json({});
}

std::string EasyedaApi::get_raw_3d_model_obj(const std::string& uuid) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        // 构造URL
        std::string url = ENDPOINT_3D_MODEL;
        size_t pos = url.find("{uuid}");
        if (pos != std::string::npos) {
            url.replace(pos, 6, uuid);
        }

        // 设置请求选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, headers["User-Agent"].c_str());

        // 设置User-Agent请求头
        struct curl_slist* curl_headers = nullptr;
        std::string user_agent = "User-Agent: " + headers["User-Agent"];
        curl_headers = curl_slist_append(curl_headers, user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

        // 执行请求
        res = curl_easy_perform(curl);

        // 清理
        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "Failed to get raw 3D model data for uuid:" << uuid 
                      << " on easyeda: " << curl_easy_strerror(res) << std::endl;
            return "";
        }

        if (response.empty()) {
            std::cerr << "No raw 3D model data found for uuid:" << uuid << " on easyeda" << std::endl;
            return "";
        }

        return response;
    }

    return "";
}

std::vector<unsigned char> EasyedaApi::get_step_3d_model(const std::string& uuid) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        // 构造URL
        std::string url = ENDPOINT_3D_MODEL_STEP;
        size_t pos = url.find("{uuid}");
        if (pos != std::string::npos) {
            url.replace(pos, 6, uuid);
        }

        // 设置请求选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, headers["User-Agent"].c_str());

        // 设置User-Agent请求头
        struct curl_slist* curl_headers = nullptr;
        std::string user_agent = "User-Agent: " + headers["User-Agent"];
        curl_headers = curl_slist_append(curl_headers, user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

        // 执行请求
        res = curl_easy_perform(curl);

        // 清理
        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "Failed to get step 3D model data for uuid:" << uuid 
                      << " on easyeda: " << curl_easy_strerror(res) << std::endl;
            return std::vector<unsigned char>();
        }

        if (response.empty()) {
            std::cerr << "No step 3D model data found for uuid:" << uuid << " on easyeda" << std::endl;
            return std::vector<unsigned char>();
        }

        // 将字符串转换为字节向量
        return std::vector<unsigned char>(response.begin(), response.end());
    }

    return std::vector<unsigned char>();
}