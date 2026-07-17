#pragma once

/**
 * @file Model3DIR.h
 * @brief 通用 3D 模型中间表示
 *
 * 从 Model3DData 迁移，移除 EasyEDA 特有依赖。
 * 3D 模型数据本身已经是 EDA 无关的（STEP/OBJ 是通用格式）。
 */

#include <QByteArray>
#include <QString>

namespace EasyKiConverter {
namespace IR {

/**
 * @brief 三维向量（用于位移、旋转等）
 */
struct Model3DVec3 {
    double x = 0.0;  ///< X 分量
    double y = 0.0;  ///< Y 分量
    double z = 0.0;  ///< Z 分量

    /** @brief 默认构造（零向量） */
    Model3DVec3() = default;

    /** @brief 参数化构造 */
    Model3DVec3(double x, double y, double z) : x(x), y(y), z(z) {}

    bool operator==(const Model3DVec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Model3DVec3& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 通用 3D 模型数据
 *
 * 包含 3D 模型的几何数据（STEP/OBJ）和放置参数（位移、旋转）。
 * 不包含任何 EDA 特有的标识符或格式信息。
 */
class Model3DIR {
public:
    Model3DIR() = default;

    /** @brief 模型名称 */
    QString name() const {
        return m_name;
    }

    void setName(const QString& name) {
        m_name = name;
    }

    /** @brief 模型位移（mm） */
    Model3DVec3 translation() const {
        return m_translation;
    }

    void setTranslation(const Model3DVec3& t) {
        m_translation = t;
    }

    /** @brief 模型旋转角度（度） */
    Model3DVec3 rotation() const {
        return m_rotation;
    }

    void setRotation(const Model3DVec3& r) {
        m_rotation = r;
    }

    /** @brief STEP 文件偏移量（mm） */
    Model3DVec3 stepOffsetMm() const {
        return m_stepOffsetMm;
    }

    void setStepOffsetMm(const Model3DVec3& offset) {
        m_stepOffsetMm = offset;
    }

    /** @brief 原始 OBJ 数据 */
    QString rawObj() const {
        return m_rawObj;
    }

    void setRawObj(const QString& obj) {
        m_rawObj = obj;
    }

    /** @brief STEP 文件数据 */
    QByteArray stepData() const {
        return m_step;
    }

    void setStepData(const QByteArray& data) {
        m_step = data;
    }

    /** @brief 是否包含有效的 STEP 数据 */
    bool hasStepData() const {
        return !m_step.isEmpty();
    }

    /** @brief 是否包含有效的 OBJ 数据 */
    bool hasObjData() const {
        return !m_rawObj.isEmpty();
    }

    /** @brief 是否包含任何有效的 3D 数据 */
    bool isValid() const {
        return hasStepData() || hasObjData();
    }

    void clear() {
        m_name.clear();
        m_translation = {};
        m_rotation = {};
        m_stepOffsetMm = {};
        m_rawObj.clear();
        m_step.clear();
    }

private:
    QString m_name;
    Model3DVec3 m_translation;
    Model3DVec3 m_rotation;
    Model3DVec3 m_stepOffsetMm;
    QString m_rawObj;
    QByteArray m_step;
};

}  // namespace IR
}  // namespace EasyKiConverter
