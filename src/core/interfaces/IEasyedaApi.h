#ifndef IEASYEDAAPI_H
#define IEASYEDAAPI_H

#include <QObject>
#include <QString>
#include <QJsonObject>

namespace EasyKiConverter {

/**
 * @brief EasyEDA API 接口类
 *
 * 定义了与 EasyEDA 服务器通信的标准接口
 * 用于依赖注入和单元测试
 */
class IEasyedaApi : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit IEasyedaApi(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief 析构函数
     */
    virtual ~IEasyedaApi() = default;

    /**
     * @brief 获取组件信息
     *
     * @param lcscId LCSC 组件 ID（应以 'C' 开头）
     */
    virtual void fetchComponentInfo(const QString &lcscId) = 0;

    /**
     * @brief 获取组件的 CAD 数据
     *
     * @param lcscId LCSC 组件 ID
     */
    virtual void fetchCadData(const QString &lcscId) = 0;

    /**
     * @brief 获取 3D 模型数据（OBJ 格式）
     *
     * @param uuid 3D 模型的 UUID
     */
    virtual void fetch3DModelObj(const QString &uuid) = 0;

    /**
     * @brief 获取 3D 模型数据（STEP 格式）
     *
     * @param uuid 3D 模型的 UUID
     */
    virtual void fetch3DModelStep(const QString &uuid) = 0;

    /**
     * @brief 取消当前请求
     */
    virtual void cancelRequest() = 0;

signals:
    /**
     * @brief 组件信息获取成功信号
     *
     * @param data 组件数据
     */
    void componentInfoFetched(const QJsonObject &data);

    /**
     * @brief CAD 数据获取成功信号
     *
     * @param data CAD 数据
     */
    void cadDataFetched(const QJsonObject &data);

    /**
     * @brief 3D 模型数据获取成功信号
     *
     * @param uuid 3D 模型的 UUID
     * @param data 3D 模型数据
     */
    void model3DFetched(const QString &uuid, const QByteArray &data);

    /**
     * @brief 请求失败信号
     *
     * @param errorMessage 错误消息
     */
    void fetchError(const QString &errorMessage);
};

} // namespace EasyKiConverter

#endif // IEASYEDAAPI_H