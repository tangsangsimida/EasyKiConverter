#pragma once

#include "core/ir/Model3DIR.h"

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 3D 模型导出器通用接口
 * @details 定义将 IR 3D 模型数据导出为目标 EDA 3D 模型格式的通用接口。
 *          WRL 和 STEP 是通用格式，KiCad 和 Altium 均支持。
 */
class IModel3DExporter {
public:
    virtual ~IModel3DExporter() = default;

    /**
     * @brief 导出模型为 WRL (VRML) 格式
     * @param model 3D 模型 IR 数据
     * @param savePath 保存路径
     * @return 是否成功
     */
    virtual bool exportToWrl(const IR::Model3DIR& model, const QString& savePath) = 0;

    /**
     * @brief 导出模型为 STEP 格式
     * @param model 3D 模型 IR 数据
     * @param savePath 保存路径
     * @return 是否成功
     */
    virtual bool exportToStep(const IR::Model3DIR& model, const QString& savePath) = 0;
};

}  // namespace EasyKiConverter
