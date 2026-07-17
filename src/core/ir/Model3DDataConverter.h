#pragma once

/**
 * @file Model3DDataConverter.h
 * @brief Model3DData -> Model3DIR 转换桥接
 *
 * 提供从旧模型 Model3DData 到 IR Model3DIR 的转换函数。
 * 3D 模型数据基本是 EDA 无关的，转换主要是字段名映射。
 */

#include "Model3DIR.h"
#include "models/Model3DData.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief Model3DData -> Model3DIR 转换
 * @param data 旧模型的 3D 模型数据
 * @return IR 的 3D 模型数据
 */
inline Model3DIR toModel3DIR(const Model3DData& data) {
    Model3DIR ir;

    ir.setName(data.name());

    const auto& t = data.translation();
    ir.setTranslation(Model3DVec3(t.x, t.y, t.z));

    const auto& r = data.rotation();
    ir.setRotation(Model3DVec3(r.x, r.y, r.z));

    const auto& o = data.stepOffsetMm();
    ir.setStepOffsetMm(Model3DVec3(o.x, o.y, o.z));

    ir.setRawObj(data.rawObj());
    ir.setStepData(data.step());

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
