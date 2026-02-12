#ifndef MODEL3DDATA_H
#define MODEL3DDATA_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

// ==================== 3D 模型基础坐标 ====================

struct Model3DBase {
    double x;
    double y;
    double z;

    Model3DBase() : x(0.0), y(0.0), z(0.0) {}
    Model3DBase(double x, double y, double z) : x(x), y(y), z(z) {}

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

// ==================== 3D 模型 ====================

class Model3DData {
public:
    Model3DData();
    ~Model3DData() = default;

    // Getter 和 Setter 方法
    QString name() const {
        return m_name;
    }
    void setName(const QString& name) {
        m_name = name;
    }

    QString uuid() const {
        return m_uuid;
    }
    void setUuid(const QString& uuid) {
        m_uuid = uuid;
    }

    Model3DBase translation() const {
        return m_translation;
    }
    void setTranslation(const Model3DBase& translation) {
        m_translation = translation;
    }

    Model3DBase rotation() const {
        return m_rotation;
    }
    void setRotation(const Model3DBase& rotation) {
        m_rotation = rotation;
    }

    QString rawObj() const {
        return m_rawObj;
    }
    void setRawObj(const QString& rawObj) {
        m_rawObj = rawObj;
    }

    QByteArray step() const {
        return m_step;
    }
    void setStep(const QByteArray& step) {
        m_step = step;
    }

    // JSON 序列
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 数据验证
    bool isValid() const;
    QString validate() const;

    // 清空数据
    void clear();

private:
    QString m_name;
    QString m_uuid;
    Model3DBase m_translation;
    Model3DBase m_rotation;
    QString m_rawObj;
    QByteArray m_step;
};

}  // namespace EasyKiConverter

#endif  // MODEL3DDATA_H
