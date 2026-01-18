#ifndef IEXPORTER3DMODEL_H
#define IEXPORTER3DMODEL_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include "src/models/Model3DData.h"

namespace EasyKiConverter {

/**
 * @brief 3D 模型导出器接口类
 *
 * 定义了 3D 模型导出的标准接口
 * 用于依赖注入和单元测试
 */
class IExporter3DModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit IExporter3DModel(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief 析构函数
     */
    virtual ~IExporter3DModel() = default;

    /**
     * @brief 导出 3D 模型
     *
     * @param model 3D 模型数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    virtual bool export3DModel(const Model3DData &model, const QString &filePath) = 0;

    /**
     * @brief 导出 3D 模型为 WRL 格式
     *
     * @param model 3D 模型数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    virtual bool exportWrlModel(const Model3DData &model, const QString &filePath) = 0;

    /**
     * @brief 导出 3D 模型为 STEP 格式
     *
     * @param model 3D 模型数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    virtual bool exportStepModel(const Model3DData &model, const QString &filePath) = 0;

    /**
     * @brief 转换 3D 模型格式
     *
     * @param inputData 输入数据
     * @param inputFormat 输入格式（"obj", "wrl", "step"）
     * @param outputFormat 输出格式（"wrl", "step"）
     * @return QByteArray 转换后的数据
     */
    virtual QByteArray convertModelFormat(const QByteArray &inputData, const QString &inputFormat, const QString &outputFormat) = 0;
};

} // namespace EasyKiConverter

#endif // IEXPORTER3DMODEL_H